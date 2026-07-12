#include "data/feed_repository.h"

#include "data/database.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantMap>

namespace rats::data {

namespace {
const QString kFeed = QStringLiteral("feed");
}

FeedRepository::FeedRepository(Database* db, QObject* parent) : QObject(parent), db_(db) { }

QJsonArray FeedRepository::load(int maxItems)
{
    QJsonArray result;
    if (!db_ || !db_->isConnected())
        return result;

    // Schema: id (auto), feedIndex (rt_field), data (rt_attr_json). The item is
    // stored as JSON in `data`, like the legacy feed.js (JSON.parse(f.data)).
    const auto rows = db_->query(QStringLiteral("SELECT * FROM feed LIMIT %1").arg(maxItems));
    for (const auto& row : rows) {
        const QString jsonData = row.value(QStringLiteral("data")).toString();
        if (jsonData.isEmpty())
            continue;

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            qWarning() << "[FeedRepository] failed to parse feed item JSON:" << parseError.errorString();
            continue;
        }
        result.append(doc.object());
    }

    return result;
}

bool FeedRepository::replaceAll(const QJsonArray& items)
{
    if (!db_ || !db_->isConnected())
        return false;

    // Manticore has no usable TRUNCATE, so clear by delete-all (legacy behaviour:
    // 'delete from feed where id > 0'). May fail harmlessly on an empty table.
    db_->execute(QStringLiteral("DELETE FROM feed WHERE id > 0"));

    // One batched INSERT instead of one per item: the feed holds up to 1000 items
    // and is rewritten in full on every flush, so a row-at-a-time loop meant a
    // thousand round trips (and a thousand log lines) each time.
    qint64 nextId = 1;
    QVector<QVariantMap> rows;
    rows.reserve(items.size());
    for (const auto& value : items) {
        if (!value.isObject())
            continue;

        QVariantMap row;
        row[QStringLiteral("id")] = nextId++;
        row[QStringLiteral("data")]
            = QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
        rows.append(row);
    }

    return db_->insertMany(kFeed, rows);
}

} // namespace rats::data
