#include "contentdetector.h"
#include "torrentdatabase.h"
#include "badwords.h"
#include <QMap>
#include <QHash>
#include <QFileInfo>
#include <QRegularExpression>

// ============================================================================
// Extension to content type mapping (from legacy content.js)
// ============================================================================

static const QHash<QString, QString>& extensionMap()
{
    static QHash<QString, QString> map = {
        // Video
        {"webm", "video"}, {"mkv", "video"}, {"flv", "video"}, {"vob", "video"},
        {"ogv", "video"}, {"drc", "video"}, {"mng", "video"}, {"avi", "video"},
        {"mov", "video"}, {"qt", "video"}, {"wmv", "video"}, {"yuv", "video"},
        {"rm", "video"}, {"rmvb", "video"}, {"asf", "video"}, {"amv", "video"},
        {"mp4", "video"}, {"m4p", "video"}, {"m4v", "video"}, {"mpg", "video"},
        {"mpeg", "video"}, {"mpv", "video"}, {"svi", "video"}, {"3gp", "video"},
        {"3g2", "video"}, {"mxf", "video"}, {"roq", "video"}, {"nsv", "video"},
        {"f4v", "video"}, {"ts", "video"}, {"divx", "video"}, {"m2ts", "video"},
        
        // Audio
        {"aa", "audio"}, {"aac", "audio"}, {"aax", "audio"}, {"act", "audio"},
        {"aiff", "audio"}, {"amr", "audio"}, {"ape", "audio"}, {"au", "audio"},
        {"awb", "audio"}, {"dct", "audio"}, {"dss", "audio"}, {"dvf", "audio"},
        {"flac", "audio"}, {"gsm", "audio"}, {"iklax", "audio"}, {"ivs", "audio"},
        {"m4a", "audio"}, {"mmf", "audio"}, {"mp3", "audio"}, {"mpc", "audio"},
        {"msv", "audio"}, {"ogg", "audio"}, {"oga", "audio"}, {"opus", "audio"},
        {"ra", "audio"}, {"raw", "audio"}, {"sln", "audio"}, {"tta", "audio"},
        {"vox", "audio"}, {"wav", "audio"}, {"wma", "audio"}, {"wv", "audio"},
        {"ac3", "audio"},
        
        // Pictures
        {"jpg", "pictures"}, {"jpeg", "pictures"}, {"exif", "pictures"},
        {"gif", "pictures"}, {"tiff", "pictures"}, {"bmp", "pictures"},
        {"png", "pictures"}, {"ppm", "pictures"}, {"pgm", "pictures"},
        {"pbm", "pictures"}, {"pnm", "pictures"}, {"webp", "pictures"},
        {"heif", "pictures"}, {"bpg", "pictures"}, {"ico", "pictures"},
        {"tga", "pictures"}, {"cd5", "pictures"}, {"deep", "pictures"},
        {"ecw", "pictures"}, {"fits", "pictures"}, {"flif", "pictures"},
        {"ilbm", "pictures"}, {"img", "pictures"}, {"nrrd", "pictures"},
        {"pam", "pictures"}, {"pcx", "pictures"}, {"pgf", "pictures"},
        {"sgi", "pictures"}, {"sid", "pictures"}, {"vicar", "pictures"},
        {"psd", "pictures"}, {"cpt", "pictures"}, {"psp", "pictures"},
        {"xcf", "pictures"}, {"svg", "pictures"}, {"cgm", "pictures"},
        {"cdr", "pictures"}, {"hvif", "pictures"}, {"odg", "pictures"},
        {"vml", "pictures"}, {"wmf", "pictures"}, {"avif", "pictures"},
        
        // Books
        {"cbr", "books"}, {"cbz", "books"}, {"cb7", "books"}, {"cbt", "books"},
        {"cba", "books"}, {"lrf", "books"}, {"lrx", "books"}, {"chm", "books"},
        {"djvu", "books"}, {"doc", "books"}, {"docx", "books"}, {"epub", "books"},
        {"pdf", "books"}, {"pdb", "books"}, {"fb2", "books"}, {"xeb", "books"},
        {"ceb", "books"}, {"htm", "books"}, {"html", "books"}, {"css", "books"},
        {"txt", "books"}, {"ibooks", "books"}, {"inf", "books"}, {"azw3", "books"},
        {"azw", "books"}, {"kf8", "books"}, {"lit", "books"}, {"prc", "books"},
        {"mobi", "books"}, {"opf", "books"}, {"rtf", "books"}, {"pdg", "books"},
        {"xml", "books"}, {"tr2", "books"}, {"tr3", "books"}, {"oxps", "books"},
        {"xps", "books"},
        
        // Software (Application)
        {"exe", "software"}, {"apk", "software"}, {"rpm", "software"},
        {"deb", "software"}, {"jar", "software"}, {"bundle", "software"},
        {"com", "software"}, {"so", "software"}, {"dll", "software"},
        {"elf", "software"}, {"ipa", "software"}, {"xbe", "software"},
        {"xap", "software"}, {"a", "software"}, {"bin", "software"},
        {"msi", "software"}, {"dmg", "software"}, {"pbi", "software"},
        {"app", "software"}, {"pkg", "software"},
        
        // Games (common game file extensions)
        {"vpk", "games"}, {"wad", "games"}, {"pak", "games"}, {"pk3", "games"},
        {"pk4", "games"}, {"bsp", "games"}, {"gcf", "games"}, {"ncf", "games"},
        {"xnb", "games"}, {"unity3d", "games"}, {"assets", "games"},
        {"nsp", "games"}, {"xci", "games"}, {"nro", "games"}, // Nintendo Switch
        {"cia", "games"}, {"3ds", "games"}, {"nds", "games"}, // Nintendo 3DS/DS
        {"gba", "games"}, {"gbc", "games"}, {"gb", "games"},  // GameBoy
        {"nes", "games"}, {"sfc", "games"}, {"smc", "games"}, // NES/SNES
        {"z64", "games"}, {"n64", "games"}, {"v64", "games"}, // N64
        {"gcm", "games"}, {"wbfs", "games"}, {"wad", "games"}, // GameCube/Wii
        {"xiso", "games"}, // Xbox
        {"pkg", "games"}, // PS3/PS4 (conflicts with software, detect by context)
        {"psv", "games"}, {"psvita", "games"}, // PS Vita
        {"cso", "games"}, {"pbp", "games"}, // PSP
        
        // Archive
        {"tar", "archive"}, {"gz", "archive"}, {"bz2", "archive"},
        {"rar", "archive"}, {"zip", "archive"}, {"lz", "archive"},
        {"lzma", "archive"}, {"lzo", "archive"}, {"rz", "archive"},
        {"sfark", "archive"}, {"sf2", "archive"}, {"xz", "archive"},
        {"z", "archive"}, {"7z", "archive"}, {"s7z", "archive"},
        {"ace", "archive"}, {"afa", "archive"}, {"arc", "archive"},
        {"arj", "archive"}, {"b1", "archive"}, {"car", "archive"},
        {"cfs", "archive"}, {"dar", "archive"}, {"ice", "archive"},
        {"sfx", "archive"}, {"shk", "archive"}, {"sit", "archive"},
        {"tgz", "archive"}, {"xar", "archive"}, {"zz", "archive"},
        {"zst", "archive"}, {"tzst", "archive"},
        
        // Disc images (mapped to archive for now, since we don't have disc type)
        {"iso", "archive"}, {"mdf", "archive"}, {"mds", "archive"},
        {"nrg", "archive"}, {"ima", "archive"}, {"imz", "archive"},
        {"mdx", "archive"}, {"uif", "archive"}, {"isz", "archive"},
        {"daa", "archive"}, {"cue", "archive"}, {"fvd", "archive"},
        {"ndif", "archive"}, {"udif", "archive"}, {"vdi", "archive"},
        {"vhd", "archive"}, {"wim", "archive"}, {"vmdk", "archive"},
        {"vhdx", "archive"}, {"qcow2", "archive"}, {"img", "archive"},
    };
    return map;
}

// ============================================================================
// Content Type ID Conversion
// ============================================================================

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

// ============================================================================
// File Type Detection (from legacy content.js: fileTypeDetect)
// ============================================================================

static QString detectFileType(const QString& filePath)
{
    // Get filename from path
    QString name = filePath.section('/', -1);
    if (name.isEmpty()) {
        name = filePath.section('\\', -1);
    }
    
    if (name.isEmpty()) {
        return QString();
    }
    
    // Get extension
    int dotPos = name.lastIndexOf('.');
    if (dotPos < 0 || dotPos == name.length() - 1) {
        return QString();
    }
    
    QString extension = name.mid(dotPos + 1).toLower();
    
    return extensionMap().value(extension);
}

// ============================================================================
// Weighted Content Type Detection (from legacy content.js: torrentTypeDetect)
// Uses file size weighting: typesPriority[type] += file.size / torrent.size
// ============================================================================

int ContentDetector::detectContentTypeFromFiles(const QVector<TorrentFile>& files)
{
    if (files.isEmpty()) {
        return static_cast<int>(ContentType::Unknown);
    }
    
    // Calculate total size
    qint64 totalSize = 0;
    for (const TorrentFile& file : files) {
        totalSize += file.size;
    }
    
    if (totalSize == 0) {
        // Fallback to simple count-based detection
        totalSize = files.size();  // Use file count as weight
    }
    
    // Weighted type priority (like legacy: typesPriority[type] += file.size / torrent.size)
    QMap<QString, double> typesPriority;
    
    for (const TorrentFile& file : files) {
        QString type = detectFileType(file.path);
        
        if (!type.isEmpty()) {
            double weight = (totalSize > 0) 
                ? static_cast<double>(file.size) / static_cast<double>(totalSize)
                : 1.0 / files.size();
            
            typesPriority[type] += weight;
        }
    }
    
    if (typesPriority.isEmpty()) {
        return static_cast<int>(ContentType::Unknown);
    }
    
    // Find type with highest weight
    QString bestType;
    double bestWeight = 0.0;
    
    for (auto it = typesPriority.begin(); it != typesPriority.end(); ++it) {
        if (it.value() > bestWeight) {
            bestWeight = it.value();
            bestType = it.key();
        }
    }
    
    return contentTypeId(bestType);
}

// ============================================================================
// Adult Content Detection (ported from legacy blockBadName in content.js)
// ============================================================================

// Static regex for splitting names by delimiters (like legacy: split(/[`~!@#$%^&*()\]\[{}.,+?/\\;:\-_' "|]/))
static const QRegularExpression& nameDelimiterRegex()
{
    static QRegularExpression regex(R"([`~!@#$%^&*()\]\[{}.,+?/\\;:\-_' "|]+)");
    return regex;
}

void ContentDetector::blockBadName(TorrentInfo& torrent, const QString& name)
{
    // Split name by delimiters (like legacy: name.split(/[`~!@#$%^&*()\]\[{}.,+?/\\;:\-_' "|]/))
    QStringList words = name.toLower().split(nameDelimiterRegex(), Qt::SkipEmptyParts);
    
    const QSet<QString>& veryBadWords = BadWords::xxxVeryBadWords();
    const QSet<QString>& blockWords = BadWords::xxxBlockWords();
    
    for (const QString& word : words) {
        // Check for very bad words first → mark as BAD type
        if (veryBadWords.contains(word)) {
            torrent.contentTypeId = static_cast<int>(ContentType::Bad);
            torrent.contentType = contentTypeFromId(torrent.contentTypeId);
            return; // Stop if marked as bad
        }
        
        // Check for block words → mark as XXX category
        if (blockWords.contains(word)) {
            torrent.contentCategoryId = static_cast<int>(ContentCategory::XXX);
            torrent.contentCategory = contentCategoryFromId(torrent.contentCategoryId);
            // Don't return - continue checking for very bad words
        }
    }
}

// ============================================================================
// Main Detection Entry Point (from legacy content.js: torrentTypeDetect + detectSubCategory)
// ============================================================================

void ContentDetector::detectContentType(TorrentInfo& torrent)
{
    // Detect type from files using weighted algorithm
    torrent.contentTypeId = detectContentTypeFromFiles(torrent.filesList);
    
    // Set string representation
    torrent.contentType = contentTypeFromId(torrent.contentTypeId);
    
    // =========================================================================
    // detectSubCategory logic (from legacy content.js)
    // Only for video, pictures, and archive types
    // =========================================================================
    if (torrent.contentTypeId == static_cast<int>(ContentType::Video) ||
        torrent.contentTypeId == static_cast<int>(ContentType::Pictures) ||
        torrent.contentTypeId == static_cast<int>(ContentType::Archive)) {
        
        // Check torrent name for bad words
        blockBadName(torrent, torrent.name);
        
        // If not marked as bad, check file names
        if (torrent.contentTypeId != static_cast<int>(ContentType::Bad)) {
            for (const TorrentFile& file : torrent.filesList) {
                // Remove extension and check filename (like legacy: fileCheck.pop())
                QString filePath = file.path;
                int dotPos = filePath.lastIndexOf('.');
                if (dotPos > 0) {
                    filePath = filePath.left(dotPos);
                }
                
                blockBadName(torrent, filePath);
                
                // Stop if marked as bad
                if (torrent.contentTypeId == static_cast<int>(ContentType::Bad)) {
                    break;
                }
            }
        }
    }
    
    // Ensure category string is set
    torrent.contentCategory = contentCategoryFromId(torrent.contentCategoryId);
}
