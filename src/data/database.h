#ifndef RATS_DATA_DATABASE_H
#define RATS_DATA_DATABASE_H

#include <QObject>
#include <QVariantMap>
#include <QVector>

namespace rats::data {

class Manticore;

// Thin synchronous execution layer over the Manticore connection. Replaces the
// old SphinxQL: the five write helpers (insert/replace/update/remove/execute)
// now share a single logged execution core instead of copy-pasting the
// open-db/exec/log/error block, and there is no queryAsync — asynchrony is a
// service-layer concern, which removes the old cross-thread `lastError_` race.
//
// Not thread-safe by itself, but safe to use from multiple threads because the
// underlying connection is per-thread; per-call error status is returned through
// the `ok` out-parameter / bool return rather than shared state.
class Database : public QObject {
    Q_OBJECT

public:
    using Row = QVariantMap;
    using Rows = QVector<Row>;

    explicit Database(Manticore* manticore, QObject* parent = nullptr);

    bool isConnected() const;

    // Run a SELECT (or any row-returning statement). `?` placeholders in `sql`
    // are substituted with escaped `params`. On error the result is empty and
    // *ok (if provided) is set to false.
    Rows query(const QString& sql, const QVariantList& params = {}, bool* ok = nullptr);

    // Run a statement with no result set (OPTIMIZE, FLUSH, ...).
    bool execute(const QString& sql);

    bool insert(const QString& table, const QVariantMap& values);
    bool replace(const QString& table, const QVariantMap& values);
    bool update(const QString& table, const QVariantMap& values, const QVariantMap& where);
    bool remove(const QString& table, const QVariantMap& where);

    qint64 maxId(const QString& table);
    qint64 count(const QString& table, const QString& whereRaw = QString());

    bool optimize(const QString& table);
    bool flushRamchunk(const QString& table);

signals:
    void queryError(const QString& error);

private:
    // Single execution core for every write/DDL statement.
    bool runWrite(const QString& sql, const char* op, const QString& table);

    Manticore* manticore_;
};

} // namespace rats::data

#endif // RATS_DATA_DATABASE_H
