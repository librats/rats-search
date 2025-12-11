#include "torrentitemdelegate.h"
#include "searchresultmodel.h"
#include <QPainter>
#include <QApplication>
#include <QPainterPath>

TorrentItemDelegate::TorrentItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QColor TorrentItemDelegate::getContentTypeColor(const QString &contentType)
{
    static const QMap<QString, QColor> colors = {
        {"video", QColor("#1ee359")},
        {"audio", QColor("#1e94e3")},
        {"pictures", QColor("#e31ebc")},
        {"books", QColor("#e3d91e")},
        {"application", QColor("#e3561e")},
        {"archive", QColor("#1e25e3")},
        {"disc", QColor("#1ee381")},
        {"unknown", QColor("#888888")}
    };
    return colors.value(contentType.toLower(), QColor("#888888"));
}

QString TorrentItemDelegate::getContentTypeName(const QString &contentType)
{
    static const QMap<QString, QString> names = {
        {"video", "Video"},
        {"audio", "Audio"},
        {"pictures", "Pictures"},
        {"books", "Books"},
        {"application", "Application"},
        {"archive", "Archive"},
        {"disc", "Disc Image"},
        {"unknown", "Unknown"}
    };
    return names.value(contentType.toLower(), "Unknown");
}

QColor TorrentItemDelegate::getSeedersColor(int seeders)
{
    if (seeders > 50) return QColor("#00C853");      // Green
    if (seeders > 10) return QColor("#64DD17");      // Light Green
    if (seeders > 0) return QColor("#FFD600");       // Yellow
    return QColor("#888888");                         // Gray
}

QColor TorrentItemDelegate::getLeechersColor(int leechers)
{
    if (leechers > 50) return QColor("#AA00FF");     // Purple
    if (leechers > 10) return QColor("#D500F9");     // Light Purple
    if (leechers > 0) return QColor("#E040FB");      // Pink
    return QColor("#888888");                         // Gray
}

void TorrentItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const
{
    if (!index.isValid()) {
        return;
    }
    
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);
    
    // Background
    QColor bgColor;
    if (option.state & QStyle::State_Selected) {
        bgColor = QColor("#3d6a99");
    } else if (option.state & QStyle::State_MouseOver) {
        bgColor = QColor("#3c4048");
    } else if (index.row() % 2 == 0) {
        bgColor = QColor("#2b2b2b");
    } else {
        bgColor = QColor("#323232");
    }
    painter->fillRect(option.rect, bgColor);
    
    // Get column
    int column = index.column();
    QRect rect = option.rect.adjusted(4, 2, -4, -2);
    
    switch (column) {
        case SearchResultModel::NameColumn: {
            // Draw content type icon (small colored square)
            QString contentType = index.data(Qt::UserRole + 1).toString();
            if (!contentType.isEmpty()) {
                QColor typeColor = getContentTypeColor(contentType);
                QRect iconRect(rect.left(), rect.top() + (rect.height() - 12) / 2, 12, 12);
                painter->setBrush(typeColor);
                painter->setPen(Qt::NoPen);
                painter->drawRoundedRect(iconRect, 2, 2);
                rect.setLeft(rect.left() + 18);
            }
            
            // Draw name
            painter->setPen(QColor("#ffffff"));
            QFont font = option.font;
            font.setPointSize(10);
            painter->setFont(font);
            QString name = index.data(Qt::DisplayRole).toString();
            painter->drawText(rect, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine, 
                             option.fontMetrics.elidedText(name, Qt::ElideRight, rect.width()));
            break;
        }
        
        case SearchResultModel::SizeColumn: {
            painter->setPen(QColor("#aaaaaa"));
            QString size = index.data(Qt::DisplayRole).toString();
            painter->drawText(rect, Qt::AlignVCenter | Qt::AlignRight, size);
            break;
        }
        
        case SearchResultModel::SeedersColumn: {
            int seeders = index.data(Qt::DisplayRole).toInt();
            painter->setPen(getSeedersColor(seeders));
            QFont font = option.font;
            font.setBold(seeders > 0);
            painter->setFont(font);
            painter->drawText(rect, Qt::AlignVCenter | Qt::AlignCenter, QString::number(seeders));
            break;
        }
        
        case SearchResultModel::LeechersColumn: {
            int leechers = index.data(Qt::DisplayRole).toInt();
            painter->setPen(getLeechersColor(leechers));
            QFont font = option.font;
            font.setBold(leechers > 0);
            painter->setFont(font);
            painter->drawText(rect, Qt::AlignVCenter | Qt::AlignCenter, QString::number(leechers));
            break;
        }
        
        case SearchResultModel::DateColumn: {
            painter->setPen(QColor("#888888"));
            QString date = index.data(Qt::DisplayRole).toString();
            painter->drawText(rect, Qt::AlignVCenter | Qt::AlignLeft, date);
            break;
        }
        
        default:
            QStyledItemDelegate::paint(painter, option, index);
            break;
    }
    
    // Draw bottom border
    painter->setPen(QColor("#3c3f41"));
    painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
    
    painter->restore();
}

QSize TorrentItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                     const QModelIndex &index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);
    return QSize(-1, 36);  // Fixed row height for better readability
}

void TorrentItemDelegate::drawRatingBar(QPainter *painter, const QRect &rect, 
                                         int good, int bad) const
{
    if (good == 0 && bad == 0) return;
    
    int total = good + bad;
    double rating = (static_cast<double>(good) / total) * 100.0;
    
    // Background
    painter->setBrush(QColor("#444444"));
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(rect, 2, 2);
    
    // Rating bar
    int barWidth = static_cast<int>(rect.width() * (rating / 100.0));
    QRect barRect = rect;
    barRect.setWidth(barWidth);
    
    QColor barColor = rating >= 50 ? QColor("#00E676") : QColor("#FF3D00");
    painter->setBrush(barColor);
    painter->drawRoundedRect(barRect, 2, 2);
}

