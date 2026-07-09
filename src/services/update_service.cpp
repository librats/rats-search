#include "services/update_service.h"

#include "version.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTextStream>
#include <QThread>

#ifdef _WIN32
#include <windows.h>
#endif

namespace rats::service {

namespace {

// GitHub repository the updater checks by default.
constexpr char kDefaultRepoOwner[] = "DEgITx";
constexpr char kDefaultRepoName[] = "rats-search";

// Milliseconds to wait for an archive extraction subprocess before giving up.
constexpr int kExtractionTimeoutMs = 120000;

// Progress percentage reported once a download has fully completed.
constexpr int kDownloadCompletePercent = 100;

// Seconds the unix updater waits for searchd to exit before force-killing it.
constexpr int kSearchdWaitSeconds = 10;

// Resource paths for the externalized updater script templates.
constexpr char kWindowsUpdaterTemplate[] = ":/update/windows_updater.bat.in";
constexpr char kUnixUpdaterTemplate[] = ":/update/unix_updater.sh.in";

// Executable / process names substituted into the updater scripts.
constexpr char kWindowsAppExe[] = "RatsSearch.exe";
constexpr char kWindowsSearchdExe[] = "searchd.exe";
constexpr char kUnixAppName[] = "RatsSearch";
constexpr char kUnixSearchdName[] = "searchd";

// Reads an updater script template from the Qt resource system, normalizes its
// line endings to '\n', and substitutes each @KEY@ token with its value.
QString loadScriptTemplate(const QString& resourcePath, const QHash<QString, QString>& tokens)
{
    QFile templateFile(resourcePath);
    if (!templateFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open updater template:" << resourcePath;
        return QString();
    }

    QString content = QString::fromUtf8(templateFile.readAll());
    templateFile.close();

    // Work from a canonical '\n' base regardless of how the resource was stored.
    content.replace("\r\n", "\n");

    for (auto it = tokens.constBegin(); it != tokens.constEnd(); ++it) {
        content.replace(it.key(), it.value());
    }

    return content;
}

} // namespace

UpdateService::UpdateService(QObject* parent)
    : QObject(parent)
    , networkManager_(new QNetworkAccessManager(this))
    , currentReply_(nullptr)
    , repoOwner_(kDefaultRepoOwner)
    , repoName_(kDefaultRepoName)
    , state_(UpdateState::Idle)
    , downloadProgress_(0)
    , checkOnStartup_(true)
    , includePrerelease_(false)
{
}

UpdateService::~UpdateService()
{
    if (currentReply_) {
        // Disconnect all signals to prevent callbacks during destruction
        disconnect(currentReply_, nullptr, this, nullptr);
        currentReply_->abort();
        currentReply_->deleteLater();
        currentReply_ = nullptr;
    }
}

QString UpdateService::currentVersion()
{
    return RATSSEARCH_VERSION_STRING;
}

QVersionNumber UpdateService::currentVersionNumber()
{
    return QVersionNumber::fromString(RATSSEARCH_VERSION_STRING);
}

void UpdateService::setRepository(const QString& owner, const QString& repo)
{
    repoOwner_ = owner;
    repoName_ = repo;
}

void UpdateService::setState(UpdateState state)
{
    if (state_ != state) {
        state_ = state;
        emit stateChanged(state);
    }
}

void UpdateService::setError(const QString& error)
{
    errorMessage_ = error;
    setState(UpdateState::Error);
    emit errorOccurred(error);
    qWarning() << "UpdateService error:" << error;
}

QString UpdateService::stateString() const
{
    switch (state_) {
    case UpdateState::Idle:
        return tr("Idle");
    case UpdateState::CheckingForUpdates:
        return tr("Checking for updates...");
    case UpdateState::UpdateAvailable:
        return tr("Update available");
    case UpdateState::Downloading:
        return tr("Downloading update...");
    case UpdateState::Extracting:
        return tr("Extracting update...");
    case UpdateState::ReadyToInstall:
        return tr("Ready to install");
    case UpdateState::Installing:
        return tr("Installing...");
    case UpdateState::Error:
        return tr("Error");
    }
    return QString();
}

bool UpdateService::isUpdateAvailable() const
{
    return state_ == UpdateState::UpdateAvailable || state_ == UpdateState::Downloading
        || state_ == UpdateState::Extracting || state_ == UpdateState::ReadyToInstall;
}

QString UpdateService::getPlatformAssetName() const
{
    QString os;
    QString arch;

#ifdef Q_OS_WIN
    os = "Windows";
    arch = "x64"; // We only build for x64 currently
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

void UpdateService::checkForUpdates()
{
    if (state_ == UpdateState::CheckingForUpdates || state_ == UpdateState::Downloading) {
        qInfo() << "Update check already in progress";
        return;
    }

    setState(UpdateState::CheckingForUpdates);
    updateInfo_ = UpdateInfo();
    errorMessage_.clear();

    // GitHub API endpoint for releases
    QString url = QString("https://api.github.com/repos/%1/%2/releases/latest").arg(repoOwner_, repoName_);

    qInfo() << "Checking for updates at:" << url;

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QString("RatsSearch/%1").arg(currentVersion()));
    request.setRawHeader("Accept", "application/vnd.github.v3+json");

    currentReply_ = networkManager_->get(request);
    connect(currentReply_, &QNetworkReply::finished, this, &UpdateService::onCheckReplyFinished);
}

void UpdateService::onCheckReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply)
        return;

    reply->deleteLater();
    currentReply_ = nullptr;

    if (reply->error() != QNetworkReply::NoError) {
        setError(tr("Failed to check for updates: %1").arg(reply->errorString()));
        return;
    }

    QByteArray data = reply->readAll();
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        setError(tr("Failed to parse update info: %1").arg(parseError.errorString()));
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

        qInfo() << "Found asset:" << name;

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

    qInfo() << "Current version:" << currentVer.toString() << "Latest version:" << newVer.toString();

    if (newVer > currentVer && updateInfo_.isValid()) {
        qInfo() << "Update available:" << version;
        setState(UpdateState::UpdateAvailable);
        emit updateAvailable(updateInfo_);
    } else {
        qInfo() << "No update available";
        setState(UpdateState::Idle);
        emit noUpdateAvailable();
    }
}

void UpdateService::downloadUpdate()
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
    request.setHeader(QNetworkRequest::UserAgentHeader, QString("RatsSearch/%1").arg(currentVersion()));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    currentReply_ = networkManager_->get(request);
    connect(currentReply_, &QNetworkReply::downloadProgress, this, &UpdateService::onDownloadProgress);
    connect(currentReply_, &QNetworkReply::finished, this, &UpdateService::onDownloadFinished);
}

void UpdateService::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal > 0) {
        downloadProgress_ = static_cast<int>((bytesReceived * kDownloadCompletePercent) / bytesTotal);
        emit downloadProgressChanged(downloadProgress_);
    }
}

void UpdateService::onDownloadFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply)
        return;

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

    downloadProgress_ = kDownloadCompletePercent;
    emit downloadProgressChanged(kDownloadCompletePercent);

    // Proceed to extraction
    applyUpdate();
}

void UpdateService::applyUpdate()
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

    // Create update script
    if (!createUpdateScript(extractDir)) {
        setError(tr("Failed to create update script"));
        return;
    }

    setState(UpdateState::ReadyToInstall);
    emit updateReady();
}

bool UpdateService::extractZipFile(const QString& zipPath, const QString& destPath)
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
    process.setArguments({ "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command",
        QString("Expand-Archive -Path '%1' -DestinationPath '%2' -Force").arg(winZipPath).arg(winDestPath) });

    process.start();
    if (!process.waitForFinished(kExtractionTimeoutMs)) {
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
        process.setArguments({ "-o", zipPath, "-d", destPath });
    } else if (zipPath.endsWith(".tar.gz", Qt::CaseInsensitive)) {
        process.setProgram("tar");
        process.setArguments({ "-xzf", zipPath, "-C", destPath });
    } else if (zipPath.endsWith(".AppImage", Qt::CaseInsensitive)) {
        // AppImage doesn't need extraction, just copy
        QString destFile = destPath + "/" + QFileInfo(zipPath).fileName();
        if (QFile::copy(zipPath, destFile)) {
            QFile::setPermissions(destFile,
                QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner | QFileDevice::ReadGroup
                    | QFileDevice::ExeGroup | QFileDevice::ReadOther | QFileDevice::ExeOther);
            return true;
        }
        return false;
    } else {
        qWarning() << "Unknown archive format:" << zipPath;
        return false;
    }

    process.start();
    if (!process.waitForFinished(kExtractionTimeoutMs)) {
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

QString UpdateService::getApplicationDir() const
{
    return QCoreApplication::applicationDirPath();
}

bool UpdateService::createUpdateScript(const QString& updateDir)
{
    QString appDir = getApplicationDir();
    QString appName = QCoreApplication::applicationName();

    qInfo() << "Creating update script for:" << appDir;
    qInfo() << "Update source:" << updateDir;

#ifdef Q_OS_WIN
    // Create batch script for Windows from the resource template
    QString scriptPath = tempDir_->path() + "/update.bat";

    // Convert paths to Windows format
    QString winAppDir = appDir;
    winAppDir.replace("/", "\\");
    QString winUpdateDir = updateDir;
    winUpdateDir.replace("/", "\\");
    QString winTempDir = tempDir_->path();
    winTempDir.replace("/", "\\");

    QHash<QString, QString> tokens;
    tokens.insert("@APP_EXE@", kWindowsAppExe);
    tokens.insert("@SEARCHD_EXE@", kWindowsSearchdExe);
    tokens.insert("@SOURCE_DIR@", winUpdateDir);
    tokens.insert("@DEST_DIR@", winAppDir);
    tokens.insert("@TEMP_DIR@", winTempDir);

    QString content = loadScriptTemplate(kWindowsUpdaterTemplate, tokens);
    if (content.isEmpty()) {
        qWarning() << "Failed to load Windows updater template";
        return false;
    }

    // Batch files use CRLF line endings.
    content.replace("\n", "\r\n");

    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to create update script";
        return false;
    }

    QTextStream out(&script);
    out.setEncoding(QStringConverter::Latin1);
    out << content;
    script.close();

    qInfo() << "Update script created:" << scriptPath;
    return true;

#elif defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    // Create shell script for Linux/macOS from the resource template
    QString scriptPath = tempDir_->path() + "/update.sh";

    // Detect AppImage mode: when running from an AppImage, the APPIMAGE env var
    // contains the real path to the .AppImage file.
    // QCoreApplication::applicationDirPath() returns a path inside the
    // FUSE-mounted temporary filesystem (e.g. /tmp/.mount_XXX/usr/bin) which is
    // destroyed when the AppImage exits, so we cannot copy files there.
    QString appImagePath = qEnvironmentVariable("APPIMAGE");
    bool isAppImage = !appImagePath.isEmpty() && QFile::exists(appImagePath);

    if (isAppImage) {
        qInfo() << "AppImage update mode, real path:" << appImagePath;
    }

    // The unified template branches on @APPIMAGE_FILE@ being non-empty at
    // runtime, so regular installs pass an empty value.
    QHash<QString, QString> tokens;
    tokens.insert("@APP_NAME@", kUnixAppName);
    tokens.insert("@SEARCHD_NAME@", kUnixSearchdName);
    tokens.insert("@SEARCHD_WAIT_SECONDS@", QString::number(kSearchdWaitSeconds));
    tokens.insert("@SOURCE_DIR@", updateDir);
    tokens.insert("@DEST_DIR@", appDir);
    tokens.insert("@APPIMAGE_FILE@", isAppImage ? appImagePath : QString());
    tokens.insert("@TEMP_DIR@", tempDir_->path());

    QString content = loadScriptTemplate(kUnixUpdaterTemplate, tokens);
    if (content.isEmpty()) {
        qWarning() << "Failed to load unix updater template";
        return false;
    }

    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to create update script";
        return false;
    }

    QTextStream out(&script);
    out << content;
    script.close();

    // Make script executable
    QFile::setPermissions(scriptPath,
        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner | QFileDevice::ReadGroup
            | QFileDevice::ExeGroup | QFileDevice::ReadOther | QFileDevice::ExeOther);

    qInfo() << "Update script created:" << scriptPath;
    return true;
#else
    Q_UNUSED(updateDir)
    return false;
#endif
}

void UpdateService::executeUpdateScript()
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
    if (CreateProcessW(NULL, const_cast<wchar_t*>(wCmdLine.c_str()), NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL,
            &si, &pi)) {
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
    QProcess::startDetached("/bin/bash", { scriptPath });

    // Keep temp directory alive
    tempDir_.release();

    // Exit the application
    qInfo() << "Update started, exiting application...";
    QCoreApplication::quit();
#else
    setError(tr("Updates not supported on this platform"));
#endif
}

} // namespace rats::service
