#include "services/export_sink.h"

#include "services/database_dump.h"
#include "services/database_export.h"

#include <QFileInfo>

namespace rats::service {

ExportFormat exportFormatForPath(const QString& path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QLatin1String("csv"))
        return ExportFormat::Csv;
    // ".json" is not strictly right — the file is one object per line, not one
    // document — but somebody who typed it wants text, and JSONL is the text.
    if (suffix == QLatin1String("jsonl") || suffix == QLatin1String("ndjson") || suffix == QLatin1String("json"))
        return ExportFormat::JsonLines;
    return ExportFormat::Ratsdb;
}

QString exportFormatName(ExportFormat format)
{
    switch (format) {
    case ExportFormat::Csv:
        return QStringLiteral("csv");
    case ExportFormat::JsonLines:
        return QStringLiteral("jsonl");
    case ExportFormat::Ratsdb:
        break;
    }
    return QStringLiteral("ratsdb");
}

std::optional<ExportFormat> exportFormatFromName(const QString& name)
{
    const QString key = name.trimmed().toLower();
    if (key == QLatin1String("ratsdb"))
        return ExportFormat::Ratsdb;
    if (key == QLatin1String("csv"))
        return ExportFormat::Csv;
    if (key == QLatin1String("jsonl") || key == QLatin1String("ndjson") || key == QLatin1String("json"))
        return ExportFormat::JsonLines;
    return std::nullopt;
}

std::unique_ptr<TorrentSink> makeExportSink(ExportFormat format, const ExportOptions& options)
{
    switch (format) {
    case ExportFormat::Csv: {
        CsvWriter::Options csv;
        csv.includeFiles = options.includeFiles;
        return std::make_unique<CsvWriter>(csv);
    }
    case ExportFormat::JsonLines:
        return std::make_unique<JsonLinesWriter>();
    case ExportFormat::Ratsdb:
        break;
    }
    return std::make_unique<DumpWriter>();
}

} // namespace rats::service
