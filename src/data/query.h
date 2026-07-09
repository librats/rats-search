#ifndef RATS_DATA_QUERY_H
#define RATS_DATA_QUERY_H

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantList>

// SphinxQL query construction for Manticore.
//
// Manticore does NOT support MySQL prepared statements and its driver mangles
// the standard '' quote-doubling, so every value must be escaped by hand with
// backslash notation and spliced into raw SQL. This module is the ONE place
// that does that escaping: hand-quoted IN clauses, interpolated ORDER BY and
// unescaped MATCH input are all injection vectors, so every value must go
// through here.
namespace rats::data::sql {

// Backslash-escape a value for inclusion inside a single-quoted SQL string.
QString escape(const QString& value);

// escape() wrapped in single quotes: 'escaped value'.
QString quote(const QString& value);

// Format a QVariant as a SQL literal: numbers unquoted, JSON compacted to a
// quoted string, everything else escaped+quoted. NULL becomes '' (Manticore has
// no real NULL).
QString formatValue(const QVariant& value);

// Escape the full-text extended-query operators so arbitrary user input can be
// placed inside MATCH('...') without causing a query-syntax error. Wildcards
// (*) are deliberately left intact so prefix search keeps working. The result
// is still a plain string to be handed to formatValue()/quote().
QString escapeMatch(const QString& userQuery);

// Replace each '?' placeholder in `sql` with the formatValue() of the next
// param. This is the raw-SQL equivalent of bound parameters.
QString substitute(const QString& sql, const QVariantList& params);

// True if `identifier` is a safe bare SQL identifier (column/table name):
// starts with a letter/underscore, then letters/digits/underscores only. Used
// to validate ORDER BY columns before they are spliced into SQL.
bool isIdentifier(const QString& identifier);

} // namespace rats::data::sql

namespace rats::data {

// A small fluent builder for SELECT statements with safe WHERE / IN / ORDER BY
// / LIMIT. Numeric conditions are added via whereRaw() by trusted callers; all
// string values go through the escaping above. ORDER BY columns are validated
// as identifiers, closing the old `ORDER BY %1` injection hole.
class SelectQuery {
public:
    explicit SelectQuery(QString table);

    SelectQuery& columns(const QString& expr); // default "*"
    SelectQuery& matchAgainst(const QString& userQuery); // WHERE MATCH('escaped')
    SelectQuery& whereEq(const QString& column, const QVariant& value);
    SelectQuery& whereIn(const QString& column, const QStringList& values);
    SelectQuery& whereRaw(const QString& condition); // trusted numeric fragment
    SelectQuery& orderBy(const QString& column, bool descending);
    SelectQuery& limit(int offset, int count);

    QString build() const;

private:
    QString table_;
    QString columns_ = QStringLiteral("*");
    QStringList conditions_;
    QString orderClause_;
    QString limitClause_;
};

} // namespace rats::data

#endif // RATS_DATA_QUERY_H
