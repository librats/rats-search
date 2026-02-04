#ifndef UTILS_H
#define UTILS_H

#include <QObject>
#include <QString>

QString capitalizeFirst(const QString &text) {
    if (text.isEmpty())
        return text;
    QString result = text;
    result[0] = result[0].toUpper();
    return result;
}


#endif // TORRENTSPIDER_H
