#ifndef RATS_COMMON_INFOHASH_H
#define RATS_COMMON_INFOHASH_H

#include <QString>

namespace rats {

// A BitTorrent v1 info-hash is a 40-character hex SHA-1 digest. This is the one
// place that validates and normalises it; never hand-roll `hash.length() !=
// 40`.
namespace infohash {

inline constexpr int kLength = 40;

inline bool isValid(const QString& hash)
{
    if (hash.length() != kLength)
        return false;
    for (QChar c : hash) {
        if (!c.isDigit() && !(c.toLower() >= 'a' && c.toLower() <= 'f'))
            return false;
    }
    return true;
}

// Normalised form used everywhere internally: lower-case hex.
inline QString normalize(const QString& hash)
{
    return hash.trimmed().toLower();
}

} // namespace infohash
} // namespace rats

#endif // RATS_COMMON_INFOHASH_H
