#ifndef RATS_UI_FORMAT_H
#define RATS_UI_FORMAT_H

#include "domain/content.h"

#include <QDateTime>
#include <QString>

// Shared UI formatting helpers — the one place widgets get size/speed/date
// strings from, so their formatting stays consistent.
namespace rats::ui {

inline QString formatSize(qint64 bytes)
{
    if (bytes <= 0)
        return QStringLiteral("0 B");
    static const char* units[] = { "B", "KB", "MB", "GB", "TB", "PB" };
    double size = static_cast<double>(bytes);
    int unit = 0;
    while (size >= 1024.0 && unit < 5) {
        size /= 1024.0;
        ++unit;
    }
    return QStringLiteral("%1 %2").arg(size, 0, 'f', unit == 0 ? 0 : 2).arg(QLatin1String(units[unit]));
}

inline QString formatSpeed(qint64 bytesPerSecond)
{
    return formatSize(bytesPerSecond) + QStringLiteral("/s");
}

inline QString formatDate(const QDateTime& when)
{
    if (!when.isValid())
        return QString();
    return when.toString(QStringLiteral("yyyy-MM-dd hh:mm"));
}

inline QString capitalizeFirst(const QString& text)
{
    if (text.isEmpty())
        return text;
    QString result = text;
    result[0] = result[0].toUpper();
    return result;
}

// Emoji/icon for a content type.
inline QString contentTypeIcon(domain::ContentType type)
{
    switch (type) {
    case domain::ContentType::Video:
        return QStringLiteral("🎬");
    case domain::ContentType::Audio:
        return QStringLiteral("🎵");
    case domain::ContentType::Books:
        return QStringLiteral("📚");
    case domain::ContentType::Pictures:
        return QStringLiteral("🖼️");
    case domain::ContentType::Software:
        return QStringLiteral("💿");
    case domain::ContentType::Games:
        return QStringLiteral("🎮");
    case domain::ContentType::Archive:
        return QStringLiteral("📦");
    default:
        return QStringLiteral("📄");
    }
}

} // namespace rats::ui

#endif // RATS_UI_FORMAT_H
