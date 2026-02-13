#ifndef LEGACYMIGRATION_H
#define LEGACYMIGRATION_H

#include <QString>

/**
 * Migrate legacy (Electron v1.x) database to the new v2.0 data directory.
 * 
 * Legacy Electron app (productName "Rats on The Boat") used app.getPath("userData"):
 *   - Windows: %APPDATA%/Rats on The Boat  (C:/Users/<user>/AppData/Roaming/Rats on The Boat)
 *   - macOS:   ~/Library/Application Support/Rats on The Boat
 *   - Linux:   ~/.config/Rats on The Boat
 * 
 * The new Qt 2.0 app uses QStandardPaths::AppDataLocation:
 *   - Windows: %LOCALAPPDATA%/Rats Search  (C:/Users/<user>/AppData/Local/Rats Search)
 *   - macOS:   ~/Library/Application Support/Rats Search
 *   - Linux:   ~/.local/share/Rats Search
 * 
 * This runs BEFORE the database is initialized. It checks if the new data
 * directory is empty (first run of v2.0) and a legacy database exists,
 * then copies the Manticore database files, config, and binlogs.
 * 
 * @param newDataDir The new v2.0 data directory path
 * @return true if migration was performed, false if not needed
 */
bool migrateLegacyDatabase(const QString& newDataDir);

#endif // LEGACYMIGRATION_H
