#ifndef RATS_SERVICE_DATABASE_TRANSFER_RESUME_H
#define RATS_SERVICE_DATABASE_TRANSFER_RESUME_H

#include <QString>

namespace rats::service {

// The note kept beside a half-received database dump, saying what it is.
//
// A peer transfer that breaks half way now leaves its bytes on disk instead of
// reclaiming them (see P2PTransport::acceptFileResume), and those bytes are worth
// nothing without knowing which file they are a prefix of: a peer rebuilds its
// snapshot every few hours, and continuing into a *different* generation is the
// one mistake the end-to-end SHA-256 catches only after the whole download has
// been paid for. So a partial is continued only while this note still matches the
// offer on the table — same peer, same snapshot generation, same total size — and
// is thrown away otherwise.
//
// Plain struct + two file operations on purpose: this is the piece that decides
// whether gigabytes are re-downloaded, and it is worth being able to test it
// without a peer, a transport or an event loop.
class DatabaseTransferResume {
public:
    QString peerId; // who the dump came from
    QString snapshot; // the generation they served ("snapshot-7.ratsdb")
    qint64 size = 0; // total size of that generation, in bytes

    // A note missing any of the three tells us nothing and is not a resume point.
    bool isValid() const;

    // Missing, unreadable or malformed all read back as an invalid note.
    static DatabaseTransferResume load(const QString& path);
    bool save(const QString& path) const;
    static void clear(const QString& path);

    // Where the transfer may continue from, or 0 for "start over". `partialSize`
    // is what is on disk right now; the other three describe the offer.
    qint64 offsetFor(
        qint64 partialSize, const QString& fromPeer, const QString& offeredSnapshot, qint64 offeredSize) const;
};

} // namespace rats::service

#endif // RATS_SERVICE_DATABASE_TRANSFER_RESUME_H
