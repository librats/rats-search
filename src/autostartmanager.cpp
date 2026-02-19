#include "autostartmanager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QDebug>

#ifdef _WIN32
#include <QSettings>
#endif

static const char* APP_ID = "RatsSearch";

QString AutoStartManager::appName()
{
    return QStringLiteral("RatsSearch");
}

QString AutoStartManager::appPath()
{
    return QCoreApplication::applicationFilePath();
}

#ifdef _WIN32
// ============================================================================
// Windows: Registry-based autostart
// ============================================================================

bool AutoStartManager::enable()
{
    QSettings settings("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
                       QSettings::NativeFormat);
    
    // Quote the path and add --minimized flag so it starts minimized on autostart
    QString value = QString("\"%1\"").arg(QDir::toNativeSeparators(appPath()));
    settings.setValue(APP_ID, value);
    settings.sync();
    
    bool ok = (settings.status() == QSettings::NoError);
    if (ok) {
        qInfo() << "Autostart enabled (Windows registry):" << value;
    } else {
        qWarning() << "Failed to enable autostart (Windows registry)";
    }
    return ok;
}

bool AutoStartManager::disable()
{
    QSettings settings("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
                       QSettings::NativeFormat);
    settings.remove(APP_ID);
    settings.sync();
    
    bool ok = (settings.status() == QSettings::NoError);
    if (ok) {
        qInfo() << "Autostart disabled (Windows registry)";
    } else {
        qWarning() << "Failed to disable autostart (Windows registry)";
    }
    return ok;
}

bool AutoStartManager::isEnabled()
{
    QSettings settings("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
                       QSettings::NativeFormat);
    return settings.contains(APP_ID);
}

#elif defined(__APPLE__)
// ============================================================================
// macOS: LaunchAgent plist
// ============================================================================

static QString launchAgentPath()
{
    return QDir::homePath() + "/Library/LaunchAgents/com.ratssearch.app.plist";
}

bool AutoStartManager::enable()
{
    QString plistPath = launchAgentPath();
    
    // Ensure LaunchAgents directory exists
    QDir dir(QDir::homePath() + "/Library/LaunchAgents");
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    QFile file(plistPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to create LaunchAgent plist:" << plistPath;
        return false;
    }
    
    QTextStream out(&file);
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
        << "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        << "<plist version=\"1.0\">\n"
        << "<dict>\n"
        << "    <key>Label</key>\n"
        << "    <string>com.ratssearch.app</string>\n"
        << "    <key>ProgramArguments</key>\n"
        << "    <array>\n"
        << "        <string>" << appPath() << "</string>\n"
        << "    </array>\n"
        << "    <key>RunAtLoad</key>\n"
        << "    <true/>\n"
        << "    <key>ProcessType</key>\n"
        << "    <string>Interactive</string>\n"
        << "</dict>\n"
        << "</plist>\n";
    
    file.close();
    qInfo() << "Autostart enabled (macOS LaunchAgent):" << plistPath;
    return true;
}

bool AutoStartManager::disable()
{
    QString plistPath = launchAgentPath();
    
    if (QFile::exists(plistPath)) {
        if (QFile::remove(plistPath)) {
            qInfo() << "Autostart disabled (macOS LaunchAgent)";
            return true;
        } else {
            qWarning() << "Failed to remove LaunchAgent plist:" << plistPath;
            return false;
        }
    }
    
    return true;  // Already disabled
}

bool AutoStartManager::isEnabled()
{
    return QFile::exists(launchAgentPath());
}

#else
// ============================================================================
// Linux: XDG autostart (.desktop file)
// ============================================================================

static QString desktopFilePath()
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return configDir + "/autostart/ratssearch.desktop";
}

bool AutoStartManager::enable()
{
    QString path = desktopFilePath();
    
    // Ensure autostart directory exists
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/autostart");
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to create autostart desktop file:" << path;
        return false;
    }
    
    QTextStream out(&file);
    out << "[Desktop Entry]\n"
        << "Type=Application\n"
        << "Name=Rats Search\n"
        << "Comment=BitTorrent P2P Search Engine\n"
        << "Exec=" << appPath() << "\n"
        << "Icon=ratssearch\n"
        << "Terminal=false\n"
        << "Categories=Network;P2P;\n"
        << "X-GNOME-Autostart-enabled=true\n";
    
    file.close();
    qInfo() << "Autostart enabled (XDG desktop file):" << path;
    return true;
}

bool AutoStartManager::disable()
{
    QString path = desktopFilePath();
    
    if (QFile::exists(path)) {
        if (QFile::remove(path)) {
            qInfo() << "Autostart disabled (XDG desktop file)";
            return true;
        } else {
            qWarning() << "Failed to remove autostart desktop file:" << path;
            return false;
        }
    }
    
    return true;  // Already disabled
}

bool AutoStartManager::isEnabled()
{
    return QFile::exists(desktopFilePath());
}

#endif

// ============================================================================
// Common
// ============================================================================

bool AutoStartManager::setEnabled(bool enabled)
{
    if (enabled) {
        return enable();
    } else {
        return disable();
    }
}
