#include "data/database.h"

#include "data/manticore.h"
#include "data/query.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>

namespace rats::data {

Database::Database(Manticore* manticore, QObject* parent) : QObject(parent), manticore_(manticore) { }

bool Database::isConnected() const
{
    return manticore_ && manticore_->isRunning();
}

Database::Rows Database::query(const QString& sql, const QVariantList& params, bool* ok)
{
    Rows rows;
    if (ok)
        *ok = false;

    QSqlDatabase db = manticore_->getDatabase();
    if (!db.isOpen() && !db.open()) {
        qWarning() << "Database: connection not open:" << db.lastError().text() << "| SQL:" << sql.left(200);
        return rows;
    }

    const QString finalSql = sql::substitute(sql, params);

    QElapsedTimer timer;
    timer.start();
    QSqlQuery q(db);
    if (!q.exec(finalSql)) {
        qWarning() << "Database: query failed:" << q.lastError().text() << "| SQL:" << finalSql.left(200);
        return rows;
    }

    const QSqlRecord record = q.record();
    while (q.next()) {
        Row row;
        for (int i = 0; i < record.count(); ++i)
            row[record.fieldName(i)] = q.value(i);
        rows.append(row);
    }

    if (ok)
        *ok = true;
    qInfo().noquote()
        << QStringLiteral("Database: %1 rows in %2ms | %3").arg(rows.size()).arg(timer.elapsed()).arg(sql.left(120));
    return rows;
}

bool Database::runWrite(const QString& sqlText, const char* op, const QString& table)
{
    QSqlDatabase db = manticore_->getDatabase();
    if (!db.isOpen() && !db.open()) {
        qWarning() << "Database:" << op << "connection not open:" << db.lastError().text();
        return false;
    }

    QElapsedTimer timer;
    timer.start();
    QSqlQuery q(db);
    if (!q.exec(sqlText)) {
        qWarning() << "Database:" << op << "failed:" << q.lastError().text() << "| SQL:" << sqlText.left(200);
        return false;
    }

    qInfo().noquote() << QStringLiteral("Database: %1 %2 | %3ms | affected %4")
                             .arg(QString::fromLatin1(op), table)
                             .arg(timer.elapsed())
                             .arg(q.numRowsAffected());
    return true;
}

bool Database::execute(const QString& sql)
{
    return runWrite(sql, "EXEC", QString());
}

bool Database::insert(const QString& table, const QVariantMap& values)
{
    QStringList columns;
    QStringList literals;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        columns << it.key();
        literals << sql::formatValue(it.value());
    }
    const QString stmt = QStringLiteral("INSERT INTO %1 (%2) VALUES (%3)")
                             .arg(table, columns.join(QLatin1String(", ")), literals.join(QLatin1String(", ")));
    return runWrite(stmt, "INSERT", table);
}

bool Database::replace(const QString& table, const QVariantMap& values)
{
    QStringList columns;
    QStringList literals;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        columns << it.key();
        literals << sql::formatValue(it.value());
    }
    const QString stmt = QStringLiteral("REPLACE INTO %1 (%2) VALUES (%3)")
                             .arg(table, columns.join(QLatin1String(", ")), literals.join(QLatin1String(", ")));
    return runWrite(stmt, "REPLACE", table);
}

bool Database::update(const QString& table, const QVariantMap& values, const QVariantMap& where)
{
    QStringList setParts;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it)
        setParts << QStringLiteral("%1 = %2").arg(it.key(), sql::formatValue(it.value()));

    QStringList whereParts;
    for (auto it = where.constBegin(); it != where.constEnd(); ++it)
        whereParts << QStringLiteral("%1 = %2").arg(it.key(), sql::formatValue(it.value()));

    const QString stmt = QStringLiteral("UPDATE %1 SET %2 WHERE %3")
                             .arg(table, setParts.join(QLatin1String(", ")), whereParts.join(QLatin1String(" AND ")));
    return runWrite(stmt, "UPDATE", table);
}

bool Database::remove(const QString& table, const QVariantMap& where)
{
    QStringList whereParts;
    for (auto it = where.constBegin(); it != where.constEnd(); ++it)
        whereParts << QStringLiteral("%1 = %2").arg(it.key(), sql::formatValue(it.value()));

    const QString stmt = QStringLiteral("DELETE FROM %1 WHERE %2").arg(table, whereParts.join(QLatin1String(" AND ")));
    return runWrite(stmt, "DELETE", table);
}

qint64 Database::maxId(const QString& table)
{
    const Rows rows = query(QStringLiteral("SELECT MAX(id) AS maxid FROM %1").arg(table));
    return rows.isEmpty() ? 0 : rows.first().value(QStringLiteral("maxid")).toLongLong();
}

qint64 Database::count(const QString& table, const QString& whereRaw)
{
    QString stmt = QStringLiteral("SELECT COUNT(*) AS cnt FROM %1").arg(table);
    if (!whereRaw.isEmpty())
        stmt += QStringLiteral(" WHERE ") + whereRaw;
    const Rows rows = query(stmt);
    return rows.isEmpty() ? 0 : rows.first().value(QStringLiteral("cnt")).toLongLong();
}

} // namespace rats::data
