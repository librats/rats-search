#include "services/filter_policy.h"

#include "domain/content.h"

#include <QRegularExpression>

namespace rats::service {

FilterPolicy::FilterPolicy(FilterSettings settings) : settings_(std::move(settings))
{
    compileNamingRegex();
}

void FilterPolicy::setSettings(FilterSettings settings)
{
    settings_ = std::move(settings);
    compileNamingRegex();
}

void FilterPolicy::compileNamingRegex()
{
    if (settings_.namingRegExp.isEmpty()) {
        namingRegex_ = QRegularExpression();
        return;
    }
    namingRegex_ = QRegularExpression(settings_.namingRegExp, QRegularExpression::CaseInsensitiveOption);
}

QString FilterPolicy::rejectionReason(const domain::Torrent& t) const
{
    if (QString r = checkFileCount(t); !r.isEmpty())
        return r;
    if (QString r = checkSize(t); !r.isEmpty())
        return r;
    if (QString r = checkAdult(t); !r.isEmpty())
        return r;
    if (QString r = checkNamingRegExp(t); !r.isEmpty())
        return r;
    if (QString r = checkContentType(t); !r.isEmpty())
        return r;
    return QString();
}

QString FilterPolicy::checkFileCount(const domain::Torrent& t) const
{
    if (settings_.maxFiles > 0 && t.files > settings_.maxFiles)
        return QStringLiteral("Too many files: %1 > %2").arg(t.files).arg(settings_.maxFiles);
    return QString();
}

QString FilterPolicy::checkSize(const domain::Torrent& t) const
{
    if (settings_.sizeMin > 0 && t.size < settings_.sizeMin)
        return QStringLiteral("Size too small: %1 < %2").arg(t.size).arg(settings_.sizeMin);
    if (settings_.sizeMax > 0 && t.size > settings_.sizeMax)
        return QStringLiteral("Size too large: %1 > %2").arg(t.size).arg(settings_.sizeMax);
    return QString();
}

QString FilterPolicy::checkAdult(const domain::Torrent& t) const
{
    if (!settings_.adultFilter)
        return QString();

    static const QStringList keywords = { QStringLiteral("xxx"), QStringLiteral("porn"), QStringLiteral("sex"),
        QStringLiteral("adult"), QStringLiteral("18+"), QStringLiteral("nsfw") };
    const QString nameLower = t.name.toLower();
    for (const QString& keyword : keywords) {
        if (nameLower.contains(keyword))
            return QStringLiteral("Adult content detected: %1").arg(keyword);
    }
    if (t.contentCategory == domain::ContentCategory::XXX)
        return QStringLiteral("Adult content category");
    return QString();
}

QString FilterPolicy::checkNamingRegExp(const domain::Torrent& t) const
{
    if (settings_.namingRegExp.isEmpty() || !namingRegex_.isValid())
        return QString();

    const bool matches = namingRegex_.match(t.name).hasMatch();
    if (settings_.namingRegExpNegative) {
        if (matches)
            return QStringLiteral("Name matches blocked pattern: %1").arg(settings_.namingRegExp);
    } else if (!matches) {
        return QStringLiteral("Name doesn't match required pattern: %1").arg(settings_.namingRegExp);
    }
    return QString();
}

QString FilterPolicy::checkContentType(const domain::Torrent& t) const
{
    const QString filter = settings_.contentTypeFilter;
    if (filter.isEmpty() || filter == QLatin1String("all"))
        return QString();

    const QStringList allowed = filter.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (allowed.isEmpty())
        return QString();

    const QString typeName = domain::toString(t.contentType);
    for (const QString& raw : allowed) {
        const QString type = raw.trimmed();
        // "application" is a UI umbrella token spanning Software + Games
        // (mirrors data::TorrentRepository::contentTypeFilter). "disc" has no
        // matching ContentType and is a legacy no-op token.
        if (type.compare(QLatin1String("application"), Qt::CaseInsensitive) == 0) {
            if (t.contentType == domain::ContentType::Software || t.contentType == domain::ContentType::Games)
                return QString();
            continue;
        }
        if (typeName.compare(type, Qt::CaseInsensitive) == 0)
            return QString();
    }
    return QStringLiteral("Content type not allowed: %1").arg(typeName);
}

} // namespace rats::service
