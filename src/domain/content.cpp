#include "domain/content.h"

namespace rats::domain {

QString toString(ContentType type)
{
    switch (type) {
    case ContentType::Video:
        return QStringLiteral("video");
    case ContentType::Audio:
        return QStringLiteral("audio");
    case ContentType::Books:
        return QStringLiteral("books");
    case ContentType::Pictures:
        return QStringLiteral("pictures");
    case ContentType::Software:
        return QStringLiteral("software");
    case ContentType::Games:
        return QStringLiteral("games");
    case ContentType::Archive:
        return QStringLiteral("archive");
    case ContentType::Bad:
        return QStringLiteral("bad");
    case ContentType::Unknown:
        break;
    }
    return QString();
}

QString toString(ContentCategory category)
{
    switch (category) {
    case ContentCategory::Movie:
        return QStringLiteral("movie");
    case ContentCategory::Series:
        return QStringLiteral("series");
    case ContentCategory::Documentary:
        return QStringLiteral("documentary");
    case ContentCategory::Anime:
        return QStringLiteral("anime");
    case ContentCategory::Music:
        return QStringLiteral("music");
    case ContentCategory::Ebook:
        return QStringLiteral("ebook");
    case ContentCategory::Comics:
        return QStringLiteral("comics");
    case ContentCategory::Software:
        return QStringLiteral("software");
    case ContentCategory::Game:
        return QStringLiteral("game");
    case ContentCategory::XXX:
        return QStringLiteral("xxx");
    case ContentCategory::Unknown:
        break;
    }
    return QString();
}

ContentType contentTypeFromString(const QString& s)
{
    const QString v = s.toLower();
    if (v == QLatin1String("video"))
        return ContentType::Video;
    if (v == QLatin1String("audio"))
        return ContentType::Audio;
    if (v == QLatin1String("books"))
        return ContentType::Books;
    if (v == QLatin1String("pictures"))
        return ContentType::Pictures;
    if (v == QLatin1String("software"))
        return ContentType::Software;
    if (v == QLatin1String("games"))
        return ContentType::Games;
    if (v == QLatin1String("archive"))
        return ContentType::Archive;
    if (v == QLatin1String("bad"))
        return ContentType::Bad;
    return ContentType::Unknown;
}

ContentCategory contentCategoryFromString(const QString& s)
{
    const QString v = s.toLower();
    if (v == QLatin1String("movie"))
        return ContentCategory::Movie;
    if (v == QLatin1String("series"))
        return ContentCategory::Series;
    if (v == QLatin1String("documentary"))
        return ContentCategory::Documentary;
    if (v == QLatin1String("anime"))
        return ContentCategory::Anime;
    if (v == QLatin1String("music"))
        return ContentCategory::Music;
    if (v == QLatin1String("ebook"))
        return ContentCategory::Ebook;
    if (v == QLatin1String("comics"))
        return ContentCategory::Comics;
    if (v == QLatin1String("software"))
        return ContentCategory::Software;
    if (v == QLatin1String("game"))
        return ContentCategory::Game;
    if (v == QLatin1String("xxx"))
        return ContentCategory::XXX;
    return ContentCategory::Unknown;
}

ContentType contentTypeFromId(int id)
{
    switch (id) {
    case 1:
        return ContentType::Video;
    case 2:
        return ContentType::Audio;
    case 3:
        return ContentType::Books;
    case 4:
        return ContentType::Pictures;
    case 5:
        return ContentType::Software;
    case 6:
        return ContentType::Games;
    case 7:
        return ContentType::Archive;
    case 100:
        return ContentType::Bad;
    default:
        return ContentType::Unknown;
    }
}

ContentCategory contentCategoryFromId(int id)
{
    switch (id) {
    case 1:
        return ContentCategory::Movie;
    case 2:
        return ContentCategory::Series;
    case 3:
        return ContentCategory::Documentary;
    case 4:
        return ContentCategory::Anime;
    case 5:
        return ContentCategory::Music;
    case 6:
        return ContentCategory::Ebook;
    case 7:
        return ContentCategory::Comics;
    case 8:
        return ContentCategory::Software;
    case 9:
        return ContentCategory::Game;
    case 100:
        return ContentCategory::XXX;
    default:
        return ContentCategory::Unknown;
    }
}

} // namespace rats::domain
