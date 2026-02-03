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
        {"mts", "video"}, {"m2t", "video"}, {"tp", "video"}, {"trp", "video"},
        {"wtv", "video"}, {"dvr-ms", "video"}, {"f4p", "video"}, {"f4a", "video"},
        {"f4b", "video"}, {"vro", "video"}, {"ifo", "video"}, {"bup", "video"},
        {"264", "video"}, {"265", "video"}, {"hevc", "video"}, {"h264", "video"},
        {"h265", "video"}, {"avc", "video"}, {"avchd", "video"}, {"fli", "video"},
        {"flc", "video"}, {"rec", "video"}, {"bik", "video"}, {"smk", "video"},
        {"vp6", "video"}, {"vp8", "video"}, {"vp9", "video"}, {"av1", "video"},
        {"hdmov", "video"}, {"mpe", "video"}, {"m1v", "video"}, {"m2v", "video"},
        {"mod", "video"}, {"tod", "video"}, {"dav", "video"}, {"cpk", "video"},
        {"xvid", "video"}, {"mpeg4", "video"},
        
        // Audio
        {"aa", "audio"}, {"aac", "audio"}, {"aax", "audio"}, {"act", "audio"},
        {"aiff", "audio"}, {"amr", "audio"}, {"ape", "audio"}, {"au", "audio"},
        {"awb", "audio"}, {"dct", "audio"}, {"dss", "audio"}, {"dvf", "audio"},
        {"flac", "audio"}, {"gsm", "audio"}, {"iklax", "audio"}, {"ivs", "audio"},
        {"m4a", "audio"}, {"mmf", "audio"}, {"mp3", "audio"}, {"mpc", "audio"},
        {"msv", "audio"}, {"ogg", "audio"}, {"oga", "audio"}, {"opus", "audio"},
        {"ra", "audio"}, {"raw", "audio"}, {"sln", "audio"}, {"tta", "audio"},
        {"vox", "audio"}, {"wav", "audio"}, {"wma", "audio"}, {"wv", "audio"},
        {"ac3", "audio"}, {"dts", "audio"}, {"dtshd", "audio"}, {"eac3", "audio"},
        {"thd", "audio"}, {"mlp", "audio"}, {"pcm", "audio"}, {"aif", "audio"},
        {"aifc", "audio"}, {"caf", "audio"}, {"snd", "audio"}, {"3ga", "audio"},
        // Tracker/Module music
        {"mod", "audio"}, {"xm", "audio"}, {"it", "audio"}, {"s3m", "audio"},
        {"umx", "audio"}, {"stm", "audio"}, {"mtm", "audio"}, {"med", "audio"},
        {"669", "audio"}, {"far", "audio"}, {"dsm", "audio"}, {"amf", "audio"},
        // MIDI
        {"mid", "audio"}, {"midi", "audio"}, {"kar", "audio"}, {"rmi", "audio"},
        // High-resolution audio
        {"dff", "audio"}, {"dsf", "audio"}, {"tak", "audio"}, {"alac", "audio"},
        {"m4b", "audio"}, {"m4r", "audio"}, {"spx", "audio"}, {"ofr", "audio"},
        {"ofs", "audio"}, {"wvc", "audio"}, {"cue", "audio"}, {"ape", "audio"},
        
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
        {"emf", "pictures"}, {"eps", "pictures"}, {"ai", "pictures"},
        {"jfif", "pictures"}, {"jpe", "pictures"}, {"jp2", "pictures"},
        {"j2k", "pictures"}, {"jpf", "pictures"}, {"jpx", "pictures"},
        {"jpm", "pictures"}, {"mj2", "pictures"}, {"jxr", "pictures"},
        {"hdp", "pictures"}, {"wdp", "pictures"}, {"dib", "pictures"},
        {"apng", "pictures"}, {"mng", "pictures"}, {"jxl", "pictures"},
        // RAW camera formats
        {"cr2", "pictures"}, {"cr3", "pictures"}, {"crw", "pictures"},  // Canon
        {"nef", "pictures"}, {"nrw", "pictures"},  // Nikon
        {"arw", "pictures"}, {"srf", "pictures"}, {"sr2", "pictures"},  // Sony
        {"orf", "pictures"},  // Olympus
        {"rw2", "pictures"}, {"rwl", "pictures"},  // Panasonic/Leica
        {"pef", "pictures"}, {"ptx", "pictures"},  // Pentax
        {"raf", "pictures"},  // Fuji
        {"x3f", "pictures"},  // Sigma
        {"dng", "pictures"},  // Adobe DNG
        {"erf", "pictures"},  // Epson
        {"kdc", "pictures"}, {"dcr", "pictures"},  // Kodak
        {"mef", "pictures"},  // Mamiya
        {"mrw", "pictures"},  // Minolta
        {"3fr", "pictures"}, {"fff", "pictures"},  // Hasselblad
        {"iiq", "pictures"},  // Phase One
        {"mos", "pictures"},  // Leaf
        {"bay", "pictures"},  // Casio
        {"cap", "pictures"}, {"eip", "pictures"}, {"lrv", "pictures"},
        {"r3d", "pictures"},  // RED camera
        
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
        {"xps", "books"}, {"odt", "books"}, {"ods", "books"}, {"odp", "books"},
        {"ppt", "books"}, {"pptx", "books"}, {"xls", "books"}, {"xlsx", "books"},
        {"sxw", "books"}, {"abw", "books"}, {"kwd", "books"}, {"wpd", "books"},
        {"wps", "books"}, {"sdc", "books"}, {"snb", "books"}, {"tcr", "books"},
        {"ps", "books"}, {"dvi", "books"}, {"oxps", "books"}, {"cbz", "books"},
        {"acsm", "books"}, {"htmlz", "books"}, {"txtz", "books"}, {"md", "books"},
        {"rst", "books"}, {"tex", "books"}, {"latex", "books"}, {"man", "books"},
        
        // Software (Application)
        {"exe", "software"}, {"apk", "software"}, {"rpm", "software"},
        {"deb", "software"}, {"jar", "software"}, {"bundle", "software"},
        {"com", "software"}, {"so", "software"}, {"dll", "software"},
        {"elf", "software"}, {"ipa", "software"}, {"xbe", "software"},
        {"xap", "software"}, {"a", "software"}, {"bin", "software"},
        {"msi", "software"}, {"dmg", "software"}, {"pbi", "software"},
        {"app", "software"}, {"pkg", "software"}, {"appimage", "software"},
        {"flatpak", "software"}, {"snap", "software"}, {"run", "software"},
        {"bat", "software"}, {"cmd", "software"}, {"ps1", "software"},
        {"sh", "software"}, {"bash", "software"}, {"csh", "software"},
        {"zsh", "software"}, {"fish", "software"}, {"ksh", "software"},
        {"cab", "software"}, {"gadget", "software"}, {"scr", "software"},
        {"cpl", "software"}, {"sys", "software"}, {"drv", "software"},
        {"ocx", "software"}, {"vxd", "software"}, {"ax", "software"},
        {"fon", "software"}, {"ttf", "software"}, {"otf", "software"},
        {"woff", "software"}, {"woff2", "software"}, {"eot", "software"},
        {"dylib", "software"}, {"framework", "software"},
        
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
        {"iso", "games"}, {"ecm", "games"}, {"chd", "games"}, // Compressed disc images for emulators
        // Sega
        {"gen", "games"}, {"smd", "games"}, {"md", "games"}, {"32x", "games"},
        {"gg", "games"}, {"sms", "games"}, {"sg", "games"}, {"sc", "games"},
        {"cdi", "games"}, {"gdi", "games"}, // Dreamcast
        // Atari
        {"a26", "games"}, {"a52", "games"}, {"a78", "games"}, {"j64", "games"},
        {"lnx", "games"}, {"jag", "games"}, {"atr", "games"}, {"xex", "games"},
        // PC Engine / TurboGrafx
        {"pce", "games"}, {"sgx", "games"}, {"ccd", "games"},
        // Neo Geo
        {"neo", "games"}, {"ngp", "games"}, {"ngc", "games"},
        // Wonderswan
        {"ws", "games"}, {"wsc", "games"},
        // Virtual Boy
        {"vb", "games"}, {"vboy", "games"},
        // Additional formats
        {"vpk", "games"}, {"xbx", "games"}, {"god", "games"}, // Xbox
        {"rvz", "games"}, {"gcz", "games"}, {"nkit", "games"}, // Wii/GC compressed
        {"p00", "games"}, {"prg", "games"}, {"d64", "games"}, {"t64", "games"}, // C64
        {"tap", "games"}, {"tzx", "games"}, {"z80", "games"}, {"sna", "games"}, // ZX Spectrum
        {"adf", "games"}, {"dms", "games"}, {"ipf", "games"}, // Amiga
        {"rom", "games"}, {"int", "games"}, // Intellivision
        {"col", "games"}, {"cv", "games"}, // ColecoVision
        {"vec", "games"}, // Vectrex
        {"mgd", "games"}, {"sfc", "games"}, // SNES alternate
        {"fig", "games"}, {"swc", "games"}, {"bs", "games"}, // SNES copier formats
        
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
        {"zst", "archive"}, {"tzst", "archive"}, {"7zip", "archive"},
        
        // Disc images (mapped to archive for now, since we don't have disc type)
        {"iso", "archive"}, {"mdf", "archive"}, {"mds", "archive"},
        {"nrg", "archive"}, {"ima", "archive"}, {"imz", "archive"},
        {"mdx", "archive"}, {"uif", "archive"}, {"isz", "archive"},
        {"daa", "archive"}, {"cue", "archive"}, {"fvd", "archive"},
        {"ndif", "archive"}, {"udif", "archive"}, {"vdi", "archive"},
        {"vhd", "archive"}, {"wim", "archive"}, {"vmdk", "archive"},
        {"vhdx", "archive"}, {"qcow2", "archive"}, {"img", "archive"},
        // Additional archive formats
        {"cab", "archive"}, {"cpio", "archive"}, {"pax", "archive"},
        {"shar", "archive"}, {"war", "archive"}, {"ear", "archive"},
        {"sar", "archive"}, {"lha", "archive"}, {"lzh", "archive"},
        {"zoo", "archive"}, {"bh", "archive"}, {"uue", "archive"},
        {"xxe", "archive"}, {"yenc", "archive"}, {"partimg", "archive"},
        {"lz4", "archive"}, {"snappy", "archive"}, {"br", "archive"},
        {"zstd", "archive"}, {"sz", "archive"}, {"zpaq", "archive"},
        // Korean archives
        {"egg", "archive"}, {"alz", "archive"}, {"hqx", "archive"},
        // Split archives
        {"001", "archive"}, {"r00", "archive"}, {"r01", "archive"},
        // macOS archives
        {"sea", "archive"}, {"sparseimage", "archive"},
        {"sparsebundle", "archive"}, {"toast", "archive"},
        // Additional disc images
        {"bin", "archive"}, {"c2d", "archive"}, {"bwi", "archive"},
        {"bws", "archive"}, {"ccd", "archive"}, {"cif", "archive"},
        {"dax", "archive"}, {"gi", "archive"}, {"ibq", "archive"},
        {"pdi", "archive"}, {"b6i", "archive"}, {"b5i", "archive"},
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
