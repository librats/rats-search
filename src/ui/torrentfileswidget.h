#ifndef TORRENTFILESWIDGET_H
#define TORRENTFILESWIDGET_H

#include <QHeaderView>
#include <QLabel>
#include <QMap>
#include <QSet>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

#include "domain/torrent.h"

namespace rats::app {
class Application;
}

/**
 * @brief Widget for displaying torrent file list in a tree view
 *
 * This widget displays the hierarchical file structure of a torrent.
 * It is designed to be placed at the bottom of the main window (like
 * qBittorrent).
 *
 * Features:
 * - Hierarchical file tree with folders and files
 * - File size display
 * - Checkbox for file selection (for partial downloads)
 * - File type icons based on extension
 */
class TorrentFilesWidget : public QWidget {
    Q_OBJECT

public:
    explicit TorrentFilesWidget(QWidget* parent = nullptr);
    ~TorrentFilesWidget();

    /**
     * @brief Provide access to the running application (for on-demand file
     * fetch).
     */
    void setApplication(rats::app::Application* app) { app_ = app; }

    /**
     * @brief Set files for the given torrent
     * @param hash Torrent info hash
     * @param name Torrent name (for display)
     * @param files File list (path + size)
     */
    void setFiles(const QString& hash, const QString& name, const QVector<rats::domain::File>& files);

    /**
     * @brief Display a torrent's files. Uses the torrent's embedded file list
     * when present, otherwise fetches it from the repository via the application.
     */
    void setTorrent(const rats::domain::Torrent& torrent);

    /**
     * @brief Clear the file tree
     */
    void clear();

    /**
     * @brief Check if widget has any files loaded
     */
    bool isEmpty() const { return currentHash_.isEmpty(); }

    /**
     * @brief Get current torrent hash
     */
    QString currentHash() const { return currentHash_; }

private slots:
    void onFileItemChanged(QTreeWidgetItem* item, int column);

private:
    void setupUi();

    // File tree building helpers
    struct FileTreeNode {
        QString name;
        qint64 size = 0;
        bool isFile = false;
        int fileIndex = -1;
        QMap<QString, FileTreeNode*> children;
        ~FileTreeNode() { qDeleteAll(children); }
    };

    FileTreeNode* buildFileTree(const QVector<rats::domain::File>& files);
    void addTreeNodeToWidget(FileTreeNode* node, QTreeWidgetItem* parent);
    QString getFileTypeIcon(const QString& filename) const;
    void collectSelectedFiles(QTreeWidgetItem* item, QList<int>& indices) const;

    // Indices of the files currently checked in the tree.
    QList<int> getSelectedFileIndices() const;

    rats::app::Application* app_ = nullptr;

    // UI elements
    QLabel* titleLabel_;
    QLabel* infoLabel_;
    QTreeWidget* filesTree_;

    // Current state
    QString currentHash_;
    QString currentName_;
    int fileCount_ = 0;
};

#endif // TORRENTFILESWIDGET_H
