#ifndef RATS_DATA_FEED_REPOSITORY_H
#define RATS_DATA_FEED_REPOSITORY_H

#include <QJsonArray>
#include <QObject>

namespace rats::data {

class Database;

// Persistence for the voted-torrents feed. The whole feed lives in the
// Manticore `feed` table (schema: rt_field feedIndex, rt_attr_json data) as one
// JSON blob per row. The repository is deliberately dumb about feed semantics:
// it moves opaque JSON objects in and out of the table, so the item shape
// (ranking, dates) stays a service concern and the data layer never depends on
// the service layer.
class FeedRepository : public QObject {
    Q_OBJECT

public:
    explicit FeedRepository(Database* db, QObject* parent = nullptr);

    // Read every stored feed row and return the parsed item JSON objects. Rows
    // whose `data` is empty or unparseable are skipped.
    QJsonArray load(int maxItems = 1000);

    // Replace the entire feed table with `items` (a delete-all followed by a
    // full reinsert). Each element must be a JSON object; non-objects are
    // skipped. Returns false if any row failed to persist.
    bool replaceAll(const QJsonArray& items);

private:
    Database* db_;
};

} // namespace rats::data

#endif // RATS_DATA_FEED_REPOSITORY_H
