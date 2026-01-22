#include "updatemanager.h"
#include "version.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QStandardPaths>
#include <QSysInfo>
#include <QDebug>
#include <QThread>
#include <QTextStream>

#ifdef _WIN32
#include <windows.h>
#endif

UpdateManager::UpdateManager(QObject *parent)
    : QObject(parent)
    , networkManager_(new QNetworkAccessManager(this))
    , currentReply_(nullptr)
    , repoOwner_("DEgITx")
    , repoName_("rats-search")
    , state_(UpdateState::Idle)
    , downloadProgress_(0)
    , checkOnStartup_(true)
    , includePrerelease_(false)
{
}

UpdateManager::~UpdateManager()
{
    if (currentReply_) {
        // Disconnect all signals to prevent callbacks during destruction
        disconnect(currentReply_, nullptr, this, nullptr);
        currentReply_->abort();
        currentReply_->deleteLater();
        currentReply_ = nullptr;
    }
}

QString UpdateManager::currentVersion()
{
    return RATSSEARCH_VERSION_STRING;
}

QVersionNumber UpdateManager::currentVersionNumber()
{
    return QVersionNumber::fromString(RATSSEARCH_VERSION_STRING);
}

void UpdateManager::setRepository(const QString& owner, const QString& repo)
{
    repoOwner_ = owner;
    repoName_ = repo;
}

void UpdateManager::setState(UpdateState state)
{
    if (state_ != state) {
        state_ = state;
        emit stateChanged(state);
    }
}

void UpdateManager::setError(const QString& error)
{
    errorMessage_ = error;
    setState(UpdateState::Error);
    emit errorOccurred(error);
    qWarning() << "UpdateManager error:" << error;
}

QString UpdateManager::stateString() const
{
    switch (state_) {
    case UpdateState::Idle: return tr("Idle");
    case UpdateState::CheckingForUpdates: return tr("Checking for updates...");
    case UpdateState::UpdateAvailable: return tr("Update available");
    case UpdateState::Downloading: return tr("Downloading update...");
    case UpdateState::Extracting: return tr("Extracting update...");
    case UpdateState::ReadyToInstall: return tr("Ready to install");
    case UpdateState::Installing: return tr("Installing...");
    case UpdateState::Error: return tr("Error");
    }
    return QString();
}

bool UpdateManager::isUpdateAvailable() const
{
    return state_ == UpdateState::UpdateAvailable || 
           state_ == UpdateState::Downloading ||
           state_ == UpdateState::Extracting ||
           state_ == UpdateState::ReadyToInstall;
}

QString UpdateManager::getPlatformAssetName() const
{
    QString os;
    QString arch;
    
#ifdef Q_OS_WIN
    os = "Windows";
    arch = "x64";  // We only build for x64 currently
#elif defined(Q_OS_MACOS)
    os = "macOS";
    // Check if running on Apple Silicon
    if (QSysInfo::currentCpuArchitecture().contains("arm")) {
        arch = "ARM";
    } else {
        arch = "Intel";
    }
#elif defined(Q_OS_LINUX)
    os = "Linux";
    arch = "x64";
#else
    return QString();
#endif

    // Return base pattern for matching (without extension)
    // Files are named like: RatsSearch-Windows-x64-v1.2.3.zip
    // We return "RatsSearch-Windows-x64" to match the prefix
    return QString("RatsSearch-%1-%2").arg(os, arch);
}

void UpdateManager::checkForUpdates()
{
    if (state_ == UpdateState::CheckingForUpdates || 
        state_ == UpdateState::Downloading) {
        qDebug() << "Update check already in progress";
        return;
    }
    
    setState(UpdateState::CheckingForUpdates);
    updateInfo_ = UpdateInfo();
    errorMessage_.clear();
    
    // GitHub API endpoint for releases
    QString url = QString("https://api.github.com/repos/%1/%2/releases/latest")
                  .arg(repoOwner_, repoName_);
    
    qInfo() << "Checking for updates at:" << url;
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, 
                      QString("RatsSearch/%1").arg(currentVersion()));
    request.setRawHeader("Accept", "application/vnd.github.v3+json");
    
    currentReply_ = networkManager_->get(request);
    connect(currentReply_, &QNetworkReply::finished, 
            this, &UpdateManager::onCheckReplyFinished);
}

void UpdateManager::onCheckReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    reply->deleteLater();
    currentReply_ = nullptr;
    
    if (reply->error() != QNetworkReply::NoError) {
        setError(tr("Failed to check for updates: %1").arg(reply->errorString()));
        emit checkComplete();
        return;
    }
    
    QByteArray data = reply->readAll();
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        setError(tr("Failed to parse update info: %1").arg(parseError.errorString()));
        emit checkComplete();
        return;
    }
    
    QJsonObject release = doc.object();
    
    // Parse release info
    QString tagName = release["tag_name"].toString();
    // Remove 'v' prefix if present
    QString version = tagName.startsWith('v') ? tagName.mid(1) : tagName;
    
    updateInfo_.version = version;
    updateInfo_.releaseNotes = release["body"].toString();
    updateInfo_.publishedAt = release["published_at"].toString();
    updateInfo_.isPrerelease = release["prerelease"].toBool();
    
    // Skip prereleases if not wanted
    if (updateInfo_.isPrerelease && !includePrerelease_) {
        qInfo() << "Skipping prerelease version" << version;
        setState(UpdateState::Idle);
        emit noUpdateAvailable();
        emit checkComplete();
        return;
    }
    
    // Find the appropriate asset for this platform
    // getPlatformAssetName() returns prefix like "RatsSearch-Windows-x64"
    QString assetPrefix = getPlatformAssetName();
    qInfo() << "Looking for asset with prefix:" << assetPrefix;
    
    // Determine expected extensions for this platform
    QStringList expectedExtensions;
#ifdef Q_OS_WIN
    expectedExtensions << ".zip";
#elif defined(Q_OS_LINUX)
    expectedExtensions << ".AppImage" << ".tar.gz";
#elif defined(Q_OS_MACOS)
    expectedExtensions << ".zip";
#else
    expectedExtensions << ".zip" << ".tar.gz";
#endif
    
    QJsonArray assets = release["assets"].toArray();
    for (const QJsonValue& assetVal : assets) {
        QJsonObject asset = assetVal.toObject();
        QString name = asset["name"].toString();
        
        qDebug() << "Found asset:" << name;
        
        // Check if asset starts with our platform prefix and has correct extension
        // Handles both old format (RatsSearch-Windows-x64.zip) and
        // new versioned format (RatsSearch-Windows-x64-v1.2.3.zip)
        if (name.startsWith(assetPrefix, Qt::CaseInsensitive)) {
            for (const QString& ext : expectedExtensions) {
                if (name.endsWith(ext, Qt::CaseInsensitive)) {
                    updateInfo_.downloadUrl = asset["browser_download_url"].toString();
                    updateInfo_.downloadSize = asset["size"].toVariant().toLongLong();
                    qInfo() << "Selected asset:" << name;
                    break;
                }
            }
            if (!updateInfo_.downloadUrl.isEmpty()) {
                break;
            }
        }
    }
    
    // Compare versions
    QVersionNumber currentVer = currentVersionNumber();
    QVersionNumber newVer = QVersionNumber::fromString(version);
    
    qInfo() << "Current version:" << currentVer.toString() 
            << "Latest version:" << newVer.toString();
    
    if (newVer > currentVer && updateInfo_.isValid()) {
        qInfo() << "Update available:" << version;
        setState(UpdateState::UpdateAvailable);
        emit updateAvailable(updateInfo_);
    } else {
        qInfo() << "No update available";
        setState(UpdateState::Idle);
        emit noUpdateAvailable();
    }
    
    emit checkComplete();
}

void UpdateManager::downloadUpdate()
{
    if (!updateInfo_.isValid()) {
        setError(tr("No update available to download"));
        return;
    }
    
    if (state_ == UpdateState::Downloading) {
        return;
    }
    
    setState(UpdateState::Downloading);
    downloadProgress_ = 0;
    
    // Create temp directory for download
    tempDir_ = std::make_unique<QTemporaryDir>();
    if (!tempDir_->isValid()) {
        setError(tr("Failed to create temporary directory"));
        return;
    }
    
    // Determine download filename
    QUrl url(updateInfo_.downloadUrl);
    QString fileName = QFileInfo(url.path()).fileName();
    downloadedFilePath_ = tempDir_->path() + "/" + fileName;
    
    qInfo() << "Downloading update to:" << downloadedFilePath_;
    qInfo() << "From URL:" << updateInfo_.downloadUrl;
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QString("RatsSearch/%1").arg(currentVersion()));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                        QNetworkRequest::NoLessSafeRedirectPolicy);
    
    currentReply_ = networkManager_->get(request);
    connect(currentReply_, &QNetworkReply::downloadProgress,
            this, &UpdateManager::onDownloadProgress);
    connect(currentReply_, &QNetworkReply::finished,
            this, &UpdateManager::onDownloadFinished);
}

void UpdateManager::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal > 0) {
        downloadProgress_ = static_cast<int>((bytesReceived * 100) / bytesTotal);
        emit downloadProgressChanged(downloadProgress_);
    }
}

void UpdateManager::onDownloadFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    reply->deleteLater();
    currentReply_ = nullptr;
    
    if (reply->error() != QNetworkReply::NoError) {
        setError(tr("Download failed: %1").arg(reply->errorString()));
        return;
    }
    
    // Save downloaded data to file
    QFile file(downloadedFilePath_);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(tr("Failed to save update file: %1").arg(file.errorString()));
        return;
    }
    
    file.write(reply->readAll());
    file.close();
    
    qInfo() << "Download complete:" << downloadedFilePath_;
    qInfo() << "File size:" << QFileInfo(downloadedFilePath_).size() << "bytes";
    
    downloadProgress_ = 100;
    emit downloadProgressChanged(100);
    emit downloadComplete();
    
    // Proceed to extraction
    applyUpdate();
}

void UpdateManager::applyUpdate()
{
    if (downloadedFilePath_.isEmpty() || !QFile::exists(downloadedFilePath_)) {
        setError(tr("Update file not found"));
        return;
    }
    
    setState(UpdateState::Extracting);
    
    // Create extraction directory
    QString extractDir = tempDir_->path() + "/extracted";
    QDir().mkpath(extractDir);
    
    qInfo() << "Extracting to:" << extractDir;
    
    // Extract the archive
    if (!extractZipFile(downloadedFilePath_, extractDir)) {
        setError(tr("Failed to extract update archive"));
        return;
    }
    
    emit extractionComplete();
    
    // Create update script
    if (!createUpdateScript(extractDir)) {
        setError(tr("Failed to create update script"));
        return;
    }
    
    setState(UpdateState::ReadyToInstall);
    emit updateReady();
}

bool UpdateManager::extractZipFile(const QString& zipPath, const QString& destPath)
{
    qInfo() << "Extracting" << zipPath << "to" << destPath;
    
#ifdef Q_OS_WIN
    // Use PowerShell to extract on Windows
    QString winZipPath = zipPath;
    QString winDestPath = destPath;
    winZipPath.replace("/", "\\");
    winDestPath.replace("/", "\\");
    
    QProcess process;
    process.setProgram("powershell");
    process.setArguments({
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-Command",
        QString("Expand-Archive -Path '%1' -DestinationPath '%2' -Force")
            .arg(winZipPath)
            .arg(winDestPath)
    });
    
    process.start();
    if (!process.waitForFinished(120000)) {  // 2 minute timeout
        qWarning() << "Extraction timeout";
        return false;
    }
    
    if (process.exitCode() != 0) {
        qWarning() << "Extraction failed:" << process.readAllStandardError();
        return false;
    }
    
    return true;
    
#elif defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    // Use unzip command on Linux/macOS
    QProcess process;
    
    if (zipPath.endsWith(".zip", Qt::CaseInsensitive)) {
        process.setProgram("unzip");
        process.setArguments({"-o", zipPath, "-d", destPath});
    } else if (zipPath.endsWith(".tar.gz", Qt::CaseInsensitive)) {
        process.setProgram("tar");
        process.setArguments({"-xzf", zipPath, "-C", destPath});
    } else if (zipPath.endsWith(".AppImage", Qt::CaseInsensitive)) {
        // AppImage doesn't need extraction, just copy
        QString destFile = destPath + "/" + QFileInfo(zipPath).fileName();
        if (QFile::copy(zipPath, destFile)) {
            QFile::setPermissions(destFile, 
                QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
                QFileDevice::ReadGroup | QFileDevice::ExeGroup |
                QFileDevice::ReadOther | QFileDevice::ExeOther);
            return true;
        }
        return false;
    } else {
        qWarning() << "Unknown archive format:" << zipPath;
        return false;
    }
    
    process.start();
    if (!process.waitForFinished(120000)) {
        qWarning() << "Extraction timeout";
        return false;
    }
    
    if (process.exitCode() != 0) {
        qWarning() << "Extraction failed:" << process.readAllStandardError();
        return false;
    }
    
    return true;
#else
    Q_UNUSED(zipPath)
    Q_UNUSED(destPath)
    return false;
#endif
}

QString UpdateManager::getApplicationDir() const
{
    return QCoreApplication::applicationDirPath();
}

bool UpdateManager::createUpdateScript(const QString& updateDir)
{
    QString appDir = getApplicationDir();
    QString appName = QCoreApplication::applicationName();
    
    qInfo() << "Creating update script for:" << appDir;
    qInfo() << "Update source:" << updateDir;
    
#ifdef Q_OS_WIN
    // Create batch script for Windows
    QString scriptPath = tempDir_->path() + "/update.bat";
    QFile script(scriptPath);
    
    if (!script.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to create update script";
        return false;
    }
    
    QTextStream out(&script);
    out.setEncoding(QStringConverter::Latin1);
    
    // Convert paths to Windows format
    QString winAppDir = appDir;
    winAppDir.replace("/", "\\");
    QString winUpdateDir = updateDir;
    winUpdateDir.replace("/", "\\");
    QString winExePath = winAppDir + "\\RatsSearch.exe";
    
    out << "@echo off\r\n";
    out << "chcp 65001 >nul\r\n";
    out << "echo Rats Search Updater\r\n";
    out << "echo ===================\r\n";
    out << "echo.\r\n";
    out << "echo Waiting for application to close...\r\n";
    out << "timeout /t 3 /nobreak >nul\r\n";
    out << "\r\n";
    out << ":waitloop\r\n";
    out << "tasklist /FI \"IMAGENAME eq RatsSearch.exe\" 2>NUL | find /I \"RatsSearch.exe\" >NUL\r\n";
    out << "if not errorlevel 1 (\r\n";
    out << "    timeout /t 1 /nobreak >nul\r\n";
    out << "    goto waitloop\r\n";
    out << ")\r\n";
    out << "\r\n";
    out << "echo Updating files...\r\n";
    out << "\r\n";
    
    // Find the actual extracted directory (could be nested)
    out << "set \"SOURCE_DIR=" << winUpdateDir << "\"\r\n";
    out << "set \"DEST_DIR=" << winAppDir << "\"\r\n";
    out << "\r\n";
    
    // Check if there's a nested directory (e.g., RatsSearch-Windows-x64/)
    out << "for /d %%D in (\"%SOURCE_DIR%\\*\") do (\r\n";
    out << "    if exist \"%%D\\RatsSearch.exe\" (\r\n";
    out << "        set \"SOURCE_DIR=%%D\"\r\n";
    out << "    )\r\n";
    out << ")\r\n";
    out << "\r\n";
    
    // Copy all files, overwriting existing
    out << "echo Copying from %SOURCE_DIR% to %DEST_DIR%\r\n";
    out << "xcopy /E /Y /I \"%SOURCE_DIR%\\*\" \"%DEST_DIR%\\\" >nul 2>&1\r\n";
    out << "\r\n";
    out << "if errorlevel 1 (\r\n";
    out << "    echo Update failed! Error copying files.\r\n";
    out << "    pause\r\n";
    out << "    exit /b 1\r\n";
    out << ")\r\n";
    out << "\r\n";
    out << "echo Update complete!\r\n";
    out << "echo Starting application...\r\n";
    out << "echo.\r\n";
    out << "start \"\" \"" << winExePath << "\"\r\n";
    out << "\r\n";
    out << "rem Clean up temp directory after a delay\r\n";
    out << "timeout /t 5 /nobreak >nul\r\n";
    out << "rd /s /q \"" << tempDir_->path().replace("/", "\\") << "\" 2>nul\r\n";
    out << "exit /b 0\r\n";
    
    script.close();
    
    qInfo() << "Update script created:" << scriptPath;
    return true;
    
#elif defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    // Create shell script for Linux/macOS
    QString scriptPath = tempDir_->path() + "/update.sh";
    QFile script(scriptPath);
    
    if (!script.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to create update script";
        return false;
    }
    
    QTextStream out(&script);
    
    QString exePath = appDir + "/RatsSearch";
    
    out << "#!/bin/bash\n";
    out << "echo 'Rats Search Updater'\n";
    out << "echo '==================='\n";
    out << "echo\n";
    out << "echo 'Waiting for application to close...'\n";
    out << "sleep 3\n";
    out << "\n";
    out << "# Wait for the app to exit\n";
    out << "while pgrep -x 'RatsSearch' > /dev/null; do\n";
    out << "    sleep 1\n";
    out << "done\n";
    out << "\n";
    out << "echo 'Updating files...'\n";
    out << "\n";
    out << "SOURCE_DIR='" << updateDir << "'\n";
    out << "DEST_DIR='" << appDir << "'\n";
    out << "\n";
    out << "# Find the actual source directory (might be nested)\n";
    out << "for dir in \"$SOURCE_DIR\"/*/; do\n";
    out << "    if [ -f \"${dir}RatsSearch\" ] || [ -f \"${dir}RatsSearch.AppImage\" ]; then\n";
    out << "        SOURCE_DIR=\"${dir%/}\"\n";
    out << "        break\n";
    out << "    fi\n";
    out << "done\n";
    out << "\n";
    out << "# Copy all files\n";
    out << "cp -rf \"$SOURCE_DIR\"/* \"$DEST_DIR/\" 2>/dev/null\n";
    out << "\n";
    out << "# Make executable\n";
    out << "chmod +x \"$DEST_DIR/RatsSearch\" 2>/dev/null\n";
    out << "chmod +x \"$DEST_DIR\"/*.AppImage 2>/dev/null\n";
    out << "chmod +x \"$DEST_DIR/searchd\" 2>/dev/null\n";
    out << "chmod +x \"$DEST_DIR/indexer\" 2>/dev/null\n";
    out << "\n";
    out << "echo 'Update complete!'\n";
    out << "echo 'Starting application...'\n";
    out << "\n";
    out << "# Start the updated application\n";
    out << "'" << exePath << "' &\n";
    out << "\n";
    out << "# Clean up\n";
    out << "sleep 5\n";
    out << "rm -rf '" << tempDir_->path() << "'\n";
    out << "exit 0\n";
    
    script.close();
    
    // Make script executable
    QFile::setPermissions(scriptPath,
        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
        QFileDevice::ReadGroup | QFileDevice::ExeGroup |
        QFileDevice::ReadOther | QFileDevice::ExeOther);
    
    qInfo() << "Update script created:" << scriptPath;
    return true;
#else
    Q_UNUSED(updateDir)
    return false;
#endif
}

void UpdateManager::executeUpdateScript()
{
    if (state_ != UpdateState::ReadyToInstall) {
        setError(tr("Update is not ready to install"));
        return;
    }
    
    setState(UpdateState::Installing);
    
#ifdef Q_OS_WIN
    QString scriptPath = tempDir_->path() + "/update.bat";
    
    qInfo() << "Executing update script:" << scriptPath;
    
    // Start the update script in a new process
    // Use cmd.exe to run the batch file so it stays open
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    
    // Build command line - run the batch file
    QString cmdLine = QString("cmd.exe /c \"%1\"").arg(scriptPath.replace("/", "\\"));
    std::wstring wCmdLine = cmdLine.toStdWString();
    
    // Create process with CREATE_NEW_CONSOLE so it's visible
    if (CreateProcessW(NULL,
                       const_cast<wchar_t*>(wCmdLine.c_str()),
                       NULL, NULL, FALSE,
                       CREATE_NEW_CONSOLE,
                       NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        // Keep temp directory alive by releasing it from unique_ptr
        // The script will clean it up after update
        tempDir_.release();
        
        // Exit the application
        qInfo() << "Update started, exiting application...";
        QCoreApplication::quit();
    } else {
        setError(tr("Failed to start update script"));
    }
    
#elif defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    QString scriptPath = tempDir_->path() + "/update.sh";
    
    qInfo() << "Executing update script:" << scriptPath;
    
    // Start the script in background
    QProcess::startDetached("/bin/bash", {scriptPath});
    
    // Keep temp directory alive
    tempDir_.release();
    
    // Exit the application
    qInfo() << "Update started, exiting application...";
    QCoreApplication::quit();
#else
    setError(tr("Updates not supported on this platform"));
#endif
}

