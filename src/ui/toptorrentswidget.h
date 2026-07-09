#ifndef TOPTORRENTSWIDGET_H
#define TOPTORRENTSWIDGET_H

#include "torrenttablewidget.h"

#include <QHash>
#include <QStringList>

class QTabBar;
class QComboBox;
class QLabel;
class QPushButton;

/**
 * @brief TopTorrentsWidget - top torrents by content type and time period.
 *
 * Thin subclass of TorrentTableWidget: keeps its category tabs + time filter
 * controls and loads data via app_->search()->top(type, time, offset, limit).
 */
class TopTorrentsWidget : public TorrentTableWidget {
    Q_OBJECT

public:
    explicit TopTorrentsWidget(QWidget* parent = nullptr);

protected:
    void refresh() override;

private slots:
    void onCategoryChanged(int index);
    void onTimeFilterChanged(int index);
    void onMoreClicked();

private:
    void setupControls();
    void loadMore();
    QString currentType() const; // empty string for "main" (= all)

    static constexpr int kPageSize = 20;

    QTabBar* categoryTabs_ = nullptr;
    QComboBox* timeFilter_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QPushButton* moreButton_ = nullptr;

    QStringList categories_;
    QHash<QString, QString> categoryLabels_;
    QStringList timeFilters_;
    QHash<QString, QString> timeLabels_;

    QString currentCategory_ = QStringLiteral("main");
    QString currentTime_ = QStringLiteral("overall");
    int loadedCount_ = 0;
    QVector<rats::domain::Torrent> torrents_;
};

#endif // TOPTORRENTSWIDGET_H
