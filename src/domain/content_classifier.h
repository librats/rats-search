#ifndef RATS_DOMAIN_CONTENT_CLASSIFIER_H
#define RATS_DOMAIN_CONTENT_CLASSIFIER_H

#include "domain/content.h"
#include "domain/torrent.h"

#include <QString>
#include <QVector>

namespace rats::domain {

// Determines a torrent's content type/category from its name and file list.
//
// Detection is weighted-by-size file-type voting, plus adult-content word
// blocking for video/pictures/archive. The extension→type table and the
// bad-word lists are loaded once from the Qt resource bundle
// (:/content/extensions.json, :/content/badwords.json) instead of being
// compiled into a header.
struct Classification {
    ContentType type = ContentType::Unknown;
    ContentCategory category = ContentCategory::Unknown;
};

class ContentClassifier {
public:
    // Classify from a name and file list. Pure — no side effects.
    static Classification classify(const QString& name, const QVector<File>& files);

    // Convenience: classify and write the result back onto the torrent.
    static void classify(Torrent& torrent);

    // The size-weighted file-type vote in isolation (exposed for testing).
    static ContentType detectTypeFromFiles(const QVector<File>& files);
};

} // namespace rats::domain

#endif // RATS_DOMAIN_CONTENT_CLASSIFIER_H
