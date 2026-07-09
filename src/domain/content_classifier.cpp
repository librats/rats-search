#include "domain/content_classifier.h"

#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QRegularExpression>
#include <QSet>

// Data notes
// ----------
// The extension→type table and bad-word lists live in :/content/*.json.
//
// Ten extensions are claimed by more than one content type; extensions.json
// keeps unique keys, so the winner is baked in: mod→audio, mng→pictures,
// pkg→games, md→games, iso→archive, cue→archive, img→archive, cab→archive,
// bin→archive, ccd→archive. Bad words are deduped (block: 1710, veryBad: 144).

namespace rats::domain {

namespace {

struct ClassifierData {
    QHash<QString, QString> extensionToType;
    QSet<QString> blockWords;
    QSet<QString> veryBadWords;
};

QByteArray readResource(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll();
}

const ClassifierData& data()
{
    static const ClassifierData d = [] {
        ClassifierData out;

        const QJsonObject exts
            = QJsonDocument::fromJson(readResource(QStringLiteral(":/content/extensions.json"))).object();
        out.extensionToType.reserve(exts.size());
        for (auto it = exts.begin(); it != exts.end(); ++it)
            out.extensionToType.insert(it.key(), it.value().toString());

        const QJsonObject words
            = QJsonDocument::fromJson(readResource(QStringLiteral(":/content/badwords.json"))).object();
        for (const QJsonValue& v : words["blockWords"].toArray())
            out.blockWords.insert(v.toString());
        for (const QJsonValue& v : words["veryBadWords"].toArray())
            out.veryBadWords.insert(v.toString());

        return out;
    }();
    return d;
}

// Delimiter set matching the legacy
// name.split(/[`~!@#$%^&*()\]\[{}.,+?/\\;:\-_' "|]/)
const QRegularExpression& nameDelimiterRegex()
{
    static const QRegularExpression re(QStringLiteral(R"([`~!@#$%^&*()\]\[{}.,+?/\\;:\-_' "|]+)"));
    return re;
}

// Map a file path to a content-type string ("video"/"audio"/…) or empty.
QString detectFileType(const QString& filePath)
{
    QString name = filePath.section('/', -1);
    if (name.isEmpty())
        name = filePath.section('\\', -1);
    if (name.isEmpty())
        return {};

    const int dotPos = name.lastIndexOf('.');
    if (dotPos < 0 || dotPos == name.length() - 1)
        return {};

    return data().extensionToType.value(name.mid(dotPos + 1).toLower());
}

// Apply the adult-content word rules to a single name; may raise `type` to Bad
// (terminal) or `category` to XXX (non-terminal). Returns true once Bad is set.
bool applyBadWords(const QString& name, ContentType& type, ContentCategory& category)
{
    const ClassifierData& d = data();
    const QStringList wordsInName = name.toLower().split(nameDelimiterRegex(), Qt::SkipEmptyParts);
    for (const QString& word : wordsInName) {
        if (d.veryBadWords.contains(word)) {
            type = ContentType::Bad;
            return true;
        }
        if (d.blockWords.contains(word))
            category = ContentCategory::XXX;
    }
    return false;
}

} // namespace

ContentType ContentClassifier::detectTypeFromFiles(const QVector<File>& files)
{
    if (files.isEmpty())
        return ContentType::Unknown;

    qint64 totalSize = 0;
    for (const File& f : files)
        totalSize += f.size;

    // Weighted vote: each file adds (file.size / totalSize) to its type. When all
    // sizes are zero, fall back to equal weight-per-file (1 / fileCount).
    QMap<QString, double> priority;
    for (const File& f : files) {
        const QString type = detectFileType(f.path);
        if (type.isEmpty())
            continue;
        const double weight
            = totalSize > 0 ? static_cast<double>(f.size) / static_cast<double>(totalSize) : 1.0 / files.size();
        priority[type] += weight;
    }

    QString bestType;
    double bestWeight = 0.0;
    for (auto it = priority.begin(); it != priority.end(); ++it) {
        if (it.value() > bestWeight) {
            bestWeight = it.value();
            bestType = it.key();
        }
    }

    return contentTypeFromString(bestType);
}

Classification ContentClassifier::classify(const QString& name, const QVector<File>& files)
{
    Classification result;
    result.type = detectTypeFromFiles(files);

    // Adult-content sub-categorisation only applies to media-ish types.
    if (result.type == ContentType::Video || result.type == ContentType::Pictures
        || result.type == ContentType::Archive) {
        if (applyBadWords(name, result.type, result.category))
            return result; // marked Bad — stop

        for (const File& f : files) {
            QString path = f.path;
            const int dotPos = path.lastIndexOf('.'); // strip extension, like legacy fileCheck.pop()
            if (dotPos > 0)
                path = path.left(dotPos);
            if (applyBadWords(path, result.type, result.category))
                break; // Bad — stop scanning files
        }
    }

    return result;
}

void ContentClassifier::classify(Torrent& torrent)
{
    const Classification c = classify(torrent.name, torrent.fileList);
    torrent.contentType = c.type;
    torrent.contentCategory = c.category;
}

} // namespace rats::domain
