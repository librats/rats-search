#ifndef STARTUPINFO_H
#define STARTUPINFO_H

#include <QString>

/**
 * @brief Logs comprehensive startup/debug information to the application log.
 * 
 * Outputs system info (OS, CPU, memory), data directory details
 * (path, free disk space, directory size), and database file sizes.
 *
 * @param dataDirectory  Path to the application data directory
 */
void logStartupInfo(const QString& dataDirectory);

#endif // STARTUPINFO_H
