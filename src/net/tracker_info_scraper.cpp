#include "net/tracker_info_scraper.h"

#include <QDebug>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>

namespace rats::net {

namespace {

// User-Agent shared by every request; trackers gate some content on it.
constexpr const char* kUserAgent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36";

} // namespace

// The authoritative strategy list. Its size — not a hardcoded constant — is
// what every scrape uses as its pending-result count.
const QVector<TrackerInfoScraper::Strategy> TrackerInfoScraper::kStrategies = {
    &TrackerInfoScraper::scrapeRutracker,
    &TrackerInfoScraper::scrapeNyaa,
};

// ============================================================================
// Construction
// ============================================================================

TrackerInfoScraper::TrackerInfoScraper(QObject* parent)
    : QObject(parent), networkManager_(new QNetworkAccessManager(this))
{
}

TrackerInfoScraper::~TrackerInfoScraper() = default;

// ============================================================================
// Public entry point
// ============================================================================

void TrackerInfoScraper::scrape(const QString& infoHash, const QString& name)
{
    if (!enabled_ || infoHash.length() != kInfoHashHexLength) {
        return;
    }

    // Per-hash cooldown: skip anything scraped inside the cooldown window.
    {
        QMutexLocker locker(&recentChecksMutex_);
        if (recentChecks_.contains(infoHash)) {
            const QDateTime lastCheck = recentChecks_[infoHash];
            if (lastCheck.secsTo(QDateTime::currentDateTime()) < cooldownSecs_) {
                qDebug() << "TrackerInfoScraper: Hash" << infoHash.left(8) << "checked recently, skipping";
                return;
            }
        }
        recentChecks_[infoHash] = QDateTime::currentDateTime();
    }

    // Register the pending scrape. The number of results we wait on is derived
    // from the strategy list, not a hardcoded STRATEGY_COUNT.
    {
        QMutexLocker locker(&pendingMutex_);
        PendingScrape pending;
        pending.name = name;
        pending.pendingCount = static_cast<int>(kStrategies.size());
        pendingScrapes_[infoHash] = pending;
    }

    qInfo() << "TrackerInfoScraper: Scraping tracker info for" << infoHash.left(16) << name.left(48);

    // Launch every strategy in parallel.
    for (const Strategy strategy : kStrategies) {
        (this->*strategy)(infoHash);
    }
}

// ============================================================================
// RuTracker strategy
// ============================================================================

void TrackerInfoScraper::scrapeRutracker(const QString& hash)
{
    // RuTracker allows searching by info hash via the ?h= parameter.
    QUrl url(QString("https://rutracker.org/forum/viewtopic.php?h=%1").arg(hash));

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, kUserAgent);
    request.setRawHeader("Accept", "text/html,application/xhtml+xml");
    request.setRawHeader("Accept-Language", "ru-RU,ru;q=0.9,en-US;q=0.8,en;q=0.7");
    request.setTransferTimeout(timeoutMs_);

    QNetworkReply* reply = networkManager_->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, hash]() {
        reply->deleteLater();

        TrackerScrapedInfo info;
        info.trackerName = "rutracker";

        if (reply->error() == QNetworkReply::NoError) {
            const QByteArray rawData = reply->readAll();
            info = parseRutrackerHtml(rawData);
        } else {
            qDebug() << "TrackerInfoScraper: RuTracker request failed:" << reply->errorString();
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

    // RuTracker pages are typically windows-1251 encoded. Sniff the head of the
    // document for the charset before deciding how to decode.
    QString html;
    const QString rawPreview = QString::fromLatin1(rawData.left(kEncodingSniffLength));
    if (rawPreview.contains("windows-1251", Qt::CaseInsensitive)
        || rawPreview.contains("charset=windows-1251", Qt::CaseInsensitive)) {
        qDebug() << "TrackerInfoScraper: Windows-1251 detected, decoding";
        html = decodeWindows1251(rawData);
    } else {
        html = QString::fromUtf8(rawData);
    }

    if (html.isEmpty()) {
        return info;
    }

    // Pre-process: add newlines before post-br and post-b (like legacy).
    html.replace(QRegularExpression("<span class=\"post-br\">"), "\n<span class=\"post-br\">");
    html.replace(QRegularExpression("><span class=\"post-b\">"), ">\n<span class=\"post-b\">");

    // Extract topic title: <a id="topic-title" ...>TITLE</a>
    // Also handles: <a ... id="topic-title" ...>TITLE</a>
    {
        QRegularExpression re(
            R"(<a[^>]*id\s*=\s*"topic-title"[^>]*>(.*?)</a>)", QRegularExpression::DotMatchesEverythingOption);
        const QRegularExpressionMatch match = re.match(html);
        if (match.hasMatch()) {
            info.name = stripHtml(match.captured(1)).trimmed();
        }
    }

    // If no topic title found, the page didn't match — return empty.
    if (info.name.isEmpty()) {
        return info;
    }

    // Extract poster image:
    // <var class="postImgAligned" title="URL"> or <var class="postImg"
    // title="URL"> Also: <img class="postImgAligned" title="URL"> or src="URL"
    {
        QRegularExpression re(R"re(<(?:var|img)[^>]*class\s*=\s*"postImg(?:Aligned)?"[^>]*title\s*=\s*"([^"]+)")re");
        const QRegularExpressionMatch match = re.match(html);
        if (match.hasMatch()) {
            info.poster = match.captured(1).trimmed();
        }
    }

    // Extract description: first <div class="post_body"> ... </div>.
    // Simplified approach — grab the first post_body and clamp its length.
    {
        QRegularExpression re(R"(<div[^>]*class\s*=\s*"post_body"[^>]*>(.*?)</div\s*>\s*</td>)",
            QRegularExpression::DotMatchesEverythingOption);
        const QRegularExpressionMatch match = re.match(html);
        if (match.hasMatch()) {
            const QString desc = stripHtml(match.captured(1));
            info.description = truncateDescription(desc).trimmed();
        }
    }

    // Extract thread ID from magnet link:
    // <a ... class="magnet-link" ... data-topic_id="12345">
    // or <a ... class="magnet-link-1" ... data-topic_id="12345">
    {
        QRegularExpression re(R"re(<a[^>]*class\s*=\s*"magnet-link(?:-1)?"[^>]*data-topic_id\s*=\s*"(\d+)")re");
        const QRegularExpressionMatch match = re.match(html);
        if (match.hasMatch()) {
            info.threadId = match.captured(1).toInt();
            info.threadUrl = QString("https://rutracker.org/forum/viewtopic.php?t=%1").arg(info.threadId);
        }
    }

    // Extract content category from navigation breadcrumb:
    // <td class="vBottom"> ... <div class="nav"> ... </div>
    {
        QRegularExpression re(
            R"(<td[^>]*class\s*=\s*"vBottom"[^>]*>.*?<[^>]*class\s*=\s*"nav"[^>]*>(.*?)</(?:div|td)>)",
            QRegularExpression::DotMatchesEverythingOption);
        const QRegularExpressionMatch match = re.match(html);
        if (match.hasMatch()) {
            QString cat = stripHtml(match.captured(1));
            cat = cat.replace(QRegularExpression("\\s+"), " ").trimmed();
            if (!cat.isEmpty()) {
                info.contentCategory = cat;
            }
        }
    }

    info.success = true;
    qInfo() << "TrackerInfoScraper: RuTracker found:" << info.name.left(60) << "threadId:" << info.threadId;

    return info;
}

// ============================================================================
// Nyaa strategy
// ============================================================================

void TrackerInfoScraper::scrapeNyaa(const QString& hash)
{
    // Nyaa allows searching by info hash via ?q=.
    QUrl url(QString("https://nyaa.si/?q=%1").arg(hash));

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, kUserAgent);
    request.setRawHeader("Accept", "text/html,application/xhtml+xml");
    request.setTransferTimeout(timeoutMs_);

    QNetworkReply* reply = networkManager_->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, hash]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "TrackerInfoScraper: Nyaa request failed:" << reply->errorString();
            TrackerScrapedInfo info;
            info.trackerName = "nyaa";
            onStrategyComplete(hash, info);
            return;
        }

        const QByteArray rawData = reply->readAll();

        // A single search result redirects straight to the /view/ page.
        const QUrl finalUrl = reply->url();
        if (finalUrl.path().startsWith("/view/")) {
            TrackerScrapedInfo info = parseNyaaViewHtml(rawData);
            if (info.success && info.threadUrl.isEmpty()) {
                info.threadUrl = finalUrl.toString();
            }
            onStrategyComplete(hash, info);
            return;
        }

        // Otherwise parse the search results to locate a view link.
        const TrackerScrapedInfo searchInfo = parseNyaaSearchHtml(rawData);

        if (searchInfo.success && searchInfo.threadId > 0) {
            // Found a result — fetch the view page for full details.
            scrapeNyaaViewPage(hash, QString("https://nyaa.si/view/%1").arg(searchInfo.threadId));
        } else if (searchInfo.success) {
            // Got some info directly from the search page.
            onStrategyComplete(hash, searchInfo);
        } else {
            // No results on Nyaa.
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
    request.setHeader(QNetworkRequest::UserAgentHeader, kUserAgent);
    request.setRawHeader("Accept", "text/html,application/xhtml+xml");
    request.setTransferTimeout(timeoutMs_);

    QNetworkReply* reply = networkManager_->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, hash, viewUrl]() {
        reply->deleteLater();

        TrackerScrapedInfo info;
        info.trackerName = "nyaa";

        if (reply->error() == QNetworkReply::NoError) {
            const QByteArray rawData = reply->readAll();
            info = parseNyaaViewHtml(rawData);
            if (info.success) {
                info.threadUrl = viewUrl;
            }
        } else {
            qDebug() << "TrackerInfoScraper: Nyaa view page request failed:" << reply->errorString();
        }

        onStrategyComplete(hash, info);
    });
}

TrackerScrapedInfo TrackerInfoScraper::parseNyaaSearchHtml(const QByteArray& rawData)
{
    TrackerScrapedInfo info;
    info.trackerName = "nyaa";

    const QString html = QString::fromUtf8(rawData);
    if (html.isEmpty()) {
        return info;
    }

    // Look for a view link in the search results:
    // <td ...><a href="/view/1234567" ...>Title</a></td>
    {
        QRegularExpression re(R"re(<a[^>]*href\s*=\s*"/view/(\d+)"[^>]*>([^<]+)</a>)re");
        const QRegularExpressionMatch match = re.match(html);
        if (match.hasMatch()) {
            info.threadId = match.captured(1).toInt();
            info.name = match.captured(2).trimmed();
            info.success = true;
        }
    }

    // Also try the panel-title (single result / detail view).
    if (!info.success) {
        QRegularExpression re(
            R"(<h3[^>]*class\s*=\s*"panel-title"[^>]*>(.*?)</h3>)", QRegularExpression::DotMatchesEverythingOption);
        const QRegularExpressionMatch match = re.match(html);
        if (match.hasMatch()) {
            QString title = stripHtml(match.captured(1)).trimmed();
            title.replace(QRegularExpression("[\\t\\n]+"), "");
            if (!title.isEmpty() && title != "Nyaa") {
                info.name = title;
                info.success = true;
            }
        }
    }

    // Grab the description if we happen to be on a detail page.
    {
        QRegularExpression re(R"(<div[^>]*id\s*=\s*"torrent-description"[^>]*>(.*?)</div>)",
            QRegularExpression::DotMatchesEverythingOption);
        const QRegularExpressionMatch match = re.match(html);
        if (match.hasMatch()) {
            info.description = truncateDescription(stripHtml(match.captured(1)).trimmed());
        }
    }

    return info;
}

TrackerScrapedInfo TrackerInfoScraper::parseNyaaViewHtml(const QByteArray& rawData)
{
    TrackerScrapedInfo info;
    info.trackerName = "nyaa";

    const QString html = QString::fromUtf8(rawData);
    if (html.isEmpty()) {
        return info;
    }

    // Extract title from panel-title.
    {
        QRegularExpression re(
            R"(<h3[^>]*class\s*=\s*"panel-title"[^>]*>(.*?)</h3>)", QRegularExpression::DotMatchesEverythingOption);
        const QRegularExpressionMatch match = re.match(html);
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

    // Extract description.
    {
        QRegularExpression re(R"(<div[^>]*id\s*=\s*"torrent-description"[^>]*>(.*?)</div>)",
            QRegularExpression::DotMatchesEverythingOption);
        const QRegularExpressionMatch match = re.match(html);
        if (match.hasMatch()) {
            info.description = truncateDescription(stripHtml(match.captured(1)).trimmed());
        }
    }

    // Extract view ID from a URL in the page (for thread linking).
    {
        QRegularExpression re(R"(/view/(\d+))");
        const QRegularExpressionMatch match = re.match(html);
        if (match.hasMatch()) {
            info.threadId = match.captured(1).toInt();
        }
    }

    qInfo() << "TrackerInfoScraper: Nyaa found:" << info.name.left(60);

    return info;
}

// ============================================================================
// Result merging
// ============================================================================

void TrackerInfoScraper::onStrategyComplete(const QString& hash, const TrackerScrapedInfo& info)
{
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
        return; // still waiting for some strategies
    }

    // All strategies complete — fold the results into a single JSON object. Only
    // the freshly scraped keys are emitted; the listener merges them into the
    // stored torrent.
    QJsonObject info;
    QJsonArray trackers;

    for (const TrackerScrapedInfo& result : it->results) {
        // Add tracker to the list (deduplicated).
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

        // Merge shared fields — first found wins for poster / description.
        if (info["poster"].toString().isEmpty() && !result.poster.isEmpty()) {
            info["poster"] = result.poster;
        }
        if (info["description"].toString().isEmpty() && !result.description.isEmpty()) {
            info["description"] = result.description;
        }

        // Tracker-specific payloads.
        if (result.trackerName == "rutracker") {
            if (result.threadId > 0) {
                info["rutrackerThreadId"] = result.threadId;
            }
            if (!result.contentCategory.isEmpty()) {
                info["contentCategory"] = result.contentCategory;
            }
            if (!result.name.isEmpty() && !info.contains("trackerName")) {
                info["trackerName"] = result.name;
            }
        } else if (result.trackerName == "nyaa") {
            if (result.threadId > 0) {
                info["nyaaThreadId"] = result.threadId;
            }
        }
    }

    if (!trackers.isEmpty()) {
        info["trackers"] = trackers;
    }

    pendingScrapes_.erase(it);
    locker.unlock();

    // Only emit when at least one tracker produced data.
    if (!trackers.isEmpty()) {
        emit scraped(hash, info);
    }
}

// ============================================================================
// HTML helpers
// ============================================================================

QString TrackerInfoScraper::truncateDescription(const QString& text)
{
    if (text.length() > kMaxDescriptionLength) {
        return text.left(kMaxDescriptionLength) + "...";
    }
    return text;
}

QString TrackerInfoScraper::stripHtml(const QString& html)
{
    QString text = html;

    // Replace <br>, <br/>, <br /> with newlines.
    text.replace(QRegularExpression("<br\\s*/?>", QRegularExpression::CaseInsensitiveOption), "\n");

    // Replace block-level tags with newlines.
    text.replace(QRegularExpression("</(?:p|div|li|tr|h[1-6])>", QRegularExpression::CaseInsensitiveOption), "\n");

    // Remove all remaining HTML tags.
    text.replace(QRegularExpression("<[^>]*>"), "");

    // Decode common HTML entities.
    text.replace("&amp;", "&");
    text.replace("&lt;", "<");
    text.replace("&gt;", ">");
    text.replace("&quot;", "\"");
    text.replace("&apos;", "'");
    text.replace("&#39;", "'");
    text.replace("&nbsp;", " ");
    text.replace("&#160;", " ");

    // Decode numeric entities.
    QRegularExpression numEntityRe("&#(\\d+);");
    QRegularExpressionMatchIterator numIt = numEntityRe.globalMatch(text);
    while (numIt.hasNext()) {
        const QRegularExpressionMatch m = numIt.next();
        const int code = m.captured(1).toInt();
        if (code > 0 && code < 0x10FFFF) {
            text.replace(m.captured(0), QChar(code));
        }
    }

    // Collapse runs of blank lines to at most two.
    text.replace(QRegularExpression("\\n{3,}"), "\n\n");

    // Trim whitespace from each line.
    QStringList lines = text.split('\n');
    for (QString& line : lines) {
        line = line.trimmed();
    }
    text = lines.join('\n');

    return text.trimmed();
}

QString TrackerInfoScraper::decodeWindows1251(const QByteArray& data)
{
    // Windows-1251 to Unicode mapping for bytes 0x80-0xFF
    static const char16_t win1251table[128] = {
        // 0x80-0x8F
        0x0402,
        0x0403,
        0x201A,
        0x0453,
        0x201E,
        0x2026,
        0x2020,
        0x2021,
        0x20AC,
        0x2030,
        0x0409,
        0x2039,
        0x040A,
        0x040C,
        0x040B,
        0x040F,
        // 0x90-0x9F
        0x0452,
        0x2018,
        0x2019,
        0x201C,
        0x201D,
        0x2022,
        0x2013,
        0x2014,
        0x0098,
        0x2122,
        0x0459,
        0x203A,
        0x045A,
        0x045C,
        0x045B,
        0x045F,
        // 0xA0-0xAF
        0x00A0,
        0x040E,
        0x045E,
        0x0408,
        0x00A4,
        0x0490,
        0x00A6,
        0x00A7,
        0x0401,
        0x00A9,
        0x0404,
        0x00AB,
        0x00AC,
        0x00AD,
        0x00AE,
        0x0407,
        // 0xB0-0xBF
        0x00B0,
        0x00B1,
        0x0406,
        0x0456,
        0x0491,
        0x00B5,
        0x00B6,
        0x00B7,
        0x0451,
        0x2116,
        0x0454,
        0x00BB,
        0x0458,
        0x0405,
        0x0455,
        0x0457,
        // 0xC0-0xCF: А-П (U+0410-U+041F)
        0x0410,
        0x0411,
        0x0412,
        0x0413,
        0x0414,
        0x0415,
        0x0416,
        0x0417,
        0x0418,
        0x0419,
        0x041A,
        0x041B,
        0x041C,
        0x041D,
        0x041E,
        0x041F,
        // 0xD0-0xDF: Р-Я (U+0420-U+042F)
        0x0420,
        0x0421,
        0x0422,
        0x0423,
        0x0424,
        0x0425,
        0x0426,
        0x0427,
        0x0428,
        0x0429,
        0x042A,
        0x042B,
        0x042C,
        0x042D,
        0x042E,
        0x042F,
        // 0xE0-0xEF: а-п (U+0430-U+043F)
        0x0430,
        0x0431,
        0x0432,
        0x0433,
        0x0434,
        0x0435,
        0x0436,
        0x0437,
        0x0438,
        0x0439,
        0x043A,
        0x043B,
        0x043C,
        0x043D,
        0x043E,
        0x043F,
        // 0xF0-0xFF: р-я (U+0440-U+044F)
        0x0440,
        0x0441,
        0x0442,
        0x0443,
        0x0444,
        0x0445,
        0x0446,
        0x0447,
        0x0448,
        0x0449,
        0x044A,
        0x044B,
        0x044C,
        0x044D,
        0x044E,
        0x044F,
    };

    QString result;
    result.reserve(data.size());

    for (int i = 0; i < data.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(data[i]);
        if (ch < 0x80) {
            result.append(QChar(ch));
        } else {
            result.append(QChar(win1251table[ch - 0x80]));
        }
    }

    return result;
}

} // namespace rats::net
