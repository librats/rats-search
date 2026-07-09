#ifndef FEEDWIDGET_H
#define FEEDWIDGET_H

#include "torrenttablewidget.h"

class QLabel;

/**
 * @brief FeedWidget - feed of voted/popular torrents.
 *
 * Thin subclass of TorrentTableWidget: loads via app_->feed()->getFeed() and
 * refreshes on the FeedService feedUpdated signal. Auto-loads more on scroll.
 */
class FeedWidget : public TorrentTableWidget {
    Q_OBJECT

public:
    explicit FeedWidget(QWidget* parent = nullptr);

protected:
    void refresh() override;
    void onApplicationSet() override;

private slots:
    void onScrollValueChanged(int value);
    void loadMore();

private:
    void setupControls();
    void updateEmptyState();

    static constexpr int kPageSize = 20;

    QLabel* statusLabel_ = nullptr;
    QLabel* emptyLabel_ = nullptr;

    QVector<rats::domain::Torrent> torrents_;
    int loadedCount_ = 0;
    bool loading_ = false;
};

#endif // FEEDWIDGET_H
