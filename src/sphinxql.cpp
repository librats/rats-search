#include "sphinxql.h"
#include "manticoremanager.h"
#include <QSqlError>
#include <QSqlRecord>
#include <QDebug>
#include <QJsonDocument>
#include <QtConcurrent>
#include <QRegularExpression>
#include <QElapsedTimer>

SphinxQL::SphinxQL(ManticoreManager* manager, QObject *parent)
    : QObject(parent)
    , manager_(manager)
{
}

SphinxQL::~SphinxQL()
{
}

QSqlDatabase SphinxQL::getDb()
{
    return manager_->getDatabase();
}

SphinxQL::Results SphinxQL::query(const QString& sql, const QVariantList& params)
{
    Results results;
    QElapsedTimer timer;
    timer.start();
    
    QSqlDatabase db = getDb();
    if (!db.isOpen() && !db.open()) {
        lastError_ = db.lastError().text();
        emit queryError(lastError_);
        qWarning() << "SphinxQL: Database not open:" << lastError_ << "| SQL:" << sql << "| params:" << params;
        return results;
    }
    
    QSqlQuery q(db);
    
    // Prepare and bind parameters
    if (!params.isEmpty()) {
        q.prepare(sql);
        for (int i = 0; i < params.size(); ++i) {
            q.addBindValue(convertValue(params[i]));
        }
        
        if (!q.exec()) {
            lastError_ = q.lastError().text();
            emit queryError(lastError_);
            qWarning() << "SphinxQL error:" << lastError_ << "| SQL:" << sql << "| params:" << params;
            return results;
        }
    } else {
        if (!q.exec(sql)) {
            lastError_ = q.lastError().text();
            emit queryError(lastError_);
            qWarning() << "SphinxQL error:" << lastError_ << "| SQL:" << sql;
            return results;
        }
    }
    
    qint64 execTime = timer.elapsed();
    
    // Fetch results
    QSqlRecord record = q.record();
    while (q.next()) {
        Row row;
        for (int i = 0; i < record.count(); ++i) {
            QString fieldName = record.fieldName(i);
            row[fieldName] = q.value(i);
        }
        results.append(row);
    }
    
    qint64 totalTime = timer.elapsed();
    qInfo() << "SphinxQL:" << sql
            << (params.isEmpty() ? "" : QString("| params: %1").arg(params.size()))
            << "| exec:" << execTime << "ms | fetch:" << (totalTime - execTime) << "ms | rows:" << results.size();
    
    emit queryCompleted(q.numRowsAffected());
    return results;
}

void SphinxQL::queryAsync(const QString& sql, const QVariantList& params, QueryCallback callback)
{
    (void)QtConcurrent::run([this, sql, params, callback]() {
        Results results = query(sql, params);
        if (callback) {
            callback(lastError_.isEmpty(), results);
        }
    });
}

bool SphinxQL::insertValues(const QString& table, const QVariantMap& values)
{
    QElapsedTimer timer;
    timer.start();
    
    QString sql = buildInsertSql(table, values, false);
    
    QVariantList params;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        params << convertValue(it.value());
    }
    
    QSqlDatabase db = getDb();
    if (!db.isOpen() && !db.open()) {
        lastError_ = db.lastError().text();
        emit queryError(lastError_);
        qWarning() << "SphinxQL insert: Database not open:" << lastError_ << "(took" << timer.elapsed() << "ms)";
        return false;
    }
    
    QSqlQuery q(db);
    q.prepare(sql);
    for (const QVariant& param : params) {
        q.addBindValue(param);
    }
    
    if (!q.exec()) {
        lastError_ = q.lastError().text();
        emit queryError(lastError_);
        qWarning() << "SphinxQL insert error:" << lastError_ << "SQL:" << sql << "(took" << timer.elapsed() << "ms)";
        return false;
    }
    
    qInfo() << "SphinxQL INSERT into" << table << "| time:" << timer.elapsed() << "ms";
    
    emit queryCompleted(q.numRowsAffected());
    return true;
}

bool SphinxQL::replaceValues(const QString& table, const QVariantMap& values)
{
    QElapsedTimer timer;
    timer.start();
    
    QString sql = buildInsertSql(table, values, true);
    
    QVariantList params;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        params << convertValue(it.value());
    }
    
    QSqlDatabase db = getDb();
    if (!db.isOpen() && !db.open()) {
        lastError_ = db.lastError().text();
        emit queryError(lastError_);
        qWarning() << "SphinxQL replace: Database not open:" << lastError_ << "(took" << timer.elapsed() << "ms)";
        return false;
    }
    
    QSqlQuery q(db);
    q.prepare(sql);
    for (const QVariant& param : params) {
        q.addBindValue(param);
    }
    
    if (!q.exec()) {
        lastError_ = q.lastError().text();
        emit queryError(lastError_);
        qWarning() << "SphinxQL replace error:" << lastError_ << "SQL:" << sql << "(took" << timer.elapsed() << "ms)";
        return false;
    }
    
    qInfo() << "SphinxQL REPLACE into" << table << "| time:" << timer.elapsed() << "ms";
    
    emit queryCompleted(q.numRowsAffected());
    return true;
}

bool SphinxQL::updateValues(const QString& table, const QVariantMap& values, const QVariantMap& where)
{
    QElapsedTimer timer;
    timer.start();
    
    QString sql = buildUpdateSql(table, values, where);
    
    QVariantList params;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        params << convertValue(it.value());
    }
    for (auto it = where.constBegin(); it != where.constEnd(); ++it) {
        params << convertValue(it.value());
    }
    
    QSqlDatabase db = getDb();
    if (!db.isOpen() && !db.open()) {
        lastError_ = db.lastError().text();
        emit queryError(lastError_);
        qWarning() << "SphinxQL update: Database not open:" << lastError_ << "(took" << timer.elapsed() << "ms)";
        return false;
    }
    
    QSqlQuery q(db);
    q.prepare(sql);
    for (const QVariant& param : params) {
        q.addBindValue(param);
    }
    
    if (!q.exec()) {
        lastError_ = q.lastError().text();
        emit queryError(lastError_);
        qWarning() << "SphinxQL update error:" << lastError_ << "SQL:" << sql << "(took" << timer.elapsed() << "ms)";
        return false;
    }
    
    qInfo() << "SphinxQL UPDATE" << table << "| time:" << timer.elapsed() << "ms | affected:" << q.numRowsAffected();
    
    emit queryCompleted(q.numRowsAffected());
    return true;
}

bool SphinxQL::deleteFrom(const QString& table, const QVariantMap& where)
{
    QElapsedTimer timer;
    timer.start();
    
    QString whereSql = buildWhereSql(where);
    QString sql = QString("DELETE FROM %1 WHERE %2").arg(table, whereSql);
    
    QVariantList params;
    for (auto it = where.constBegin(); it != where.constEnd(); ++it) {
        params << convertValue(it.value());
    }
    
    QSqlDatabase db = getDb();
    if (!db.isOpen() && !db.open()) {
        lastError_ = db.lastError().text();
        emit queryError(lastError_);
        qWarning() << "SphinxQL delete: Database not open:" << lastError_ << "(took" << timer.elapsed() << "ms)";
        return false;
    }
    
    QSqlQuery q(db);
    q.prepare(sql);
    for (const QVariant& param : params) {
        q.addBindValue(param);
    }
    
    if (!q.exec()) {
        lastError_ = q.lastError().text();
        emit queryError(lastError_);
        qWarning() << "SphinxQL delete error:" << lastError_ << "SQL:" << sql << "(took" << timer.elapsed() << "ms)";
        return false;
    }
    
    qInfo() << "SphinxQL DELETE from" << table << "| time:" << timer.elapsed() << "ms | affected:" << q.numRowsAffected();
    
    emit queryCompleted(q.numRowsAffected());
    return true;
}

QString SphinxQL::escape(const QString& value)
{
    QString result = value;
    result.replace("\\", "\\\\");
    result.replace("'", "\\'");
    result.replace("\"", "\\\"");
    result.replace("\n", "\\n");
    result.replace("\r", "\\r");
    result.replace("\t", "\\t");
    // Note: Null character escaping requires QChar, not C-string literal
    result.replace(QChar('\0'), QString("\\0"));
    return result;
}

QString SphinxQL::escapeString(const QString& value)
{
    return "'" + escape(value) + "'";
}

bool SphinxQL::isConnected() const
{
    return manager_->isRunning();
}

bool SphinxQL::exec(const QString& sql)
{
    QElapsedTimer timer;
    timer.start();
    
    QSqlDatabase db = getDb();
    if (!db.isOpen() && !db.open()) {
        lastError_ = db.lastError().text();
        emit queryError(lastError_);
        qWarning() << "SphinxQL exec: Database not open:" << lastError_ << "(took" << timer.elapsed() << "ms)";
        return false;
    }
    
    QSqlQuery q(db);
    if (!q.exec(sql)) {
        lastError_ = q.lastError().text();
        emit queryError(lastError_);
        qWarning() << "SphinxQL exec error:" << lastError_ << "SQL:" << sql << "(took" << timer.elapsed() << "ms)";
        return false;
    }
    
    qInfo() << "SphinxQL exec:" << sql
            << "| time:" << timer.elapsed() << "ms | affected:" << q.numRowsAffected();
    
    emit queryCompleted(q.numRowsAffected());
    return true;
}

qint64 SphinxQL::getMaxId(const QString& table)
{
    Results results = query(QString("SELECT MAX(id) as maxid FROM %1").arg(table));
    if (!results.isEmpty() && results[0].contains("maxid")) {
        return results[0]["maxid"].toLongLong();
    }
    return 0;
}

qint64 SphinxQL::getCount(const QString& table, const QString& where)
{
    QString sql = QString("SELECT COUNT(*) as cnt FROM %1").arg(table);
    if (!where.isEmpty()) {
        sql += " WHERE " + where;
    }
    
    Results results = query(sql);
    if (!results.isEmpty() && results[0].contains("cnt")) {
        return results[0]["cnt"].toLongLong();
    }
    return 0;
}

bool SphinxQL::optimize(const QString& table)
{
    return exec(QString("OPTIMIZE INDEX %1").arg(table));
}

bool SphinxQL::flushRamchunk(const QString& table)
{
    return exec(QString("FLUSH RAMCHUNK %1").arg(table));
}

QString SphinxQL::buildInsertSql(const QString& table, const QVariantMap& values, bool replace)
{
    QStringList columns;
    QStringList placeholders;
    
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        columns << it.key();
        placeholders << "?";
    }
    
    QString verb = replace ? "REPLACE" : "INSERT";
    return QString("%1 INTO %2 (%3) VALUES (%4)")
        .arg(verb, table, columns.join(", "), placeholders.join(", "));
}

QString SphinxQL::buildUpdateSql(const QString& table, const QVariantMap& values, const QVariantMap& where)
{
    QStringList setParts;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        setParts << QString("%1 = ?").arg(it.key());
    }
    
    QString whereSql = buildWhereSql(where);
    
    return QString("UPDATE %1 SET %2 WHERE %3")
        .arg(table, setParts.join(", "), whereSql);
}

QString SphinxQL::buildWhereSql(const QVariantMap& where)
{
    QStringList parts;
    for (auto it = where.constBegin(); it != where.constEnd(); ++it) {
        parts << QString("%1 = ?").arg(it.key());
    }
    return parts.join(" AND ");
}

QVariant SphinxQL::convertValue(const QVariant& value)
{
    // Handle null/invalid values - Manticore doesn't support NULL, use defaults
    if (value.isNull() || !value.isValid()) {
        return QString("");  // Return empty string instead of NULL
    }
    
    // Convert QJsonObject/QJsonArray to string
    if (value.typeId() == QMetaType::QJsonObject) {
        return QString::fromUtf8(QJsonDocument(value.toJsonObject()).toJson(QJsonDocument::Compact));
    }
    if (value.typeId() == QMetaType::QJsonArray) {
        return QString::fromUtf8(QJsonDocument(value.toJsonArray()).toJson(QJsonDocument::Compact));
    }
    if (value.typeId() == QMetaType::QVariantMap) {
        return QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(value.toMap())).toJson(QJsonDocument::Compact));
    }
    
    // Ensure QString values are not treated as NULL
    if (value.typeId() == QMetaType::QString) {
        return value.toString();  // Force conversion to non-null QString
    }
    
    return value;
}

