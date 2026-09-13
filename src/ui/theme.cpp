#include "theme.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QRegularExpression>

namespace rats::ui {

namespace {

QString readResource(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Theme: cannot read" << path << file.errorString();
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

/**
 * Replaces every `@token` in @p tmpl with its value. Anything unknown is left
 * verbatim and reported: Qt would otherwise drop the whole declaration without
 * a word, which is a miserable thing to debug from a screenshot.
 */
QString expand(const QString& tmpl, const QHash<QString, QString>& tokens)
{
    static const QRegularExpression placeholder(QStringLiteral("@([A-Za-z][A-Za-z0-9_]*)"));

    QString out;
    out.reserve(tmpl.size() + tmpl.size() / 4);

    qsizetype copied = 0;
    auto it = placeholder.globalMatch(tmpl);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const QString name = match.captured(1);
        const auto token = tokens.constFind(name);
        if (token == tokens.constEnd()) {
            qWarning() << "Theme: unknown token @" << name << "in theme.qss";
            continue; // leave it in place, copied with the surrounding text
        }
        out += QStringView(tmpl).mid(copied, match.capturedStart() - copied);
        out += *token;
        copied = match.capturedEnd();
    }
    out += QStringView(tmpl).mid(copied);
    return out;
}

} // namespace

Theme& Theme::instance()
{
    static Theme theme;
    return theme;
}

void Theme::setDark(bool dark)
{
    if (loaded_ && dark_ == dark) {
        return;
    }
    load(dark);
}

void Theme::load(bool dark)
{
    dark_ = dark;
    loaded_ = true;
    tokens_.clear();

    const QString tokenPath
        = dark ? QStringLiteral(":/styles/styles/dark.json") : QStringLiteral(":/styles/styles/light.json");

    QJsonParseError error {};
    const QJsonDocument doc = QJsonDocument::fromJson(readResource(tokenPath).toUtf8(), &error);
    if (!doc.isObject()) {
        qWarning() << "Theme: bad token file" << tokenPath << error.errorString();
    } else {
        const QJsonObject object = doc.object();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            if (it.key().startsWith(QLatin1Char('_'))) {
                continue; // documentation keys
            }
            tokens_.insert(it.key(), it.value().toString());
        }
    }

    styleSheet_ = expand(readResource(QStringLiteral(":/styles/styles/theme.qss")), tokens_);
    qInfo() << (dark ? "Dark" : "Light") << "theme loaded:" << tokens_.size() << "tokens";
}

QString Theme::styleSheet() const
{
    if (!loaded_) {
        const_cast<Theme*>(this)->load(dark_);
    }
    return styleSheet_;
}

QString Theme::value(QLatin1String token) const
{
    if (!loaded_) {
        const_cast<Theme*>(this)->load(dark_);
    }
    return tokens_.value(QString(token));
}

QColor Theme::color(QLatin1String token, const QColor& fallback) const
{
    const QString raw = value(token);
    const QColor color(raw);
    if (!color.isValid()) {
        qWarning() << "Theme: token" << token << "is not a colour:" << raw;
        return fallback;
    }
    return color;
}

} // namespace rats::ui
