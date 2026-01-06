#ifndef TRANSLATIONMANAGER_H
#define TRANSLATIONMANAGER_H

#include <QObject>
#include <QCoreApplication>
#include <QTranslator>
#include <QLocale>
#include <QStringList>
#include <QMap>
#include <memory>

/**
 * @brief TranslationManager - Manages application translations
 * 
 * Provides:
 * - Loading and switching between translations
 * - Available languages enumeration
 * - Language change notifications
 * - Support for Qt .qm files
 */
class TranslationManager : public QObject
{
    Q_OBJECT
    
    Q_PROPERTY(QString currentLanguage READ currentLanguage WRITE setLanguage NOTIFY languageChanged)

public:
    struct LanguageInfo {
        QString code;           // Language code (e.g., "en", "ru")
        QString name;           // English name (e.g., "Russian")
        QString nativeName;     // Native name (e.g., "Русский")
        QString flagEmoji;      // Flag emoji for display
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
     * @brief Get list of available languages
     */
    QList<LanguageInfo> availableLanguages() const;
    
    /**
     * @brief Get language codes as string list
     */
    QStringList availableLanguageCodes() const;
    
    /**
     * @brief Get current language code
     */
    QString currentLanguage() const;
    
    /**
     * @brief Get current language info
     */
    LanguageInfo currentLanguageInfo() const;
    
    /**
     * @brief Get language info by code
     */
    LanguageInfo languageInfo(const QString& code) const;
    
    /**
     * @brief Check if language is available
     */
    bool hasLanguage(const QString& code) const;
    
    /**
     * @brief Get system language code
     */
    static QString systemLanguage();

public slots:
    /**
     * @brief Set current language
     * @param code Language code (e.g., "en", "ru", "de", "es")
     * @return true if language was changed successfully
     */
    bool setLanguage(const QString& code);

signals:
    /**
     * @brief Emitted when language changes
     * @param code New language code
     */
    void languageChanged(const QString& code);
    
    /**
     * @brief Emitted before language change to allow UI updates
     */
    void languageAboutToChange();

private:
    TranslationManager(QObject* parent = nullptr);
    ~TranslationManager() = default;
    
    // Prevent copying
    TranslationManager(const TranslationManager&) = delete;
    TranslationManager& operator=(const TranslationManager&) = delete;
    
    void registerLanguages();
    bool loadTranslation(const QString& code);
    
    QCoreApplication* app_ = nullptr;
    QString translationsPath_;
    QString currentLanguage_;
    
    std::unique_ptr<QTranslator> appTranslator_;
    std::unique_ptr<QTranslator> qtTranslator_;
    
    QMap<QString, LanguageInfo> languages_;
};

#endif // TRANSLATIONMANAGER_H


