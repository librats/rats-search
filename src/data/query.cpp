#include "data/query.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>

namespace rats::data::sql {

QString escape(const QString& value)
{
    QString result = value;
    result.replace('\\', QLatin1String("\\\\"));
    result.replace('\'', QLatin1String("\\'"));
    result.replace('"', QLatin1String("\\\""));
    result.replace('\n', QLatin1String("\\n"));
    result.replace('\r', QLatin1String("\\r"));
    result.replace('\t', QLatin1String("\\t"));
    result.replace(QChar('\0'), QLatin1String("\\0"));
    return result;
}

QString quote(const QString& value)
{
    return QLatin1Char('\'') + escape(value) + QLatin1Char('\'');
}

static QString jsonToCompactString(const QVariant& value)
{
    if (value.typeId() == QMetaType::QJsonObject)
        return QString::fromUtf8(QJsonDocument(value.toJsonObject()).toJson(QJsonDocument::Compact));
    if (value.typeId() == QMetaType::QJsonArray)
        return QString::fromUtf8(QJsonDocument(value.toJsonArray()).toJson(QJsonDocument::Compact));
    if (value.typeId() == QMetaType::QVariantMap)
        return QString::fromUtf8(
            QJsonDocument(QJsonObject::fromVariantMap(value.toMap())).toJson(QJsonDocument::Compact));
    return QString();
}

QString formatValue(const QVariant& value)
{
    if (value.isNull() || !value.isValid())
        return QStringLiteral("''"); // Manticore has no NULL

    switch (value.typeId()) {
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::Long:
    case QMetaType::ULong:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::Double:
    case QMetaType::Float:
        return value.toString();
    case QMetaType::QJsonObject:
    case QMetaType::QJsonArray:
    case QMetaType::QVariantMap:
        return quote(jsonToCompactString(value));
    default:
        return quote(value.toString());
    }
}

QString escapeMatch(const QString& userQuery)
{
    // Manticore extended full-text operators. '*' is intentionally excluded so
    // prefix/wildcard search keeps working. Backslash is escaped first so we do
    // not double-escape the escapes we add.
    static const QString specials = QStringLiteral("\\!\"$()-/<@^|~");
    QString result;
    result.reserve(userQuery.size() + 8);
    for (QChar c : userQuery) {
        if (specials.contains(c))
            result += QLatin1Char('\\');
        result += c;
    }
    return result;
}

QString substitute(const QString& sql, const QVariantList& params)
{
    if (params.isEmpty())
        return sql;

    QString result;
    result.reserve(sql.size() + params.size() * 32);
    int paramIndex = 0;
    for (QChar c : sql) {
        if (c == QLatin1Char('?') && paramIndex < params.size())
            result += formatValue(params[paramIndex++]);
        else
            result += c;
    }
    return result;
}

bool isIdentifier(const QString& identifier)
{
    if (identifier.isEmpty())
        return false;
    const QChar first = identifier.at(0);
    if (!first.isLetter() && first != QLatin1Char('_'))
        return false;
    for (QChar c : identifier) {
        if (!c.isLetterOrNumber() && c != QLatin1Char('_'))
            return false;
    }
    return true;
}

} // namespace rats::data::sql

namespace rats::data {

SelectQuery::SelectQuery(QString table) : table_(std::move(table)) { }

SelectQuery& SelectQuery::columns(const QString& expr)
{
    columns_ = expr;
    return *this;
}

SelectQuery& SelectQuery::matchAgainst(const QString& userQuery)
{
    conditions_ << QStringLiteral("MATCH(%1)").arg(sql::quote(sql::escapeMatch(userQuery)));
    return *this;
}

SelectQuery& SelectQuery::whereEq(const QString& column, const QVariant& value)
{
    conditions_ << QStringLiteral("%1 = %2").arg(column, sql::formatValue(value));
    return *this;
}

SelectQuery& SelectQuery::whereIn(const QString& column, const QStringList& values)
{
    if (values.isEmpty()) {
        conditions_ << QStringLiteral("1 = 0"); // empty IN matches nothing
        return *this;
    }
    QStringList quoted;
    quoted.reserve(values.size());
    for (const QString& v : values)
        quoted << sql::quote(v);
    conditions_ << QStringLiteral("%1 IN (%2)").arg(column, quoted.join(QLatin1String(", ")));
    return *this;
}

SelectQuery& SelectQuery::whereRaw(const QString& condition)
{
    if (!condition.isEmpty())
        conditions_ << condition;
    return *this;
}

SelectQuery& SelectQuery::orderBy(const QString& column, bool descending)
{
    // Defence in depth: silently ignore a column that is not a bare identifier,
    // so a caller mistake can never turn into SQL injection.
    if (sql::isIdentifier(column))
        orderClause_
            = QStringLiteral(" ORDER BY %1 %2").arg(column, descending ? QLatin1String("DESC") : QLatin1String("ASC"));
    return *this;
}

SelectQuery& SelectQuery::limit(int offset, int count)
{
    limitClause_ = QStringLiteral(" LIMIT %1,%2").arg(offset).arg(count);
    return *this;
}

QString SelectQuery::build() const
{
    QString sql = QStringLiteral("SELECT %1 FROM %2").arg(columns_, table_);
    if (!conditions_.isEmpty())
        sql += QStringLiteral(" WHERE ") + conditions_.join(QLatin1String(" AND "));
    sql += orderClause_;
    sql += limitClause_;
    return sql;
}

} // namespace rats::data
