#pragma once
#include "../engine/Types.h"
#include <QObject>
#include <QSqlDatabase>

struct LibraryTrack {
    qint64 id = -1;
    QString path, title, artist, album, genre;
    double durationSec = 0, bpm = 0;
    QString keyText;
    int playCount = 0;
    bool stemsReady = false;
};

struct Playlist { qint64 id = -1; QString name; int trackCount = 0; };

// SQLite library (library.db in data dir), folder/USB scanning, playlists,
// history sessions, tag list, per-track hotcues/grids (also mirrored to TSV
// sidecars grilles-locales.tsv / hotcues-locaux.tsv like the original).
class Library : public QObject {
    Q_OBJECT
public:
    static Library& instance();
    bool open();

    QVector<LibraryTrack> allTracks(const QString& search = {}) const;
    QVector<LibraryTrack> tracksByArtist(const QString& artist) const;
    QVector<LibraryTrack> tracksByAlbum(const QString& album) const;
    QVector<LibraryTrack> tracksByKey(const QString& key) const;
    QStringList artists() const;
    QStringList albums() const;
    QStringList keys() const;

    QVector<Playlist> playlists() const;
    qint64 createPlaylist(const QString& name);
    void deletePlaylist(qint64 id);
    QVector<LibraryTrack> playlistTracks(qint64 playlistId) const;
    void addToPlaylist(qint64 playlistId, qint64 trackId);
    void removeFromPlaylist(qint64 playlistId, qint64 trackId);

    // Tag list is a reserved playlist named "__taglist__".
    QVector<LibraryTrack> tagList() const;
    void toggleTag(qint64 trackId);

    qint64 trackIdForPath(const QString& path) const;
    qint64 addOrUpdateTrack(const TrackInfo& info);
    void setAnalysis(qint64 trackId, double bpm, double firstBeat, const QString& keyText);
    void setStemsReady(qint64 trackId, bool ready);
    void recordPlay(qint64 trackId);
    QVector<LibraryTrack> history(int limit = 200) const;

    // hotcues / grids persistence
    void saveHotcues(const QString& path, const std::array<Hotcue, kNumHotcues>& cues, double cueSec);
    void loadHotcues(const QString& path, std::array<Hotcue, kNumHotcues>* cues, double* cueSec);
    void saveGrid(const QString& path, double bpm, double firstBeatSec);

    int scanDirectory(const QString& dir);    // returns new tracks found (tags only, fast)
    static QStringList removableVolumes();    // USB sticks for SOURCE screen

signals:
    void changed();
    void scanProgress(int done, int total);

private:
    Library() = default;
    QSqlDatabase m_db;
    void migrate();
    QVector<LibraryTrack> queryTracks(const QString& where, const QVariantList& binds = {}) const;
};
