#ifndef TORRENTITEMDELEGATE_H
#define TORRENTITEMDELEGATE_H

#include <QStyledItemDelegate>
#include <QPainter>
#include <QApplication>

/**
 * @brief Custom delegate for rich torrent item display
 * Displays torrent with type icons, colored seeders/leechers, progress bars
 */
class TorrentItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit TorrentItemDelegate(QObject *parent = nullptr);
    
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

    // Content type icons
    static QIcon getContentTypeIcon(const QString &contentType);
    static QColor getContentTypeColor(const QString &contentType);
    static QString getContentTypeName(const QString &contentType);
    
    // Seeders/Leechers colors
    static QColor getSeedersColor(int seeders);
    static QColor getLeechersColor(int leechers);
    
private:
    void paintTorrentRow(QPainter *painter, const QStyleOptionViewItem &option,
                         const QModelIndex &index) const;
    void drawContentTypeIcon(QPainter *painter, const QRect &rect, 
                             const QString &contentType) const;
    void drawRatingBar(QPainter *painter, const QRect &rect, 
                       int good, int bad) const;
};

#endif // TORRENTITEMDELEGATE_H

