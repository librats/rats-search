#ifndef RATS_SERVICE_FILTER_POLICY_H
#define RATS_SERVICE_FILTER_POLICY_H

#include "domain/torrent.h"

#include <QRegularExpression>
#include <QString>

namespace rats::service {

// Plain settings that drive the filter (sourced from ConfigStore). Keeping the
// policy dependent on this struct rather than on ConfigStore keeps it pure and
// trivially testable.
struct FilterSettings {
    int maxFiles = 0; // 0 = no limit
    qint64 sizeMin = 0;
    qint64 sizeMax = 0;
    bool adultFilter = false;
    QString namingRegExp;
    bool namingRegExpNegative = false;
    QString contentTypeFilter; // empty or "all" = accept all types; otherwise CSV
};

// Decides whether a torrent is allowed into the index — one method per rule.
class FilterPolicy {
public:
    FilterPolicy() = default;
    explicit FilterPolicy(FilterSettings settings);

    void setSettings(FilterSettings settings);
    const FilterSettings& settings() const { return settings_; }

    // Empty string = accepted; otherwise a human-readable rejection reason.
    QString rejectionReason(const domain::Torrent& torrent) const;
    bool accepts(const domain::Torrent& torrent) const { return rejectionReason(torrent).isEmpty(); }

private:
    QString checkFileCount(const domain::Torrent& t) const;
    QString checkSize(const domain::Torrent& t) const;
    QString checkAdult(const domain::Torrent& t) const;
    QString checkNamingRegExp(const domain::Torrent& t) const;
    QString checkContentType(const domain::Torrent& t) const;

    // Recompile the naming regex from settings_; called whenever settings change,
    // so the hot per-torrent path reuses one compiled QRegularExpression.
    void compileNamingRegex();

    FilterSettings settings_;
    QRegularExpression namingRegex_;
};

} // namespace rats::service

#endif // RATS_SERVICE_FILTER_POLICY_H
