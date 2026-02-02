#include "contentdetector.h"
#include "torrentdatabase.h"
#include <QMimeDatabase>
#include <QMap>

int ContentDetector::contentTypeId(const QString& type)
{
    static QMap<QString, int> types = {
        {"video", 1}, {"audio", 2}, {"books", 3}, {"pictures", 4},
        {"software", 5}, {"games", 6}, {"archive", 7}, {"bad", 100}
    };
    return types.value(type.toLower(), 0);
}

QString ContentDetector::contentTypeFromId(int id)
{
    static QMap<int, QString> types = {
        {1, "video"}, {2, "audio"}, {3, "books"}, {4, "pictures"},
        {5, "software"}, {6, "games"}, {7, "archive"}, {100, "bad"}
    };
    return types.value(id, "unknown");
}

int ContentDetector::contentCategoryId(const QString& category)
{
    static QMap<QString, int> categories = {
        {"movie", 1}, {"series", 2}, {"documentary", 3}, {"anime", 4},
        {"music", 5}, {"ebook", 6}, {"comics", 7}, {"software", 8},
        {"game", 9}, {"xxx", 100}
    };
    return categories.value(category.toLower(), 0);
}

QString ContentDetector::contentCategoryFromId(int id)
{
    static QMap<int, QString> categories = {
        {1, "movie"}, {2, "series"}, {3, "documentary"}, {4, "anime"},
        {5, "music"}, {6, "ebook"}, {7, "comics"}, {8, "software"},
        {9, "game"}, {100, "xxx"}
    };
    return categories.value(id, "unknown");
}

int ContentDetector::detectContentTypeFromFiles(const QVector<TorrentFile>& files)
{
    QMimeDatabase mimeDb;
    
    // Count file types
    int videoCount = 0, audioCount = 0, imageCount = 0;
    int archiveCount = 0, executableCount = 0, docCount = 0;
    
    for (const TorrentFile& file : files) {
        QString mimeType = mimeDb.mimeTypeForFile(file.path, QMimeDatabase::MatchExtension).name();
        
        if (mimeType.startsWith("video/")) videoCount++;
        else if (mimeType.startsWith("audio/")) audioCount++;
        else if (mimeType.startsWith("image/")) imageCount++;
        else if (mimeType.contains("zip") || mimeType.contains("rar") || 
                 mimeType.contains("7z") || mimeType.contains("tar")) archiveCount++;
        else if (mimeType.contains("executable") || file.path.endsWith(".exe") ||
                 file.path.endsWith(".msi") || file.path.endsWith(".dmg")) executableCount++;
        else if (mimeType.contains("pdf") || mimeType.contains("epub") ||
                 file.path.endsWith(".mobi") || file.path.endsWith(".fb2")) docCount++;
    }
    
    // Determine type
    if (videoCount > 0) {
        return static_cast<int>(ContentType::Video);
    } else if (audioCount > 0) {
        return static_cast<int>(ContentType::Audio);
    } else if (docCount > 0) {
        return static_cast<int>(ContentType::Books);
    } else if (imageCount > 0) {
        return static_cast<int>(ContentType::Pictures);
    } else if (executableCount > 0) {
        return static_cast<int>(ContentType::Software);
    } else if (archiveCount > 0) {
        return static_cast<int>(ContentType::Archive);
    }
    
    return static_cast<int>(ContentType::Unknown);
}

bool ContentDetector::isAdultContent(const QString& name)
{
    QString nameLower = name.toLower();
    static QStringList adultKeywords = {"xxx", "porn", "sex", "adult", "18+"};
    
    for (const QString& keyword : adultKeywords) {
        if (nameLower.contains(keyword)) {
            return true;
        }
    }
    return false;
}

void ContentDetector::detectContentType(TorrentInfo& torrent)
{
    // Detect type from files
    torrent.contentTypeId = detectContentTypeFromFiles(torrent.filesList);
    
    // Set string representation
    torrent.contentType = contentTypeFromId(torrent.contentTypeId);
    
    // Check for adult content in name
    if (isAdultContent(torrent.name)) {
        torrent.contentCategoryId = static_cast<int>(ContentCategory::XXX);
    }
    
    torrent.contentCategory = contentCategoryFromId(torrent.contentCategoryId);
}
