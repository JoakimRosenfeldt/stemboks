// Layout verified against Deep Symmetry's Database Exports analysis and
// crate-digger/src/main/kaitai/rekordbox_pdb.ksy, consulted 2026-10-14:
// https://djl-analysis.deepsymmetry.org/rekordbox-export-analysis/exports.html
// https://github.com/Deep-Symmetry/crate-digger/blob/main/src/main/kaitai/rekordbox_pdb.ksy
// Verified: header/page layouts, packed row counts, row groups, string encodings,
// track fields/string indices, lookup rows, and playlist rows. The track diagram
// and Kaitai use 0x5e for string offsets; the analysis prose conflicts with this.
// No offsets are guessed from memory. TODO: verify with a real USB export;
// local sample search found none. Beat grids require ANLZ files, not export.pdb.

#include "RekordboxUsb.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <algorithm>
#include <limits>
#include <utility>

namespace stemboks {

namespace RekordboxUsb {
namespace {

constexpr int PageHeaderSize = 0x28;

QString databasePath(const QString& root)
{
    return QDir(root).filePath(QStringLiteral("PIONEER/rekordbox/export.pdb"));
}

// A failed read poisons the view, so callers cannot accidentally accept truncation.
struct Bytes {
    const QByteArray& data;
    int end;
    bool ok = true;

    bool contains(int pos, int count) const
    {
        return pos >= 0 && count >= 0 && pos <= end && count <= end - pos;
    }

    quint32 number(int pos, int count)
    {
        if (!ok || !contains(pos, count)) {
            ok = false;
            return 0;
        }
        quint32 value = 0;
        for (int i = 0; i < count; ++i)
            value |= quint32(static_cast<unsigned char>(data.at(pos + i))) << (8 * i);
        return value;
    }

    quint32 u8(int pos) { return number(pos, 1); }
    quint32 u16(int pos) { return number(pos, 2); }
    quint32 u32(int pos) { return number(pos, 4); }

    QString string(int pos)
    {
        const quint32 kind = u8(pos);
        int header = 1;
        int length = int(kind >> 1);
        if (!(kind & 1)) {
            if (kind != 0x40 && kind != 0x90) {
                ok = false;
                return {};
            }
            header = 4;
            length = int(u16(pos + 1));
        }
        if (!ok || length < header || !contains(pos, length)) {
            ok = false;
            return {};
        }
        if (kind != 0x90)
            return QString::fromLatin1(data.constData() + pos + header, length - header);
        if ((length - header) % 2) {
            ok = false;
            return {};
        }
        QString result;
        result.reserve((length - header) / 2);
        for (int i = header; i < length; i += 2)
            result.append(QChar(ushort(u16(pos + i))));
        return result;
    }

    QString offsetString(int row, int offset, int minimum)
    {
        if (offset < minimum || row < PageHeaderSize || row > end || offset >= end - row) {
            ok = false;
            return {};
        }
        return string(row + offset);
    }
};

struct TrackRefs { quint32 artist, album, genre, key; };
struct PlaylistNode { quint32 id, order; RbPlaylist playlist; };
struct Entry { quint32 playlist, order; int track; };

class Parser {
public:
    Library library;
    QString error;

    bool read(const QString& path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return fail(QStringLiteral("Cannot open %1: %2").arg(path, file.errorString()));
        const QByteArray header = file.read(28);
        Bytes h{header, int(header.size())};
        const quint32 zero = h.u32(0);
        const quint32 pageSize = h.u32(4);
        const quint32 numTables = h.u32(8);
        if (!h.ok || zero != 0 || pageSize < 64 || pageSize > 1024 * 1024
            || file.size() < pageSize || file.size() % pageSize != 0
            || numTables == 0 || numTables > (pageSize - 28) / 16)
            return fail(QStringLiteral("Invalid or truncated DeviceSQL header"));

        const QByteArray tables = file.read(qint64(numTables) * 16);
        Bytes t{tables, int(tables.size())};
        if (tables.size() != qint64(numTables) * 16)
            return fail(QStringLiteral("Truncated table directory"));
        QSet<quint32> types;
        for (quint32 i = 0; i < numTables; ++i) {
            const int base = int(i * 16);
            const quint32 type = t.u32(base);
            const quint32 first = t.u32(base + 8);
            const quint32 last = t.u32(base + 12);
            if (!t.ok || types.contains(type))
                return fail(QStringLiteral("Invalid or duplicate table descriptor"));
            types.insert(type);
            if (type <= 3 || type == 5 || type == 7 || type == 8) {
                if (!readTable(file, pageSize, type, first, last))
                    return false;
            }
        }
        if (!types.contains(0))
            return fail(QStringLiteral("DeviceSQL database has no tracks table"));

        for (int i = 0; i < library.tracks.size(); ++i) {
            auto& track = library.tracks[i];
            const auto& refs = trackRefs[i];
            track.artist = artists.value(refs.artist);
            track.album = albums.value(refs.album);
            track.genre = genres.value(refs.genre);
            track.keyText = keys.value(refs.key);
        }
        std::stable_sort(nodes.begin(), nodes.end(), [](const PlaylistNode& a, const PlaylistNode& b) {
            return a.order < b.order;
        });
        QHash<quint32, int> playlistIndex;
        for (const auto& node : nodes) {
            playlistIndex.insert(node.id, int(library.playlists.size()));
            library.playlists.append(node.playlist);
        }
        std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
            return a.order < b.order;
        });
        for (const auto& entry : entries) {
            const auto it = playlistIndex.constFind(entry.playlist);
            if (it != playlistIndex.cend())
                library.playlists[it.value()].trackRowIds.append(entry.track);
        }
        return true;
    }

private:
    QVector<TrackRefs> trackRefs;
    QVector<PlaylistNode> nodes;
    QVector<Entry> entries;
    QHash<quint32, QString> artists, albums, genres, keys;
    QSet<quint32> playlistIds;

    bool fail(const QString& message)
    {
        error = message;
        return false;
    }

    bool readTable(QFile& file, quint32 pageSize, quint32 type, quint32 first, quint32 last)
    {
        if (first == 0 && last == 0)
            return true;
        const quint64 pageCount = quint64(file.size()) / pageSize;
        if (first == 0 || last == 0 || first >= pageCount || last >= pageCount)
            return fail(QStringLiteral("Table %1 has an invalid page range").arg(type));
        QSet<quint32> visited;
        quint32 index = first;
        while (true) {
            if (index == 0 || index >= pageCount || visited.contains(index))
                return fail(QStringLiteral("Broken or cyclic page chain in table %1").arg(type));
            visited.insert(index);
            if (!file.seek(qint64(index) * pageSize))
                return fail(QStringLiteral("Cannot seek to page %1").arg(index));
            const QByteArray page = file.read(pageSize);
            Bytes p{page, int(page.size())};
            const quint32 zero = p.u32(0);
            const quint32 storedIndex = p.u32(4);
            const quint32 storedType = p.u32(8);
            const quint32 next = p.u32(12);
            const quint32 flags = p.u8(0x1b);
            if (!p.ok || page.size() != pageSize || zero != 0 || storedIndex != index || storedType != type)
                return fail(QStringLiteral("Invalid header on page %1").arg(index));
            if (!(flags & 0x40) && !readRows(page, type))
                return fail(QStringLiteral("Malformed row or row index on page %1, table %2").arg(index).arg(type));
            if (index == last)
                break;
            index = next;
        }
        return true;
    }

    bool readRows(const QByteArray& page, quint32 type)
    {
        Bytes p{page, int(page.size())};
        const quint32 counts = p.number(0x18, 3);
        const int count = int(counts & 0x1fff);
        const int groups = (count + 15) / 16;
        // The final group allocates only the offsets it needs, plus two masks.
        const int heapEnd = int(page.size()) - count * 2 - groups * 4;
        if (!p.ok || heapEnd < PageHeaderSize)
            return false;
        for (int i = 0; i < count; ++i) {
            const int base = int(page.size()) - (i / 16) * 36;
            const quint32 present = p.u16(base - 4);
            if (!(present & (quint32(1) << (i % 16))))
                continue;
            const int row = PageHeaderSize + int(p.u16(base - 6 - (i % 16) * 2));
            if (!p.ok || row >= heapEnd)
                return false;
            Bytes r{page, heapEnd};
            if (!readRow(r, row, type) || !r.ok)
                return false;
        }
        return p.ok;
    }

    bool readRow(Bytes& r, int row, quint32 type)
    {
        if (type == 0) {
            constexpr int Strings = 0x5e;
            constexpr int FixedSize = Strings + 21 * 2;
            if (!r.contains(row, FixedSize) || r.u16(row) != 0x24)
                return false;
            const quint32 id = r.u32(row + 0x48);
            if (id > quint32(std::numeric_limits<int>::max()) || library.rowIdToTrack.contains(int(id)))
                return false;
            RbTrack track;
            track.bpm = r.u32(row + 0x38) / 100.0;
            track.durationSec = int(r.u16(row + 0x54));
            track.title = r.offsetString(row, int(r.u16(row + Strings + 17 * 2)), FixedSize);
            track.filename = r.offsetString(row, int(r.u16(row + Strings + 19 * 2)), FixedSize);
            QString path = r.offsetString(row, int(r.u16(row + Strings + 20 * 2)), FixedSize);
            path.replace(QLatin1Char('\\'), QLatin1Char('/'));
            const int slash = int(path.lastIndexOf(QLatin1Char('/')));
            track.folderPath = slash >= 0 ? path.left(slash) : QString();
            if (track.filename.isEmpty())
                track.filename = path.mid(slash + 1);
            const TrackRefs refs{r.u32(row + 0x44), r.u32(row + 0x40),
                                 r.u32(row + 0x3c), r.u32(row + 0x20)};
            if (!r.ok)
                return false;
            library.rowIdToTrack.insert(int(id), int(library.tracks.size()));
            library.tracks.append(std::move(track));
            trackRefs.append(refs);
        } else if (type == 1 || type == 5) {
            const quint32 id = r.u32(row);
            const QString name = r.string(row + (type == 1 ? 4 : 8));
            auto& names = type == 1 ? genres : keys;
            if (!r.ok || names.contains(id))
                return false;
            names.insert(id, name);
        } else if (type == 2 || type == 3) {
            const bool album = type == 3;
            const quint32 subtype = r.u16(row);
            const quint32 nearType = album ? 0x80 : 0x60;
            if (subtype != nearType && subtype != (nearType | 4))
                return false;
            const bool far = subtype & 4;
            const quint32 id = r.u32(row + (album ? 12 : 4));
            const int offsetAt = row + (album ? 20 : 8);
            const int offset = int(far ? r.u16(offsetAt + 2) : r.u8(offsetAt + 1));
            const QString name = r.offsetString(row, offset, (album ? 20 : 8) + (far ? 4 : 2));
            auto& names = album ? albums : artists;
            if (!r.ok || names.contains(id))
                return false;
            names.insert(id, name);
        } else if (type == 7) {
            const quint32 order = r.u32(row + 8);
            const quint32 id = r.u32(row + 12);
            const quint32 folder = r.u32(row + 16);
            const QString name = r.string(row + 20);
            if (!r.ok || playlistIds.contains(id))
                return false;
            playlistIds.insert(id);
            if (!folder)
                nodes.append({id, order, {name, {}}});
        } else if (type == 8) {
            const quint32 order = r.u32(row);
            const quint32 track = r.u32(row + 4);
            const quint32 playlist = r.u32(row + 8);
            if (!r.ok || track > quint32(std::numeric_limits<int>::max()))
                return false;
            entries.append({playlist, order, int(track)});
        }
        return r.ok;
    }
};

}

bool isRekordboxUsb(const QString& volumeRoot)
{
    return QFileInfo(databasePath(volumeRoot)).isFile();
}

bool parse(const QString& volumeRoot, Library* out, QString* error)
{
    if (error)
        error->clear();
    if (!out) {
        if (error)
            *error = QStringLiteral("Output library is null");
        return false;
    }
    Parser parser;
    if (!parser.read(databasePath(volumeRoot))) {
        if (error)
            *error = parser.error;
        return false;
    }
    *out = std::move(parser.library);
    return true;
}

QString trackAbsolutePath(const QString& volumeRoot, const RbTrack& t)
{
    if (t.filename.isEmpty())
        return {};
    QString relative = t.folderPath;
    relative.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (relative.startsWith(QLatin1Char('/')))
        relative.remove(0, 1);
    QString filename = t.filename;
    filename.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (filename.contains(QLatin1Char('/')) || filename == QLatin1String(".") || filename == QLatin1String(".."))
        return {};
    const QString root = QDir(volumeRoot).absolutePath();
    const QString path = QDir::cleanPath(root + QLatin1Char('/') + relative + QLatin1Char('/') + filename);
    const QString prefix = root.endsWith(QLatin1Char('/')) ? root : root + QLatin1Char('/');
    return path.startsWith(prefix) ? path : QString();
}

}

} // namespace stemboks
