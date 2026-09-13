#include "torrentitemdelegate.h"
#include "domain/content.h"
#include "format.h"
#include "searchresultmodel.h"
#include "theme.h"
#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>

TorrentItemDelegate::TorrentItemDelegate(QObject* parent) : QStyledItemDelegate(parent) { }

QColor TorrentItemDelegate::getSeedersColor(int seeders)
{
    const rats::ui::Theme& theme = rats::ui::Theme::instance();
    if (seeders > 50)
        return theme.color(QLatin1String("seedersHigh"));
    if (seeders > 10)
        return theme.color(QLatin1String("seedersMid"));
    if (seeders > 0)
        return theme.color(QLatin1String("seedersLow"));
    return theme.color(QLatin1String("peersNone"));
}

QColor TorrentItemDelegate::getLeechersColor(int leechers)
{
    const rats::ui::Theme& theme = rats::ui::Theme::instance();
    if (leechers > 50)
        return theme.color(QLatin1String("leechersHigh"));
    if (leechers > 10)
        return theme.color(QLatin1String("leechersMid"));
    if (leechers > 0)
        return theme.color(QLatin1String("leechersLow"));
    return theme.color(QLatin1String("peersNone"));
}

void TorrentItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if (!index.isValid()) {
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    // Every colour comes from the same token table the stylesheet is built
    // from, so the rows cannot drift from the view they sit in.
    const rats::ui::Theme& theme = rats::ui::Theme::instance();
    const QColor selectedTextColor = theme.color(QLatin1String("textOnAccent"));

    // A hit a peer answered with is not in the local index: it gets a tinted row
    // and a stripe down its left edge, so "found elsewhere" is readable at a
    // glance without a column of its own.
    const bool remoteHit = index.data(SearchResultModel::RemoteRole).toBool();

    QColor bgColor;
    if (option.state & QStyle::State_Selected) {
        bgColor = theme.color(QLatin1String("accent"));
    } else if (option.state & QStyle::State_MouseOver) {
        bgColor = theme.color(QLatin1String("rowHover"));
    } else if (index.row() % 2 == 0) {
        bgColor = theme.color(remoteHit ? QLatin1String("remoteRow") : QLatin1String("surface"));
    } else {
        bgColor = theme.color(remoteHit ? QLatin1String("remoteRowAlt") : QLatin1String("surfaceAlt"));
    }
    painter->fillRect(option.rect, bgColor);

    const QColor textColor = theme.color(QLatin1String("text"));
    const QColor mutedTextColor = theme.color(QLatin1String("textMuted"));
    const QColor dimTextColor = theme.color(QLatin1String("textFaint"));
    const QColor borderColor = theme.color(QLatin1String("rowBorder"));

    // Get column
    int column = index.column();
    // Paddings
    int borderBottom = 1; // Border bottom line from style sheet take one pixel of bottom padding
    QRect rect = option.rect.adjusted(4, 2 - borderBottom, -4, -2 - borderBottom);

    // Only the first column carries the stripe — repeated at every column border
    // it would read as a grid. On a selected row the violet would sink into the
    // accent fill, so there it is drawn in the selection's own text colour.
    if (remoteHit && column == SearchResultModel::NameColumn) {
        painter->fillRect(QRect(option.rect.left(), option.rect.top(), RemoteStripeWidth, option.rect.height() - 1),
            option.state & QStyle::State_Selected ? selectedTextColor : theme.color(QLatin1String("remoteStripe")));
        rect.setLeft(rect.left() + RemoteStripeWidth);
    }

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

        // Draw content type icon (emoji glyph from the domain content type)
        rats::domain::ContentType contentType
            = rats::domain::contentTypeFromId(index.data(SearchResultModel::ContentTypeRole).toInt());
        QString typeIcon = rats::ui::contentTypeIcon(contentType);
        int iconLeft = nameRect.left();
        if (!typeIcon.isEmpty()) {
            const int filesOffset = hasFilePaths ? -3 : 0; // Offset for files as name will be shifted top
            QRect iconRect(iconLeft,
                nameRect.top() + filesOffset + borderBottom + (qMin(nameRect.height(), BaseRowHeight - 4) - 14) / 2, 16,
                14);
            QFont iconFont = option.font;
            iconFont.setPointSize(10);
            painter->setFont(iconFont);
            painter->setPen(option.state & QStyle::State_Selected ? selectedTextColor : textColor);
            painter->drawText(iconRect, Qt::AlignVCenter | Qt::AlignLeft, typeIcon);
            nameRect.setLeft(iconLeft + 18);
        }

        // Draw name - use selected text color if selected
        painter->setPen(option.state & QStyle::State_Selected ? selectedTextColor : textColor);
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

            const QColor pathColor
                = option.state & QStyle::State_Selected ? selectedTextColor : theme.color(QLatin1String("pathText"));
            // The plain highlight is tuned for the row fill; over the accent it
            // needs the lighter amber to stay legible.
            const QColor highlightColor
                = theme.color(option.state & QStyle::State_Selected ? QLatin1String("matchHighlightSelected")
                                                                    : QLatin1String("matchHighlight"));

            int pathTop = rect.top() + BaseRowHeight - 6;
            int pathsToShow = qMin(matchingPaths.size(), MaxVisiblePaths);

            for (int i = 0; i < pathsToShow; ++i) {
                QRect pathRect(iconLeft + 20, pathTop + (i * FilePathRowHeight), rect.width() - 24, FilePathRowHeight);

                // Draw tree connector
                painter->setPen(pathColor);
                painter->drawText(
                    QRect(iconLeft + 4, pathRect.top(), 16, FilePathRowHeight), Qt::AlignVCenter | Qt::AlignLeft, "└");

                // Draw highlighted path
                drawHighlightedPath(painter, pathRect, matchingPaths[i], pathColor, highlightColor, pathFont);
            }
        }
        break;
    }

    case SearchResultModel::SizeColumn: {
        painter->setPen(option.state & QStyle::State_Selected ? selectedTextColor : mutedTextColor);
        QString size = index.data(Qt::DisplayRole).toString();
        painter->drawText(rect, Qt::AlignVCenter | Qt::AlignRight, size);
        break;
    }

    case SearchResultModel::SeedersColumn: {
        int seeders = index.data(Qt::DisplayRole).toInt();
        // The swarm-health tint would sit unreadably on the accent fill, so a
        // selected row falls back to the selection's own text colour.
        painter->setPen(option.state & QStyle::State_Selected ? selectedTextColor : getSeedersColor(seeders));
        QFont font = option.font;
        font.setBold(seeders > 0);
        painter->setFont(font);
        painter->drawText(rect, Qt::AlignVCenter | Qt::AlignCenter, QString::number(seeders));
        break;
    }

    case SearchResultModel::LeechersColumn: {
        int leechers = index.data(Qt::DisplayRole).toInt();
        painter->setPen(option.state & QStyle::State_Selected ? selectedTextColor : getLeechersColor(leechers));
        QFont font = option.font;
        font.setBold(leechers > 0);
        painter->setFont(font);
        painter->drawText(rect, Qt::AlignVCenter | Qt::AlignCenter, QString::number(leechers));
        break;
    }

    case SearchResultModel::DateColumn: {
        painter->setPen(option.state & QStyle::State_Selected ? selectedTextColor : dimTextColor);
        QString date = index.data(Qt::DisplayRole).toString();
        painter->drawText(rect, Qt::AlignVCenter | Qt::AlignLeft, date);
        break;
    }

    default:
        QStyledItemDelegate::paint(painter, option, index);
        break;
    }

    painter->setPen(borderColor);
    painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());

    painter->restore();
}

QSize TorrentItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
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

void TorrentItemDelegate::drawHighlightedPath(QPainter* painter, const QRect& rect, const QString& path,
    const QColor& textColor, const QColor& highlightColor, const QFont& font) const
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
                painter->drawText(
                    x, y, maxWidth - (x - rect.left()), height, Qt::AlignVCenter | Qt::AlignLeft, elidedBefore);
                x += fm.horizontalAdvance(elidedBefore);
            }

            // Draw the bold (highlighted) part
            QString boldText = match.captured(1);
            if (!boldText.isEmpty() && x < rect.right()) {
                painter->setFont(boldFont);
                painter->setPen(highlightColor);
                QString elidedBold = fmBold.elidedText(boldText, Qt::ElideRight, maxWidth - (x - rect.left()));
                painter->drawText(
                    x, y, maxWidth - (x - rect.left()), height, Qt::AlignVCenter | Qt::AlignLeft, elidedBold);
                x += fmBold.horizontalAdvance(elidedBold);
            }

            remaining = remaining.mid(match.capturedEnd());
        } else {
            // No more bold tags, draw remaining text
            painter->setFont(normalFont);
            painter->setPen(textColor);
            QString elidedRemaining = fm.elidedText(remaining, Qt::ElideRight, maxWidth - (x - rect.left()));
            painter->drawText(
                x, y, maxWidth - (x - rect.left()), height, Qt::AlignVCenter | Qt::AlignLeft, elidedRemaining);
            break;
        }
    }
}
