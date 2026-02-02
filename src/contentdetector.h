#ifndef CONTENTDETECTOR_H
#define CONTENTDETECTOR_H

#include <QString>
#include <QVector>

// Forward declaration
struct TorrentInfo;
struct TorrentFile;

/**
 * @brief Content type enumeration
 */
enum class ContentType {
    Unknown = 0,
    Video = 1,
    Audio = 2,
    Books = 3,
    Pictures = 4,
    Software = 5,
    Games = 6,
    Archive = 7,
    Bad = 100
};

/**
 * @brief Content category enumeration
 */
enum class ContentCategory {
    Unknown = 0,
    Movie = 1,
    Series = 2,
    Documentary = 3,
    Anime = 4,
    Music = 5,
    Ebook = 6,
    Comics = 7,
    Software = 8,
    Game = 9,
    XXX = 100
};

/**
 * @brief ContentDetector - utility class for detecting and managing content types
 */
class ContentDetector
{
public:
    // =========================================================================
    // Content Type ID Conversion
    // =========================================================================
    
    /**
     * @brief Get content type ID from string
     * @param type Content type string (video, audio, books, etc.)
     * @return Content type ID
     */
    static int contentTypeId(const QString& type);
    
    /**
     * @brief Get content type string from ID
     * @param id Content type ID
     * @return Content type string
     */
    static QString contentTypeFromId(int id);
    
    // =========================================================================
    // Content Category ID Conversion
    // =========================================================================
    
    /**
     * @brief Get content category ID from string
     * @param category Content category string (movie, series, xxx, etc.)
     * @return Content category ID
     */
    static int contentCategoryId(const QString& category);
    
    /**
     * @brief Get content category string from ID
     * @param id Content category ID
     * @return Content category string
     */
    static QString contentCategoryFromId(int id);
    
    // =========================================================================
    // Content Detection
    // =========================================================================
    
    /**
     * @brief Detect content type and category from torrent name and files
     * @param torrent TorrentInfo to analyze and update
     */
    static void detectContentType(TorrentInfo& torrent);
    
    /**
     * @brief Detect content type from file list
     * @param files List of files to analyze
     * @return Detected content type ID
     */
    static int detectContentTypeFromFiles(const QVector<TorrentFile>& files);
    
    /**
     * @brief Check if torrent name contains adult keywords
     * @param name Torrent name to check
     * @return true if adult content detected
     */
    static bool isAdultContent(const QString& name);
};

#endif // CONTENTDETECTOR_H
