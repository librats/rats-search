#include "trackerinfoscraper.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QTimer>
#include <QDebug>
#include <QJsonDocument>
#include <QStringDecoder>
#include <QUrl>

// ============================================================================
// Constructor / Destructor
// ============================================================================

TrackerInfoScraper::TrackerInfoScraper(QObject *parent)
    : QObject(parent)
    , networkManager_(new QNetworkAccessManager(this))
{
}

TrackerInfoScraper::~TrackerInfoScraper()
{
}

// ============================================================================
// Public Methods
// ============================================================================

void TrackerInfoScraper::scrapeAll(const QString& hash,
                                    const QJsonObject& existingInfo,
                                    MergedCallback callback)
{
    if (!enabled_ || hash.length() != 40) {
        if (callback) callback(hash, existingInfo);
        return;
    }
    
    // Check if already has tracker info
    if (existingInfo.contains("trackers")) {
        QJsonArray trackers = existingInfo["trackers"].toArray();
        if (!trackers.isEmpty()) {
            qDebug() << "TrackerInfoScraper: Hash" << hash.left(8) 
                     << "already has tracker info, skipping";
            if (callback) callback(hash, existingInfo);
            return;
        }
    }
    
    // Check cooldown
    {
        QMutexLocker locker(&recentChecksMutex_);
        if (recentChecks_.contains(hash)) {
            QDateTime lastCheck = recentChecks_[hash];
            if (lastCheck.secsTo(QDateTime::currentDateTime()) < cooldownSecs_) {
                qDebug() << "TrackerInfoScraper: Hash" << hash.left(8) 
                         << "checked recently, skipping";
                if (callback) callback(hash, existingInfo);
                return;
            }
        }
        recentChecks_[hash] = QDateTime::currentDateTime();
    }
    
    // Setup pending scrape tracking
    {
        QMutexLocker locker(&pendingMutex_);
        PendingScrape pending;
        pending.existingInfo = existingInfo;
        pending.callback = callback;
        pending.pendingCount = STRATEGY_COUNT;
        pendingScrapes_[hash] = pending;
    }
    
    qInfo() << "TrackerInfoScraper: Scraping tracker info for" << hash.left(16);
    
    // Launch all strategies in parallel
    scrapeRutracker(hash);
    scrapeNyaa(hash);
}

bool TrackerInfoScraper::wasRecentlyScraped(const QString& hash) const
{
    QMutexLocker locker(&recentChecksMutex_);
    if (!recentChecks_.contains(hash)) {
        return false;
    }
    return recentChecks_[hash].secsTo(QDateTime::currentDateTime()) < cooldownSecs_;
}

// ============================================================================
// RuTracker Strategy
// ============================================================================

void TrackerInfoScraper::scrapeRutracker(const QString& hash)
{
    // RuTracker allows searching by info hash via ?h= parameter
    // URL: https://rutracker.org/forum/viewtopic.php?h=HASH
    QUrl url(QString("https://rutracker.org/forum/viewtopic.php?h=%1").arg(hash));
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, 
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    request.setRawHeader("Accept", "text/html,application/xhtml+xml");
    request.setRawHeader("Accept-Language", "ru-RU,ru;q=0.9,en-US;q=0.8,en;q=0.7");
    request.setTransferTimeout(timeoutMs_);
    
    QNetworkReply* reply = networkManager_->get(request);
    
    connect(reply, &QNetworkReply::finished, this, [this, reply, hash]() {
        reply->deleteLater();
        
        TrackerScrapedInfo info;
        info.trackerName = "rutracker";
        
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray rawData = reply->readAll();
            info = parseRutrackerHtml(rawData);
        } else {
            qDebug() << "TrackerInfoScraper: RuTracker request failed:" 
                     << reply->errorString();
        }
        
        onStrategyComplete(hash, info);
    });
}

TrackerScrapedInfo TrackerInfoScraper::parseRutrackerHtml(const QByteArray& rawData)
{
    TrackerScrapedInfo info;
    info.trackerName = "rutracker";
    
    if (rawData.isEmpty()) {
        return info;
    }
    
    // RuTracker pages are typically windows-1251 encoded
    // Check Content-Type or meta tag for encoding
    QString html;
    
    // Try to detect encoding from meta tag
    QString rawPreview = QString::fromLatin1(rawData.left(2000));
    if (rawPreview.contains("windows-1251", Qt::CaseInsensitive) || 
        rawPreview.contains("charset=windows-1251", Qt::CaseInsensitive)) {
        auto decoder = QStringDecoder(QStringDecoder::System);
        // Try windows-1251 decoder
        auto win1251 = QStringDecoder("windows-1251");
        qDebug() << "TrackerInfoScraper: Windows-1251 detected";
        if (win1251.isValid()) {
            html = win1251(rawData);
        } else {
            qDebug() << "TrackerInfoScraper: Windows-1251 decoder not available, using UTF-8";
            html = QString::fromUtf8(rawData);
        }
    } else {
        html = QString::fromUtf8(rawData);
    }
    
    if (html.isEmpty()) {
        return info;
    }
    
    // Pre-process: add newlines before post-br and post-b (like legacy)
    html.replace(QRegularExpression("<span class=\"post-br\">"), "\n<span class=\"post-br\">");
    html.replace(QRegularExpression("><span class=\"post-b\">"), ">\n<span class=\"post-b\">");
    
    // Extract topic title: <a id="topic-title" ...>TITLE</a>
    // Also handles: <a ... id="topic-title" ...>TITLE</a>
    {
        QRegularExpression re(R"(<a[^>]*id\s*=\s*"topic-title"[^>]*>(.*?)</a>)", 
                             QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatch match = re.match(html);
        if (match.hasMatch()) {
            info.name = stripHtml(match.captured(1)).trimmed();
        }
    }
    
    // If no topic title found, the page didn't match - return empty
    if (info.name.isEmpty()) {
        return info;
    }
    
    // Extract poster image:
    // <var class="postImgAligned" title="URL"> or <var class="postImg" title="URL">
    // Also: <img class="postImgAligned" title="URL"> or src="URL"
    {
        QRegularExpression re(R"re(<(?:var|img)[^>]*class\s*=\s*"postImg(?:Aligned)?"[^>]*title\s*=\s*"([^"]+)")re");
        QRegularExpressionMatch match = re.match(html);
        if (match.hasMatch()) {
            info.poster = match.captured(1).trimmed();
        }
    }
    
    // Extract description: first <div class="post_body"> ... </div>
    // This is complex because post_body can contain nested divs
    // Use a simplified approach: find the first post_body and extract a reasonable chunk
    {
        QRegularExpression re(R"(<div[^>]*class\s*=\s*"post_body"[^>]*>(.*?)</div\s*>\s*</td>)",
                             QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatch match = re.match(html);
        if (match.hasMatch()) {
            QString desc = match.captured(1);
            // Strip HTML tags and clean up
            desc = stripHtml(desc);
            // Limit description length
            if (desc.length() > 5000) {
                desc = desc.left(5000) + "...";
            }
            info.description = desc.trimmed();
        }
    }
    
    // Extract thread ID from magnet link:
    // <a ... class="magnet-link" ... data-topic_id="12345">
    // or <a ... class="magnet-link-1" ... data-topic_id="12345">
    {
        QRegularExpression re(R"re(<a[^>]*class\s*=\s*"magnet-link(?:-1)?"[^>]*data-topic_id\s*=\s*"(\d+)")re");
        QRegularExpressionMatch match = re.match(html);
        if (match.hasMatch()) {
            info.threadId = match.captured(1).toInt();
            info.threadUrl = QString("https://rutracker.org/forum/viewtopic.php?t=%1").arg(info.threadId);
        }
    }
    
    // Extract content category from navigation breadcrumb:
    // <td class="vBottom"> ... <div class="nav"> ... </div>
    {
        QRegularExpression re(R"(<td[^>]*class\s*=\s*"vBottom"[^>]*>.*?<[^>]*class\s*=\s*"nav"[^>]*>(.*?)</(?:div|td)>)",
                             QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatch match = re.match(html);
        if (match.hasMatch()) {
            QString cat = stripHtml(match.captured(1));
            // Clean up: remove excessive whitespace
            cat = cat.replace(QRegularExpression("\\s+"), " ").trimmed();
            if (!cat.isEmpty()) {
                info.contentCategory = cat;
            }
        }
    }
    
    info.success = true;
    qInfo() << "TrackerInfoScraper: RuTracker found:" << info.name.left(60)
            << "threadId:" << info.threadId;
    
    return info;
}

// ============================================================================
// Nyaa Strategy
// ============================================================================

void TrackerInfoScraper::scrapeNyaa(const QString& hash)
{
    // Nyaa allows searching by info hash
    // URL: https://nyaa.si/?q=HASH
    QUrl url(QString("https://nyaa.si/?q=%1").arg(hash));
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, 
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    request.setRawHeader("Accept", "text/html,application/xhtml+xml");
    request.setTransferTimeout(timeoutMs_);
    
    QNetworkReply* reply = networkManager_->get(request);
    
    connect(reply, &QNetworkReply::finished, this, [this, reply, hash]() {
        reply->deleteLater();
        
        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "TrackerInfoScraper: Nyaa request failed:" 
                     << reply->errorString();
            TrackerScrapedInfo info;
            info.trackerName = "nyaa";
            onStrategyComplete(hash, info);
            return;
        }
        
        QByteArray rawData = reply->readAll();
        
        // Check if we got redirected to a view page (single result)
        QUrl finalUrl = reply->url();
        if (finalUrl.path().startsWith("/view/")) {
            // Already on a view page - parse directly
            TrackerScrapedInfo info = parseNyaaViewHtml(rawData);
            if (info.success && info.threadUrl.isEmpty()) {
                info.threadUrl = finalUrl.toString();
            }
            onStrategyComplete(hash, info);
            return;
        }
        
        // Parse search results to find a view link
        TrackerScrapedInfo searchInfo = parseNyaaSearchHtml(rawData);
        
        if (searchInfo.success && searchInfo.threadId > 0) {
            // Found a result - fetch the view page for full details
            scrapeNyaaViewPage(hash, 
                QString("https://nyaa.si/view/%1").arg(searchInfo.threadId));
        } else if (searchInfo.success) {
            // Got some info from search page directly
            onStrategyComplete(hash, searchInfo);
        } else {
            // No results on Nyaa
            TrackerScrapedInfo emptyInfo;
            emptyInfo.trackerName = "nyaa";
            onStrategyComplete(hash, emptyInfo);
        }
    });
}

void TrackerInfoScraper::scrapeNyaaViewPage(const QString& hash, const QString& viewUrl)
{
    QUrl url(viewUrl);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, 
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    request.setRawHeader("Accept", "text/html,application/xhtml+xml");
    request.setTransferTimeout(timeoutMs_);
    
    QNetworkReply* reply = networkManager_->get(request);
    
    connect(reply, &QNetworkReply::finished, this, [this, reply, hash, viewUrl]() {
        reply->deleteLater();
        
        TrackerScrapedInfo info;
        info.trackerName = "nyaa";
        
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray rawData = reply->readAll();
            info = parseNyaaViewHtml(rawData);
            if (info.success) {
                info.threadUrl = viewUrl;
            }
        } else {
            qDebug() << "TrackerInfoScraper: Nyaa view page request failed:" 
                     << reply->errorString();
        }
        
        onStrategyComplete(hash, info);
    });
}

TrackerScrapedInfo TrackerInfoScraper::parseNyaaSearchHtml(const QByteArray& rawData)
{
    TrackerScrapedInfo info;
    info.trackerName = "nyaa";
    
    QString html = QString::fromUtf8(rawData);
    if (html.isEmpty()) {
        return info;
    }
    
    // Look for a view link in search results:
    // <td ...><a href="/view/1234567" ...>Title</a></td>
    // Nyaa search results table has links like /view/ID
    {
        QRegularExpression re(R"re(<a[^>]*href\s*=\s*"/view/(\d+)"[^>]*>([^<]+)</a>)re");
        QRegularExpressionMatch match = re.match(html);
        if (match.hasMatch()) {
            info.threadId = match.captured(1).toInt();
            info.name = match.captured(2).trimmed();
            info.success = true;
        }
    }
    
    // Also try to extract title from panel-title (if single result / detail view)
    if (!info.success) {
        QRegularExpression re(R"(<h3[^>]*class\s*=\s*"panel-title"[^>]*>(.*?)</h3>)",
                             QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatch match = re.match(html);
        if (match.hasMatch()) {
            QString title = stripHtml(match.captured(1)).trimmed();
            title.replace(QRegularExpression("[\\t\\n]+"), "");
            if (!title.isEmpty() && title != "Nyaa") {
                info.name = title;
                info.success = true;
            }
        }
    }
    
    // Try to get description if on a detail page
    {
        QRegularExpression re(R"(<div[^>]*id\s*=\s*"torrent-description"[^>]*>(.*?)</div>)",
                             QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatch match = re.match(html);
        if (match.hasMatch()) {
            info.description = stripHtml(match.captured(1)).trimmed();
            if (info.description.length() > 5000) {
                info.description = info.description.left(5000) + "...";
            }
        }
    }
    
    return info;
}

TrackerScrapedInfo TrackerInfoScraper::parseNyaaViewHtml(const QByteArray& rawData)
{
    TrackerScrapedInfo info;
    info.trackerName = "nyaa";
    
    QString html = QString::fromUtf8(rawData);
    if (html.isEmpty()) {
        return info;
    }
    
    // Extract title from panel-title
    {
        QRegularExpression re(R"(<h3[^>]*class\s*=\s*"panel-title"[^>]*>(.*?)</h3>)",
                             QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatch match = re.match(html);
        if (match.hasMatch()) {
            QString title = stripHtml(match.captured(1)).trimmed();
            title.replace(QRegularExpression("[\\t\\n]+"), "");
            if (!title.isEmpty() && title != "Nyaa") {
                info.name = title;
                info.success = true;
            }
        }
    }
    
    if (!info.success) {
        return info;
    }
    
    // Extract description
    {
        QRegularExpression re(R"(<div[^>]*id\s*=\s*"torrent-description"[^>]*>(.*?)</div>)",
                             QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatch match = re.match(html);
        if (match.hasMatch()) {
            info.description = stripHtml(match.captured(1)).trimmed();
            if (info.description.length() > 5000) {
                info.description = info.description.left(5000) + "...";
            }
        }
    }
    
    // Extract view ID from URL in the page (for thread linking)
    {
        QRegularExpression re(R"(/view/(\d+))");
        QRegularExpressionMatch match = re.match(html);
        if (match.hasMatch()) {
            info.threadId = match.captured(1).toInt();
        }
    }
    
    qInfo() << "TrackerInfoScraper: Nyaa found:" << info.name.left(60);
    
    return info;
}

// ============================================================================
// Result Merging
// ============================================================================

void TrackerInfoScraper::onStrategyComplete(const QString& hash, const TrackerScrapedInfo& info)
{
    if (info.success) {
        emit trackerInfoFound(hash, info.trackerName, info);
    }
    
    {
        QMutexLocker locker(&pendingMutex_);
        auto it = pendingScrapes_.find(hash);
        if (it == pendingScrapes_.end()) {
            return;
        }
        
        if (info.success) {
            it->results.append(info);
        }
        it->pendingCount--;
    }
    
    checkAllComplete(hash);
}

void TrackerInfoScraper::checkAllComplete(const QString& hash)
{
    QMutexLocker locker(&pendingMutex_);
    auto it = pendingScrapes_.find(hash);
    if (it == pendingScrapes_.end()) {
        return;
    }
    
    if (it->pendingCount > 0) {
        return;  // Still waiting for some strategies
    }
    
    // All strategies complete - merge results
    QJsonObject mergedInfo = it->existingInfo;
    QJsonArray trackers;
    
    // Preserve existing trackers
    if (mergedInfo.contains("trackers")) {
        trackers = mergedInfo["trackers"].toArray();
    }
    
    for (const TrackerScrapedInfo& result : it->results) {
        // Add tracker to list (deduplicated)
        bool alreadyListed = false;
        for (const QJsonValue& t : trackers) {
            if (t.toString() == result.trackerName) {
                alreadyListed = true;
                break;
            }
        }
        if (!alreadyListed) {
            trackers.append(result.trackerName);
        }
        
        // Merge fields - first found wins for poster/description
        if (mergedInfo["poster"].toString().isEmpty() && !result.poster.isEmpty()) {
            mergedInfo["poster"] = result.poster;
        }
        if (mergedInfo["description"].toString().isEmpty() && !result.description.isEmpty()) {
            mergedInfo["description"] = result.description;
        }
        
        // Store tracker-specific data
        if (result.trackerName == "rutracker") {
            if (result.threadId > 0) {
                mergedInfo["rutrackerThreadId"] = result.threadId;
            }
            if (!result.contentCategory.isEmpty()) {
                mergedInfo["contentCategory"] = result.contentCategory;
            }
            if (!result.name.isEmpty() && !mergedInfo.contains("trackerName")) {
                mergedInfo["trackerName"] = result.name;
            }
        } else if (result.trackerName == "nyaa") {
            if (result.threadId > 0) {
                mergedInfo["nyaaThreadId"] = result.threadId;
            }
        }
    }
    
    if (!trackers.isEmpty()) {
        mergedInfo["trackers"] = trackers;
    }
    
    MergedCallback callback = it->callback;
    pendingScrapes_.erase(it);
    locker.unlock();
    
    // Emit signal and call callback
    if (!trackers.isEmpty()) {
        emit scrapeCompleted(hash, mergedInfo);
    }
    
    if (callback) {
        callback(hash, mergedInfo);
    }
}

// ============================================================================
// HTML Parsing Helpers
// ============================================================================

QString TrackerInfoScraper::extractTagContent(const QString& html, const QString& tagPattern)
{
    QRegularExpression re(tagPattern, QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatch match = re.match(html);
    if (match.hasMatch()) {
        return match.captured(1);
    }
    return QString();
}

QString TrackerInfoScraper::extractAttribute(const QString& html, const QString& elementPattern,
                                              const QString& attrName)
{
    QRegularExpression re(elementPattern);
    QRegularExpressionMatch match = re.match(html);
    if (match.hasMatch()) {
        QString element = match.captured(0);
        QRegularExpression attrRe(QString(R"re(%1\s*=\s*"([^"]*)")re").arg(
            QRegularExpression::escape(attrName)));
        QRegularExpressionMatch attrMatch = attrRe.match(element);
        if (attrMatch.hasMatch()) {
            return attrMatch.captured(1);
        }
    }
    return QString();
}

QString TrackerInfoScraper::stripHtml(const QString& html)
{
    QString text = html;
    
    // Replace <br>, <br/>, <br /> with newlines
    text.replace(QRegularExpression("<br\\s*/?>", QRegularExpression::CaseInsensitiveOption), "\n");
    
    // Replace block-level tags with newlines
    text.replace(QRegularExpression("</(?:p|div|li|tr|h[1-6])>", QRegularExpression::CaseInsensitiveOption), "\n");
    
    // Remove all remaining HTML tags
    text.replace(QRegularExpression("<[^>]*>"), "");
    
    // Decode common HTML entities
    text.replace("&amp;", "&");
    text.replace("&lt;", "<");
    text.replace("&gt;", ">");
    text.replace("&quot;", "\"");
    text.replace("&apos;", "'");
    text.replace("&#39;", "'");
    text.replace("&nbsp;", " ");
    text.replace("&#160;", " ");
    
    // Decode numeric entities
    QRegularExpression numEntityRe("&#(\\d+);");
    QRegularExpressionMatchIterator numIt = numEntityRe.globalMatch(text);
    while (numIt.hasNext()) {
        QRegularExpressionMatch m = numIt.next();
        int code = m.captured(1).toInt();
        if (code > 0 && code < 0x10FFFF) {
            text.replace(m.captured(0), QChar(code));
        }
    }
    
    // Collapse multiple blank lines to max 2
    text.replace(QRegularExpression("\\n{3,}"), "\n\n");
    
    // Remove leading/trailing whitespace from each line
    QStringList lines = text.split('\n');
    for (QString& line : lines) {
        line = line.trimmed();
    }
    text = lines.join('\n');
    
    return text.trimmed();
}
