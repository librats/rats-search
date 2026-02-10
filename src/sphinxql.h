#ifndef SPHINXQL_H
#define SPHINXQL_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>
#include <QVariantMap>
#include <QVector>
#include <QJsonObject>
#include <memory>
#include <functional>

class ManticoreManager;

/**
 * @brief SphinxQL - SQL query interface for Manticore Search
 * 
 * Provides convenient methods for querying and manipulating data
 * Similar to the mysql.js interface in legacy code
 */
class SphinxQL : public QObject
{
    Q_OBJECT

public:
    using QueryCallback = std::function<void(bool success, const QVector<QVariantMap>& results)>;
    using Row = QVariantMap;
    using Results = QVector<Row>;

    explicit SphinxQL(ManticoreManager* manager, QObject *parent = nullptr);
    ~SphinxQL();

    /**
     * @brief Execute a SQL query with parameters
     * @param sql SQL query string with ? placeholders
     * @param params Query parameters
     * @return Query results as vector of maps
     */
    Results query(const QString& sql, const QVariantList& params = {});

    /**
     * @brief Execute a SQL query asynchronously
     */
    void queryAsync(const QString& sql, const QVariantList& params, QueryCallback callback);

    /**
     * @brief Insert values into a table
     */
    bool insertValues(const QString& table, const QVariantMap& values);

    /**
     * @brief Update values in a table
     */
    bool updateValues(const QString& table, const QVariantMap& values, const QVariantMap& where);

    /**
     * @brief Replace values in a table (INSERT OR REPLACE)
     */
    bool replaceValues(const QString& table, const QVariantMap& values);

    /**
     * @brief Delete from table
     */
    bool deleteFrom(const QString& table, const QVariantMap& where);

    /**
     * @brief Escape a string value for SQL
     */
    QString escape(const QString& value);

    /**
     * @brief Escape a string value for SQL and quote it
     */
    QString escapeString(const QString& value);

    /**
     * @brief Check if connected to database
     */
    bool isConnected() const;

    /**
     * @brief Get last error message
     */
    QString lastError() const { return lastError_; }

    /**
     * @brief Execute raw SQL (no parameter binding)
     */
    bool exec(const QString& sql);

    /**
     * @brief Get maximum ID from a table
     */
    qint64 getMaxId(const QString& table);

    /**
     * @brief Get count from a table
     */
    qint64 getCount(const QString& table, const QString& where = "");

    /**
     * @brief Optimize a table index
     */
    bool optimize(const QString& table);

    /**
     * @brief Flush RAM chunk to disk
     */
    bool flushRamchunk(const QString& table);

signals:
    void queryError(const QString& error);
    void queryCompleted(int affectedRows);

private:
    friend class TestSphinxQL;
    
    QSqlDatabase getDb();
    QVariant convertValue(const QVariant& value);
    
    /**
     * @brief Format a QVariant as a SphinxQL literal (escaped and quoted for strings)
     * 
     * Manticore's SphinxQL does NOT support MySQL prepared statements and uses
     * backslash escaping for quotes (not doubled quotes). We must build raw SQL
     * with properly escaped values instead of using prepare()/addBindValue().
     */
    QString formatSqlValue(const QVariant& value);
    
    /**
     * @brief Replace ? placeholders in SQL with formatted values
     */
    QString buildRawSql(const QString& sql, const QVariantList& params);

    ManticoreManager* manager_;
    QString lastError_;
};

#endif // SPHINXQL_H

