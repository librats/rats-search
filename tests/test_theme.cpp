#include "ui/theme.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QtTest>

using rats::ui::Theme;

/**
 * The two themes share one stylesheet, so nothing here checks how anything
 * looks — it checks the two halves still fit together: every placeholder the
 * sheet spells has a value, both token tables carry the same keys, and the
 * expansion actually happens.
 *
 * The failure these guard against is silent: Qt drops a declaration containing
 * an unresolved `@name` without a word, so a missing token shows up as a widget
 * that quietly lost its colour in one theme only.
 */
class TestTheme : public QObject {
    Q_OBJECT

private slots:
    void bothThemesDefineTheSameTokens();
    void everyPlaceholderResolves_data();
    void everyPlaceholderResolves();
    void styleSheetIsExpanded_data();
    void styleSheetIsExpanded();
    void tokensReadAsColours_data();
    void tokensReadAsColours();
    void switchingThemeChangesTheSheet();

private:
    static QSet<QString> tokenKeys(const QString& path);
    static QString resource(const QString& path);
};

QString TestTheme::resource(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

QSet<QString> TestTheme::tokenKeys(const QString& path)
{
    const QJsonObject object = QJsonDocument::fromJson(resource(path).toUtf8()).object();
    QSet<QString> keys;
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if (!it.key().startsWith(QLatin1Char('_'))) {
            keys.insert(it.key());
        }
    }
    return keys;
}

void TestTheme::bothThemesDefineTheSameTokens()
{
    const QSet<QString> light = tokenKeys(QStringLiteral(":/styles/styles/light.json"));
    const QSet<QString> dark = tokenKeys(QStringLiteral(":/styles/styles/dark.json"));

    QVERIFY(!light.isEmpty());

    QStringList onlyLight = QStringList(QList<QString>((light - dark).values()));
    QStringList onlyDark = QStringList(QList<QString>((dark - light).values()));
    onlyLight.sort();
    onlyDark.sort();
    QVERIFY2(onlyLight.isEmpty(), qPrintable(QStringLiteral("only in light.json: ") + onlyLight.join(", ")));
    QVERIFY2(onlyDark.isEmpty(), qPrintable(QStringLiteral("only in dark.json: ") + onlyDark.join(", ")));
}

void TestTheme::everyPlaceholderResolves_data()
{
    QTest::addColumn<bool>("dark");
    QTest::newRow("light") << false;
    QTest::newRow("dark") << true;
}

void TestTheme::everyPlaceholderResolves()
{
    QFETCH(bool, dark);

    const QString tmpl = resource(QStringLiteral(":/styles/styles/theme.qss"));
    QVERIFY(!tmpl.isEmpty());

    const QSet<QString> keys
        = tokenKeys(dark ? QStringLiteral(":/styles/styles/dark.json") : QStringLiteral(":/styles/styles/light.json"));

    static const QRegularExpression placeholder(QStringLiteral("@([A-Za-z][A-Za-z0-9_]*)"));
    QStringList missing;
    auto it = placeholder.globalMatch(tmpl);
    while (it.hasNext()) {
        const QString name = it.next().captured(1);
        if (!keys.contains(name) && !missing.contains(name)) {
            missing << name;
        }
    }
    QVERIFY2(missing.isEmpty(), qPrintable(QStringLiteral("theme.qss uses undefined tokens: ") + missing.join(", ")));
}

void TestTheme::styleSheetIsExpanded_data()
{
    everyPlaceholderResolves_data();
}

void TestTheme::styleSheetIsExpanded()
{
    QFETCH(bool, dark);

    Theme& theme = Theme::instance();
    theme.setDark(dark);

    QString sheet = theme.styleSheet();
    QVERIFY(sheet.contains(QStringLiteral("QPushButton")));

    // Comments are the only place an at-name may survive expansion.
    static const QRegularExpression comment(
        QStringLiteral("/\\*.*?\\*/"), QRegularExpression::DotMatchesEverythingOption);
    sheet.remove(comment);
    QVERIFY2(!sheet.contains(QLatin1Char('@')), "stylesheet still carries an unresolved placeholder");
}

void TestTheme::tokensReadAsColours_data()
{
    everyPlaceholderResolves_data();
}

void TestTheme::tokensReadAsColours()
{
    QFETCH(bool, dark);

    Theme& theme = Theme::instance();
    theme.setDark(dark);
    QCOMPARE(theme.isDark(), dark);

    // The delegate paints straight from these; a typo would show up as magenta.
    for (const char* token :
        { "text", "textMuted", "textFaint", "textOnAccent", "surface", "surfaceAlt", "accent", "rowHover", "rowBorder",
            "remoteRow", "remoteRowAlt", "remoteStripe", "pathText", "matchHighlight", "matchHighlightSelected",
            "seedersHigh", "seedersMid", "seedersLow", "leechersHigh", "leechersMid", "leechersLow", "peersNone" }) {
        const QColor color = theme.color(QLatin1String(token), QColor());
        QVERIFY2(color.isValid(), token);
    }
}

void TestTheme::switchingThemeChangesTheSheet()
{
    Theme& theme = Theme::instance();

    theme.setDark(false);
    const QString light = theme.styleSheet();
    const QColor lightSurface = theme.color(QLatin1String("surface"));

    theme.setDark(true);
    const QString dark = theme.styleSheet();
    const QColor darkSurface = theme.color(QLatin1String("surface"));

    QVERIFY(light != dark);
    QVERIFY(lightSurface != darkSurface);
    QVERIFY(lightSurface.lightness() > darkSurface.lightness());
}

QTEST_MAIN(TestTheme)
#include "test_theme.moc"
