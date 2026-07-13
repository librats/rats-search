#include "app/translation_manager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QLibraryInfo>
#include <algorithm>

namespace rats {
namespace app {

TranslationManager& TranslationManager::instance()
{
    static TranslationManager instance;
    return instance;
}

TranslationManager::TranslationManager(QObject* parent)
    : QObject(parent), appTranslator_(std::make_unique<QTranslator>()), qtTranslator_(std::make_unique<QTranslator>())
{
    registerLanguages();
}

void TranslationManager::registerLanguages()
{
    // English is built into the sources; the rest must have a .qm in
    // translations/.
    languages_ = { { "en", "English", "🇬🇧" }, { "ru", "Русский", "🇷🇺" }, { "de", "Deutsch", "🇩🇪" },
        { "es", "Español", "🇪🇸" }, { "fr", "Français", "🇫🇷" } };
}

void TranslationManager::initialize(QCoreApplication* app, const QString& translationsPath)
{
    app_ = app;

    // Determine translations path
    if (translationsPath.isEmpty()) {
        // Try resource path first
        translationsPath_ = ":/translations";
    } else {
        translationsPath_ = translationsPath;
    }

    // Default to English
    currentLanguage_ = "en";

    qInfo() << "TranslationManager initialized, translations path:" << translationsPath_;
}

QList<TranslationManager::LanguageInfo> TranslationManager::availableLanguages() const
{
    return languages_;
}

bool TranslationManager::hasLanguage(const QString& code) const
{
    return std::any_of(
        languages_.cbegin(), languages_.cend(), [&code](const LanguageInfo& l) { return l.code == code; });
}

QString TranslationManager::systemLanguage()
{
    return QLocale::system().name().left(2);
}

bool TranslationManager::setLanguage(const QString& code)
{
    if (code == currentLanguage_) {
        return true; // Already set
    }

    if (!hasLanguage(code)) {
        qWarning() << "Language not available:" << code;
        return false;
    }

    if (!loadTranslation(code)) {
        qWarning() << "Failed to load translation for:" << code;
        // Fall back to English
        if (code != "en") {
            loadTranslation("en");
            currentLanguage_ = "en";
        }
        return false;
    }

    currentLanguage_ = code;
    qInfo() << "Language changed to:" << code;
    return true;
}

bool TranslationManager::loadTranslation(const QString& code)
{
    if (!app_) {
        qWarning() << "TranslationManager not initialized with application";
        return false;
    }

    // Remove previous translators
    if (!appTranslator_->isEmpty()) {
        app_->removeTranslator(appTranslator_.get());
    }
    if (!qtTranslator_->isEmpty()) {
        app_->removeTranslator(qtTranslator_.get());
    }

    // English is the source language - no translation file needed
    if (code == "en") {
        appTranslator_ = std::make_unique<QTranslator>();
        qtTranslator_ = std::make_unique<QTranslator>();
        return true;
    }

    // Try to load application translation
    QString appQmFile = QString("ratssearch_%1.qm").arg(code);

    // Try resource path first
    QString resourcePath = QString(":/translations/%1").arg(appQmFile);
    bool appLoaded = appTranslator_->load(resourcePath);

    // If not in resources, try file path
    if (!appLoaded && !translationsPath_.isEmpty()) {
        QString filePath = translationsPath_ + "/" + appQmFile;
        appLoaded = appTranslator_->load(filePath);
        if (appLoaded) {
            qInfo() << "Loaded app translation from file:" << filePath;
        }
    }

    if (appLoaded) {
        app_->installTranslator(appTranslator_.get());
        qInfo() << "Loaded app translation:" << appQmFile;
    } else {
        qWarning() << "Could not load app translation:" << appQmFile;
    }

    // Try to load Qt translation
    QString qtQmFile = QString("qt_%1.qm").arg(code);
    QString qtPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);

    bool qtLoaded = qtTranslator_->load(qtQmFile, qtPath);
    if (qtLoaded) {
        app_->installTranslator(qtTranslator_.get());
        qInfo() << "Loaded Qt translation:" << qtQmFile;
    }

    // Return true even if only app translation loaded (Qt translations are
    // optional)
    return appLoaded;
}

} // namespace app
} // namespace rats
