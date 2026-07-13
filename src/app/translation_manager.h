#ifndef RATS_APP_TRANSLATION_MANAGER_H
#define RATS_APP_TRANSLATION_MANAGER_H

#include <QCoreApplication>
#include <QList>
#include <QLocale>
#include <QObject>
#include <QTranslator>
#include <memory>

namespace rats {
namespace app {

/**
 * @brief TranslationManager - Manages application translations
 *
 * Provides:
 * - Loading and switching between translations
 * - Available languages enumeration
 * - Support for Qt .qm files
 *
 * The language a user picked lives in ConfigStore; this only installs the
 * matching translators when Application relays ConfigStore::languageChanged.
 */
class TranslationManager : public QObject {
    Q_OBJECT

public:
    struct LanguageInfo {
        QString code; // Language code (e.g., "en", "ru")
        QString nativeName; // Native name (e.g., "Русский")
        QString flagEmoji; // Flag emoji for display
    };

    /**
     * @brief Get singleton instance
     */
    static TranslationManager& instance();

    /**
     * @brief Initialize translations with application
     * @param app Application to install translators into
     * @param translationsPath Path to translations directory (or use resources)
     */
    void initialize(QCoreApplication* app, const QString& translationsPath = QString());

    /**
     * @brief Get the languages that ship with a compiled .qm (plus built-in
     * English)
     */
    QList<LanguageInfo> availableLanguages() const;

    /**
     * @brief Get system language code
     */
    static QString systemLanguage();

    /**
     * @brief Set current language
     * @param code Language code (e.g., "en", "ru", "de", "es")
     * @return true if language was changed successfully
     */
    bool setLanguage(const QString& code);

private:
    TranslationManager(QObject* parent = nullptr);
    ~TranslationManager() = default;

    // Prevent copying
    TranslationManager(const TranslationManager&) = delete;
    TranslationManager& operator=(const TranslationManager&) = delete;

    /**
     * @brief Check if language is available
     */
    bool hasLanguage(const QString& code) const;

    void registerLanguages();
    bool loadTranslation(const QString& code);

    QCoreApplication* app_ = nullptr;
    QString translationsPath_;
    QString currentLanguage_;

    std::unique_ptr<QTranslator> appTranslator_;
    std::unique_ptr<QTranslator> qtTranslator_;

    // Display order matters (English first), so this is a list, not a map.
    QList<LanguageInfo> languages_;
};

} // namespace app
} // namespace rats

#endif // RATS_APP_TRANSLATION_MANAGER_H
