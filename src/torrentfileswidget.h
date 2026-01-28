#ifndef TORRENTFILESWIDGET_H
#define TORRENTFILESWIDGET_H

#include <QWidget>
#include <QTreeWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QSet>

/**
 * @brief Widget for displaying torrent file list in a tree view
 * 
 * This widget displays the hierarchical file structure of a torrent.
 * It is designed to be placed at the bottom of the main window (like qBittorrent).
 * 
 * Features:
 * - Hierarchical file tree with folders and files
 * - File size display
 * - Checkbox for file selection (for partial downloads)
 * - File type icons based on extension
 */
class TorrentFilesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TorrentFilesWidget(QWidget *parent = nullptr);
    ~TorrentFilesWidget();
    
    /**
     * @brief Set files for the given torrent
     * @param hash Torrent info hash
     * @param name Torrent name (for display)
     * @param files JSON array of files with path and size
     */
    void setFiles(const QString& hash, const QString& name, const QJsonArray& files);
    
    /**
     * @brief Clear the file tree
     */
    void clear();
    
    /**
     * @brief Get indices of selected files (for partial download)
     * @return List of file indices that are checked
     */
    QList<int> getSelectedFileIndices() const;
    
    /**
     * @brief Check if widget has any files loaded
     */
    bool isEmpty() const { return currentHash_.isEmpty(); }
    
    /**
     * @brief Get current torrent hash
     */
    QString currentHash() const { return currentHash_; }

signals:
    /**
     * @brief Emitted when file selection changes
     * @param hash Torrent info hash
     * @param selectedIndices List of selected file indices
     */
    void fileSelectionChanged(const QString& hash, const QList<int>& selectedIndices);

private slots:
    void onFileItemChanged(QTreeWidgetItem* item, int column);

private:
    void setupUi();
    QString formatBytes(qint64 bytes) const;
    
    // File tree building helpers
    struct FileTreeNode {
        QString name;
        qint64 size = 0;
        bool isFile = false;
        int fileIndex = -1;
        QMap<QString, FileTreeNode*> children;
        ~FileTreeNode() { qDeleteAll(children); }
    };
    
    FileTreeNode* buildFileTree(const QJsonArray& files);
    void addTreeNodeToWidget(FileTreeNode* node, QTreeWidgetItem* parent);
    QString getFileTypeIcon(const QString& filename) const;
    void collectSelectedFiles(QTreeWidgetItem* item, QList<int>& indices) const;
    
    // UI elements
    QLabel* titleLabel_;
    QLabel* infoLabel_;
    QTreeWidget* filesTree_;
    
    // Current state
    QString currentHash_;
    QString currentName_;
};

#endif // TORRENTFILESWIDGET_H
