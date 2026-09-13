#ifndef RATS_UI_THEME_H
#define RATS_UI_THEME_H

#include <QColor>
#include <QHash>
#include <QString>

namespace rats::ui {

/**
 * @brief The single source of truth for every colour in the interface.
 *
 * There is one stylesheet template (`:/styles/styles/theme.qss`) written
 * against `@token` names, and one token table per theme
 * (`:/styles/styles/{light,dark}.json`). Theme substitutes the second into the
 * first, which is why a layout tweak can no longer land in one theme and be
 * forgotten in the other.
 *
 * Widgets that paint themselves (TorrentItemDelegate) read the same tokens
 * through color(), so they never drift from the sheet.
 *
 * Not thread-safe: it is UI state and is only touched from the GUI thread.
 */
class Theme {
public:
    static Theme& instance();

    /** Loads the token table for @p dark. Cheap to call again with the same value. */
    void setDark(bool dark);
    bool isDark() const { return dark_; }

    /** The expanded stylesheet for the current theme. */
    QString styleSheet() const;

    /**
     * Token value as a colour. An unknown or non-colour token returns @p fallback
     * and logs once — a typo shows up in the log instead of silently painting black.
     */
    QColor color(QLatin1String token, const QColor& fallback = QColor(Qt::magenta)) const;

    /** Raw token value (colours, icon paths, font stacks). Empty when unknown. */
    QString value(QLatin1String token) const;

private:
    Theme() = default;

    void load(bool dark);

    bool loaded_ = false;
    bool dark_ = false;
    QHash<QString, QString> tokens_;
    QString styleSheet_;
};

} // namespace rats::ui

#endif // RATS_UI_THEME_H
