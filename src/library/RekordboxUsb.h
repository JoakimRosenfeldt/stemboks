#pragma once

#include <QHash>
#include <QString>
#include <QVector>

namespace stemboks {

namespace RekordboxUsb {

bool isRekordboxUsb(const QString& volumeRoot);

struct RbTrack {
    QString title, artist, album, genre, filename, folderPath;
    double bpm = 0;
    int durationSec = 0;
    QString keyText;
    double firstBeatSec = -1;
};

struct RbPlaylist {
    QString name;
    QVector<int> trackRowIds;
};

struct Library {
    QVector<RbTrack> tracks;
    QVector<RbPlaylist> playlists;
    QHash<int, int> rowIdToTrack;
};

// Replaces out only on success. Playlist folders are omitted; track IDs are PDB IDs.
bool parse(const QString& volumeRoot, Library* out, QString* error = nullptr);

// folderPath is the volume-relative parent directory of the audio file.
// Returns an empty string for an empty filename or a path escaping volumeRoot.
QString trackAbsolutePath(const QString& volumeRoot, const RbTrack& t);

}

} // namespace stemboks
