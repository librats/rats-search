#ifndef AUTOSTARTMANAGER_H
#define AUTOSTARTMANAGER_H

#include <QString>

/**
 * @brief AutoStartManager - Manages OS-level application autostart
 * 
 * Platform-specific implementations:
 * - Windows: Registry (HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\Run)
 * - Linux: XDG autostart (.desktop file in ~/.config/autostart/)
 * - macOS: LaunchAgent plist in ~/Library/LaunchAgents/
 */
class AutoStartManager
{
public:
    /**
     * @brief Enable application autostart on system login
     * @return true if autostart was successfully enabled
     */
    static bool enable();
    
    /**
     * @brief Disable application autostart on system login
     * @return true if autostart was successfully disabled
     */
    static bool disable();
    
    /**
     * @brief Check if autostart is currently enabled at OS level
     * @return true if application is registered for autostart
     */
    static bool isEnabled();
    
    /**
     * @brief Set autostart state (convenience method)
     * @param enabled true to enable, false to disable
     * @return true if operation succeeded
     */
    static bool setEnabled(bool enabled);

private:
    static QString appName();
    static QString appPath();
};

#endif // AUTOSTARTMANAGER_H
