#include "torrentitemdelegate.h"
#include "searchresultmodel.h"
#include <QPainter>
#include <QApplication>
#include <QPainterPath>
#include <QRegularExpression>

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
        {"software", QColor("#e3561e")},  // Alias for application
        {"games", QColor("#9c27b0")},
        {"archive", QColor("#1e25e3")},
        {"disc", QColor("#1ee381")},
        {"bad", QColor("#888888")},
        {"unknown", QColor("#000000")}
    };
    return colors.value(contentType.toLower(), QColor("#000000"));
}

QString TorrentItemDelegate::getContentTypeName(const QString &contentType)
{
    static const QMap<QString, QString> names = {
        {"video", "Video"},
        {"audio", "Audio"},
        {"pictures", "Pictures"},
        {"books", "Books"},
        {"software", "Software"},
        {"application", "Application"},  // Alias
        {"games", "Games"},
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
    
    // Get colors from palette for theme support
    const QPalette& palette = option.palette;
    
    // Detect if dark mode based on background color luminance
    QColor baseColor = palette.color(QPalette::Base);
    bool isDarkMode = (baseColor.red() + baseColor.green() + baseColor.blue()) / 3 < 128;
    
    // Background colors (theme-aware)
    QColor bgColor;
    if (option.state & QStyle::State_Selected) {
        bgColor = palette.color(QPalette::Highlight);
    } else if (option.state & QStyle::State_MouseOver) {
        bgColor = isDarkMode ? QColor("#3c4048") : QColor("#f0f7ff");
    } else if (index.row() % 2 == 0) {
        bgColor = palette.color(QPalette::Base);
    } else {
        bgColor = palette.color(QPalette::AlternateBase);
    }
    painter->fillRect(option.rect, bgColor);
    
    // Text colors (theme-aware)
    QColor textColor = palette.color(QPalette::Text);
    QColor mutedTextColor = isDarkMode ? QColor("#aaaaaa") : QColor("#666666");
    QColor dimTextColor = isDarkMode ? QColor("#888888") : QColor("#999999");
    QColor borderColor = isDarkMode ? QColor("#3c3f41") : QColor("#e0e0e0");
    
    // Get column
    int column = index.column();
    // Paddings
    int borderBottom = 1; // Border bottom line from style sheet take one pixel of bottom padding
    QRect rect = option.rect.adjusted(4, 2 - borderBottom, -4, -2 - borderBottom);
    
    switch (column) {
        case SearchResultModel::NameColumn: {
            // Get matching file paths if this is a file search result
            QStringList matchingPaths = index.data(SearchResultModel::MatchingPathsRole).toStringList();
            bool hasFilePaths = !matchingPaths.isEmpty();
            
            // Calculate name area (top portion for torrents with file paths)
            QRect nameRect = rect;
            if (hasFilePaths) {
                nameRect.setHeight(BaseRowHeight - 4);
            }
            
            // Draw content type icon (small colored square)
            QString contentType = index.data(Qt::UserRole + 1).toString();
            int iconLeft = nameRect.left();
            if (!contentType.isEmpty()) {
                QColor typeColor = getContentTypeColor(contentType);
                const int filesOffset = hasFilePaths ? -3 : 0; // Offset for files as name will be shifted top
                QRect iconRect(iconLeft, nameRect.top() + filesOffset + borderBottom + (qMin(nameRect.height(), BaseRowHeight - 4) - 12) / 2, 12, 12);
                painter->setBrush(typeColor);
                painter->setPen(Qt::NoPen);
                painter->drawRoundedRect(iconRect, 2, 2);
                nameRect.setLeft(iconLeft + 18);
            }
            
            // Draw name - use selected text color if selected
            if (option.state & QStyle::State_Selected) {
                painter->setPen(palette.color(QPalette::HighlightedText));
            } else {
                painter->setPen(textColor);
            }
            QFont font = option.font;
            font.setPointSize(10);
            painter->setFont(font);
            QString name = index.data(Qt::DisplayRole).toString();
            
            QRect textRect = nameRect;
            if (hasFilePaths) {
                textRect.setTop(nameRect.top() + 2);
                painter->drawText(textRect, Qt::AlignTop | Qt::AlignLeft | Qt::TextSingleLine, 
                                 option.fontMetrics.elidedText(name, Qt::ElideRight, textRect.width()));
            } else {
                painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine, 
                                 option.fontMetrics.elidedText(name, Qt::ElideRight, textRect.width()));
            }
            
            // Draw matching file paths below the name
            if (hasFilePaths) {
                QFont pathFont = option.font;
                pathFont.setPointSize(8);
                painter->setFont(pathFont);
                
                QColor pathColor = isDarkMode ? QColor("#9e9e9e") : QColor("#757575");
                QColor highlightColor = isDarkMode ? QColor("#ffab40") : QColor("#e65100");
                
                int pathTop = rect.top() + BaseRowHeight - 6;
                int pathsToShow = qMin(matchingPaths.size(), MaxVisiblePaths);
                
                for (int i = 0; i < pathsToShow; ++i) {
                    QRect pathRect(iconLeft + 20, pathTop + (i * FilePathRowHeight), 
                                  rect.width() - 24, FilePathRowHeight);
                    
                    // Draw tree connector
                    painter->setPen(pathColor);
                    painter->drawText(QRect(iconLeft + 4, pathRect.top(), 16, FilePathRowHeight),
                                     Qt::AlignVCenter | Qt::AlignLeft, "└");
                    
                    // Draw highlighted path
                    drawHighlightedPath(painter, pathRect, matchingPaths[i], 
                                       pathColor, highlightColor, pathFont);
                }
            }
            break;
        }
        
        case SearchResultModel::SizeColumn: {
            if (option.state & QStyle::State_Selected) {
                painter->setPen(palette.color(QPalette::HighlightedText));
            } else {
                painter->setPen(mutedTextColor);
            }
            QString size = index.data(Qt::DisplayRole).toString();
            painter->drawText(rect, Qt::AlignVCenter | Qt::AlignRight, size);
            break;
        }
        
        case SearchResultModel::SeedersColumn: {
            int seeders = index.data(Qt::DisplayRole).toInt();
            // Seeders color stays the same (green tones) - visible on both themes
            painter->setPen(getSeedersColor(seeders));
            QFont font = option.font;
            font.setBold(seeders > 0);
            painter->setFont(font);
            painter->drawText(rect, Qt::AlignVCenter | Qt::AlignCenter, QString::number(seeders));
            break;
        }
        
        case SearchResultModel::LeechersColumn: {
            int leechers = index.data(Qt::DisplayRole).toInt();
            // Leechers color stays the same (purple tones) - visible on both themes
            painter->setPen(getLeechersColor(leechers));
            QFont font = option.font;
            font.setBold(leechers > 0);
            painter->setFont(font);
            painter->drawText(rect, Qt::AlignVCenter | Qt::AlignCenter, QString::number(leechers));
            break;
        }
        
        case SearchResultModel::DateColumn: {
            if (option.state & QStyle::State_Selected) {
                painter->setPen(palette.color(QPalette::HighlightedText));
            } else {
                painter->setPen(dimTextColor);
            }
            QString date = index.data(Qt::DisplayRole).toString();
            painter->drawText(rect, Qt::AlignVCenter | Qt::AlignLeft, date);
            break;
        }
        
        default:
            QStyledItemDelegate::paint(painter, option, index);
            break;
    }
    
    // Draw bottom border (theme-aware)
    painter->setPen(borderColor);
    painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
    
    painter->restore();
}

QSize TorrentItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                     const QModelIndex &index) const
{
    Q_UNUSED(option);
    
    // Check if this result has matching file paths
    QStringList matchingPaths = index.data(SearchResultModel::MatchingPathsRole).toStringList();
    
    if (matchingPaths.isEmpty()) {
        return QSize(-1, BaseRowHeight);
    }
    
    // Calculate height based on number of file paths to show
    int pathsToShow = qMin(matchingPaths.size(), MaxVisiblePaths);
    int totalHeight = BaseRowHeight + (pathsToShow * FilePathRowHeight);
    
    return QSize(-1, totalHeight);
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

void TorrentItemDelegate::drawHighlightedPath(QPainter *painter, const QRect &rect,
                                               const QString &path, const QColor &textColor,
                                               const QColor &highlightColor, const QFont &font) const
{
    // Parse path with <b>highlighted</b> sections and render
    // Format: "some/path/<b>match</b>/file.txt"
    
    QFont normalFont = font;
    QFont boldFont = font;
    boldFont.setBold(true);
    
    QFontMetrics fm(normalFont);
    QFontMetrics fmBold(boldFont);
    
    int x = rect.left();
    int y = rect.top();
    int maxWidth = rect.width();
    int height = rect.height();
    
    // Parse <b> tags
    QString remaining = path;
    static QRegularExpression boldRegex("<b>([^<]*)</b>");
    
    while (!remaining.isEmpty() && x < rect.right()) {
        QRegularExpressionMatch match = boldRegex.match(remaining);
        
        if (match.hasMatch()) {
            // Draw text before the bold part
            QString beforeBold = remaining.left(match.capturedStart());
            if (!beforeBold.isEmpty()) {
                painter->setFont(normalFont);
                painter->setPen(textColor);
                QString elidedBefore = fm.elidedText(beforeBold, Qt::ElideRight, maxWidth - (x - rect.left()));
                painter->drawText(x, y, maxWidth - (x - rect.left()), height, 
                                 Qt::AlignVCenter | Qt::AlignLeft, elidedBefore);
                x += fm.horizontalAdvance(elidedBefore);
            }
            
            // Draw the bold (highlighted) part
            QString boldText = match.captured(1);
            if (!boldText.isEmpty() && x < rect.right()) {
                painter->setFont(boldFont);
                painter->setPen(highlightColor);
                QString elidedBold = fmBold.elidedText(boldText, Qt::ElideRight, maxWidth - (x - rect.left()));
                painter->drawText(x, y, maxWidth - (x - rect.left()), height,
                                 Qt::AlignVCenter | Qt::AlignLeft, elidedBold);
                x += fmBold.horizontalAdvance(elidedBold);
            }
            
            remaining = remaining.mid(match.capturedEnd());
        } else {
            // No more bold tags, draw remaining text
            painter->setFont(normalFont);
            painter->setPen(textColor);
            QString elidedRemaining = fm.elidedText(remaining, Qt::ElideRight, maxWidth - (x - rect.left()));
            painter->drawText(x, y, maxWidth - (x - rect.left()), height,
                             Qt::AlignVCenter | Qt::AlignLeft, elidedRemaining);
            break;
        }
    }
}
