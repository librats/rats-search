#ifndef RATS_DATA_DATABASE_H
#define RATS_DATA_DATABASE_H

#include <QObject>
#include <QVariantMap>
#include <QVector>

namespace rats::data {

class Manticore;

// Thin synchronous execution layer over the Manticore connection. The four
// write helpers (insert/update/remove/execute) share a single logged execution
// core. Asynchrony is a service-layer concern, so there is no queryAsync here.
//
// Not thread-safe by itself, but safe to use from multiple threads because the
// underlying connection is per-thread; per-call error status is returned
// through the `ok` out-parameter / bool return rather than shared state.
class Database : public QObject {
public:
    using Row = QVariantMap;
    using Rows = QVector<Row>;

    explicit Database(Manticore* manticore, QObject* parent = nullptr);

    bool isConnected() const;

    // Run a SELECT (or any row-returning statement). `?` placeholders in `sql`
    // are substituted with escaped `params`. On error the result is empty and
    // *ok (if provided) is set to false.
    Rows query(const QString& sql, const QVariantList& params = {}, bool* ok = nullptr);

    // Run a statement with no result set (DDL, OPTIMIZE, FLUSH, ...).
    bool execute(const QString& sql);

    bool insert(const QString& table, const QVariantMap& values);
    // Multi-row INSERT. Columns come from the first row (every row must carry the
    // same keys). Rows are batched into as few statements as fit under Manticore's
    // packet limit, so bulk writes cost a handful of queries instead of one each.
    bool insertMany(const QString& table, const QVector<QVariantMap>& rows);
    bool update(const QString& table, const QVariantMap& values, const QVariantMap& where);
    bool remove(const QString& table, const QVariantMap& where);

    qint64 maxId(const QString& table);
    qint64 count(const QString& table, const QString& whereRaw = QString());

private:
    // Single execution core for every write/DDL statement.
    bool runWrite(const QString& sqlText, const char* op, const QString& table);

    Manticore* manticore_;
};

} // namespace rats::data

#endif // RATS_DATA_DATABASE_H
