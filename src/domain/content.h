#ifndef RATS_DOMAIN_CONTENT_H
#define RATS_DOMAIN_CONTENT_H

#include <QString>

namespace rats::domain {

// Content type of a torrent. The numeric values ARE the ids stored in the
// Manticore `contenttype` column, so they must never be renumbered.
enum class ContentType {
    Unknown = 0,
    Video = 1,
    Audio = 2,
    Books = 3,
    Pictures = 4,
    Software = 5,
    Games = 6,
    Archive = 7,
    Bad = 100,
};

// Finer-grained category. Values are the ids stored in `contentcategory`.
enum class ContentCategory {
    Unknown = 0,
    Movie = 1,
    Series = 2,
    Documentary = 3,
    Anime = 4,
    Music = 5,
    Ebook = 6,
    Comics = 7,
    Software = 8,
    Game = 9,
    XXX = 100,
};

// String <-> enum conversion. The strings are the canonical wire/JSON names
// (lower-case, e.g. "video", "movie"). Unknown/empty maps to the Unknown enum.
QString toString(ContentType type);
QString toString(ContentCategory category);
ContentType contentTypeFromString(const QString& s);
ContentCategory contentCategoryFromString(const QString& s);

inline int toId(ContentType type)
{
    return static_cast<int>(type);
}
inline int toId(ContentCategory category)
{
    return static_cast<int>(category);
}
ContentType contentTypeFromId(int id);
ContentCategory contentCategoryFromId(int id);

} // namespace rats::domain

#endif // RATS_DOMAIN_CONTENT_H
