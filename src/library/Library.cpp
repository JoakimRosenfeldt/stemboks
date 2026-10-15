#include "Library.h"
#include "RekordboxUsb.h"
#include "../control/AppSettings.h"
#include "../engine/Decoder.h"
#include "../stems/StemManager.h"
#include <QDateTime>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStorageInfo>
#include <QTextStream>
#include <QDebug>
#include <algorithm>

namespace stemboks {

namespace {
bool execute(QSqlDatabase db, const QString& sql, const QVariantList& binds = {}) {
    QSqlQuery q(db);
    if (!q.prepare(sql)) { qWarning() << q.lastError(); return false; }
    for (const auto& v : binds) q.addBindValue(v);
    if (!q.exec()) { qWarning() << q.lastError(); return false; }
    return true;
}
QString escaped(QString s) {
    return s.replace("\\", "\\\\").replace("\t", "\\t").replace("\n", "\\n").replace("\r", "\\r");
}
QString unescaped(const QString& s) {
    QString out;
    for (int i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            const QChar c = s[++i];
            out += c == 't' ? QChar('\t') : c == 'n' ? QChar('\n') : c == 'r' ? QChar('\r') : c;
        } else out += s[i];
    }
    return out;
}
QVector<QStringList> readTsv(const QString& name) {
    QFile f(AppSettings::dataDir() + '/' + name);
    QVector<QStringList> rows;
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return rows;
    QTextStream stream(&f);
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        if (line.isEmpty() || line.startsWith('#')) continue;
        QStringList row = line.split('\t');
        for (auto& value : row) value = unescaped(value);
        rows.append(row);
    }
    return rows;
}
void mirror(QSqlDatabase db, const QString& name, const QString& sql) {
    QSqlQuery q(db);
    if (!q.exec(sql)) { qWarning() << q.lastError(); return; }
    QSaveFile f(AppSettings::dataDir() + '/' + name);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) { qWarning() << f.errorString(); return; }
    QTextStream stream(&f);
    while (q.next()) {
        QStringList row;
        for (int i = 0; i < q.record().count(); ++i) row.append(escaped(q.value(i).toString()));
        stream << row.join('\t') << '\n';
    }
    stream.flush();
    if (!f.commit()) qWarning() << f.errorString();
}
QStringList distinct(QSqlDatabase db, const QString& column) {
    QStringList values;
    QSqlQuery q(db);
    if (q.exec("SELECT DISTINCT " + column + " FROM tracks WHERE " + column + " <> '' ORDER BY " + column + " COLLATE NOCASE"))
        while (q.next()) values.append(q.value(0).toString());
    return values;
}
}

Library& Library::instance() { static Library lib; return lib; }

bool Library::open() {
    if (m_db.isOpen()) return true;
    m_db = QSqlDatabase::contains("stemboks-library") ? QSqlDatabase::database("stemboks-library")
                                                     : QSqlDatabase::addDatabase("QSQLITE", "stemboks-library");
    m_db.setDatabaseName(AppSettings::dataDir() + "/library.db");
    if (!m_db.open()) { qWarning() << m_db.lastError(); return false; }
    if (!execute(m_db, "PRAGMA foreign_keys=ON") || !execute(m_db, "PRAGMA busy_timeout=5000")) return false;
    migrate();
    const QStringList required = {"tracks", "playlists", "playlist_tracks", "history", "hotcues", "cues", "grids"};
    for (const auto& table : required) if (!m_db.tables().contains(table)) { m_db.close(); return false; }
    // TSV rows fill missing records only. SQLite remains authoritative.
    if (!m_db.transaction()) return false;
    bool ok = true;
    for (const auto& row : readTsv("hotcues-locaux.tsv")) {
        if (row.size() != 5) continue;
        bool indexOk, positionOk, cueOk;
        const int idx = row[1].toInt(&indexOk);
        const double position = row[2].toDouble(&positionOk), cue = row[4].toDouble(&cueOk);
        if (!indexOk || idx < 0 || idx >= kNumHotcues || !positionOk || !cueOk) continue;
        ok &= execute(m_db, "INSERT OR IGNORE INTO hotcues(path,idx,position,label) VALUES(?,?,?,?)", {row[0], idx, position, row[3]});
        ok &= execute(m_db, "INSERT OR IGNORE INTO cues(path,cue) VALUES(?,?)", {row[0], cue});
    }
    for (const auto& row : readTsv("grilles-locales.tsv")) {
        if (row.size() != 3) continue;
        bool bpmOk, beatOk;
        const double bpm = row[1].toDouble(&bpmOk), beat = row[2].toDouble(&beatOk);
        if (bpmOk && beatOk) ok &= execute(m_db, "INSERT OR IGNORE INTO grids(path,bpm,first_beat) VALUES(?,?,?)", {row[0], bpm, beat});
    }
    if (ok) ok = execute(m_db, "UPDATE tracks SET bpm=(SELECT bpm FROM grids WHERE path=tracks.path),first_beat=(SELECT first_beat FROM grids WHERE path=tracks.path) WHERE EXISTS(SELECT 1 FROM grids WHERE path=tracks.path)");
    if (ok) ok = m_db.commit(); else m_db.rollback();
    if (!ok) m_db.close();
    return ok;
}

void Library::migrate() {
    const QStringList schema = {
        "CREATE TABLE IF NOT EXISTS tracks(id INTEGER PRIMARY KEY,path TEXT NOT NULL UNIQUE,title TEXT,artist TEXT,album TEXT,genre TEXT,duration REAL DEFAULT 0,bpm REAL DEFAULT 0,first_beat REAL DEFAULT 0,key TEXT,play_count INTEGER DEFAULT 0,stems_ready INTEGER DEFAULT 0,added_at INTEGER)",
        "CREATE TABLE IF NOT EXISTS playlists(id INTEGER PRIMARY KEY,name TEXT NOT NULL)",
        "CREATE TABLE IF NOT EXISTS playlist_tracks(pid INTEGER REFERENCES playlists(id) ON DELETE CASCADE,tid INTEGER REFERENCES tracks(id) ON DELETE CASCADE,position INTEGER,PRIMARY KEY(pid,tid))",
        "CREATE TABLE IF NOT EXISTS history(id INTEGER PRIMARY KEY,tid INTEGER REFERENCES tracks(id) ON DELETE CASCADE,played_at INTEGER)",
        "CREATE TABLE IF NOT EXISTS hotcues(path TEXT,idx INTEGER,position REAL,label TEXT,PRIMARY KEY(path,idx))",
        "CREATE TABLE IF NOT EXISTS cues(path TEXT PRIMARY KEY,cue REAL)",
        "CREATE TABLE IF NOT EXISTS grids(path TEXT PRIMARY KEY,bpm REAL,first_beat REAL)",
        "CREATE INDEX IF NOT EXISTS history_time ON history(played_at)",
        "CREATE INDEX IF NOT EXISTS playlist_order ON playlist_tracks(pid,position)"
    };
    if (!m_db.transaction()) return;
    for (const auto& sql : schema) if (!execute(m_db, sql)) { m_db.rollback(); return; }
    m_db.commit();
}

QVector<LibraryTrack> Library::queryTracks(const QString& where, const QVariantList& binds) const {
    QVector<LibraryTrack> tracks;
    if (!m_db.isOpen()) return tracks;
    QSqlQuery q(m_db);
    q.prepare("SELECT t.id,t.path,t.title,t.artist,t.album,t.genre,t.duration,t.bpm,t.key,t.play_count,t.stems_ready FROM tracks t " + where);
    for (const auto& v : binds) q.addBindValue(v);
    if (!q.exec()) { qWarning() << q.lastError(); return tracks; }
    while (q.next()) {
        LibraryTrack t;
        t.id = q.value(0).toLongLong(); t.path = q.value(1).toString(); t.title = q.value(2).toString();
        t.artist = q.value(3).toString(); t.album = q.value(4).toString(); t.genre = q.value(5).toString();
        t.durationSec = q.value(6).toDouble(); t.bpm = q.value(7).toDouble(); t.keyText = q.value(8).toString();
        t.playCount = q.value(9).toInt(); t.stemsReady = q.value(10).toBool();
        tracks.append(t);
    }
    return tracks;
}
QVector<LibraryTrack> Library::allTracks(const QString& search) const {
    if (search.isEmpty()) return queryTracks("ORDER BY t.artist COLLATE NOCASE,t.title COLLATE NOCASE,t.id");
    QString literal = search;
    literal.replace("\\", "\\\\").replace("%", "\\%").replace("_", "\\_");
    const QString pattern = '%' + literal + '%';
    return queryTracks("WHERE t.title LIKE ? ESCAPE '\\' OR t.artist LIKE ? ESCAPE '\\' OR t.album LIKE ? ESCAPE '\\' OR t.genre LIKE ? ESCAPE '\\' OR t.path LIKE ? ESCAPE '\\' ORDER BY t.artist COLLATE NOCASE,t.title COLLATE NOCASE,t.id", {pattern, pattern, pattern, pattern, pattern});
}
QVector<LibraryTrack> Library::tracksByArtist(const QString& artist) const { return queryTracks("WHERE t.artist=? ORDER BY t.album,t.title,t.id", {artist}); }
QVector<LibraryTrack> Library::tracksByAlbum(const QString& album) const { return queryTracks("WHERE t.album=? ORDER BY t.title,t.id", {album}); }
QVector<LibraryTrack> Library::tracksByKey(const QString& key) const { return queryTracks("WHERE t.key=? ORDER BY t.artist,t.title,t.id", {key}); }
QStringList Library::artists() const { return distinct(m_db, "artist"); }
QStringList Library::albums() const { return distinct(m_db, "album"); }
QStringList Library::keys() const { return distinct(m_db, "key"); }

QVector<Playlist> Library::playlists() const {
    QVector<Playlist> result;
    QSqlQuery q(m_db);
    if (q.exec("SELECT p.id,p.name,COUNT(pt.tid) FROM playlists p LEFT JOIN playlist_tracks pt ON pt.pid=p.id WHERE p.name<>'__taglist__' GROUP BY p.id ORDER BY p.name COLLATE NOCASE,p.id"))
        while (q.next()) result.append({q.value(0).toLongLong(), q.value(1).toString(), q.value(2).toInt()});
    return result;
}
qint64 Library::createPlaylist(const QString& name) {
    if (!m_db.isOpen()) return -1;
    QSqlQuery q(m_db); q.prepare("INSERT INTO playlists(name) VALUES(?)"); q.addBindValue(name);
    if (!q.exec()) { qWarning() << q.lastError(); return -1; }
    const qint64 id = q.lastInsertId().toLongLong(); emit changed(); return id;
}
void Library::deletePlaylist(qint64 id) { if (execute(m_db, "DELETE FROM playlists WHERE id=?", {id})) emit changed(); }
QVector<LibraryTrack> Library::playlistTracks(qint64 id) const { return queryTracks("JOIN playlist_tracks pt ON pt.tid=t.id WHERE pt.pid=? ORDER BY pt.position,t.id", {id}); }
void Library::addToPlaylist(qint64 pid, qint64 tid) {
    if (execute(m_db, "INSERT OR IGNORE INTO playlist_tracks(pid,tid,position) SELECT ?,?,COALESCE(MAX(position),-1)+1 FROM playlist_tracks WHERE pid=?", {pid, tid, pid})) emit changed();
}
void Library::removeFromPlaylist(qint64 pid, qint64 tid) { if (execute(m_db, "DELETE FROM playlist_tracks WHERE pid=? AND tid=?", {pid, tid})) emit changed(); }
QVector<LibraryTrack> Library::tagList() const { return queryTracks("JOIN playlist_tracks pt ON pt.tid=t.id JOIN playlists p ON p.id=pt.pid WHERE p.name='__taglist__' ORDER BY pt.position,t.id"); }
void Library::toggleTag(qint64 tid) {
    QSqlQuery q(m_db); q.exec("SELECT id FROM playlists WHERE name='__taglist__' ORDER BY id LIMIT 1");
    const qint64 pid = q.next() ? q.value(0).toLongLong() : createPlaylist("__taglist__");
    if (pid < 0) return;
    q.prepare("SELECT 1 FROM playlist_tracks WHERE pid=? AND tid=?"); q.addBindValue(pid); q.addBindValue(tid);
    if (!q.exec()) return;
    if (q.next()) removeFromPlaylist(pid, tid); else addToPlaylist(pid, tid);
}
qint64 Library::trackIdForPath(const QString& path) const {
    if (!m_db.isOpen()) return -1;
    QSqlQuery q(m_db); q.prepare("SELECT id FROM tracks WHERE path=?"); q.addBindValue(path);
    return q.exec() && q.next() ? q.value(0).toLongLong() : -1;
}
namespace {
qint64 upsertTrack(QSqlDatabase db, const TrackInfo& info, bool stemsReady) {
    if (!db.isOpen() || info.path.isEmpty()) return -1;
    const QString title = info.title.isEmpty() ? QFileInfo(info.path).completeBaseName() : info.title;
    if (!execute(db, "INSERT INTO tracks(path,title,artist,album,genre,duration,bpm,key,stems_ready,added_at) VALUES(?,?,?,?,?,?,?,?,?,?) ON CONFLICT(path) DO UPDATE SET title=excluded.title,artist=excluded.artist,album=excluded.album,genre=excluded.genre,duration=excluded.duration,bpm=CASE WHEN tracks.bpm=0 THEN excluded.bpm ELSE tracks.bpm END,key=CASE WHEN COALESCE(tracks.key,'')='' THEN excluded.key ELSE tracks.key END,stems_ready=excluded.stems_ready",
                 {info.path, title, info.artist, info.album, info.genre, info.durationSec, info.bpm, info.keyText, stemsReady, QDateTime::currentMSecsSinceEpoch()})) return -1;
    if (!execute(db, "UPDATE tracks SET bpm=(SELECT bpm FROM grids WHERE path=tracks.path),first_beat=(SELECT first_beat FROM grids WHERE path=tracks.path) WHERE path=? AND EXISTS(SELECT 1 FROM grids WHERE path=tracks.path)", {info.path})) return -1;
    QSqlQuery query(db); query.prepare("SELECT id FROM tracks WHERE path=?"); query.addBindValue(info.path);
    return query.exec() && query.next() ? query.value(0).toLongLong() : -1;
}
}
qint64 Library::addOrUpdateTrack(const TrackInfo& info) {
    if (!m_db.isOpen() || info.path.isEmpty()) return -1;
    const qint64 id = upsertTrack(m_db, info, StemManager::stemsExistFor(info.path));
    if (id >= 0) emit changed();
    return id;
}
bool Library::importRekordbox(const QString& root, const RekordboxUsb::Library& usb, const QVector<bool>& stemsReady) {
    if (stemsReady.size() != usb.tracks.size() || !m_db.transaction()) return false;
    QVector<qint64> ids(usb.tracks.size(), -1);
    for (qsizetype i = 0; i < usb.tracks.size(); ++i) {
        const auto& track = usb.tracks[i];
        TrackInfo info;
        info.path = RekordboxUsb::trackAbsolutePath(root, track);
        if (info.path.isEmpty()) continue;
        info.title = track.title; info.artist = track.artist; info.album = track.album; info.genre = track.genre;
        info.durationSec = track.durationSec; info.bpm = track.bpm; info.keyText = track.keyText;
        ids[i] = upsertTrack(m_db, info, stemsReady[i]);
        if (ids[i] < 0) { m_db.rollback(); return false; }
    }
    for (const auto& playlist : usb.playlists) {
        QSqlQuery query(m_db);
        const QString name = "RB: " + playlist.name;
        query.prepare("INSERT INTO playlists(name) VALUES(?)"); query.addBindValue(name);
        if (!query.exec()) { m_db.rollback(); return false; }
        const qint64 pid = query.lastInsertId().toLongLong();
        int position = 0;
        for (int rowId : playlist.trackRowIds) {
            const int index = usb.rowIdToTrack.value(rowId, -1);
            if (index < 0 || index >= ids.size() || ids[index] < 0) continue;
            if (!execute(m_db, "INSERT OR IGNORE INTO playlist_tracks(pid,tid,position) VALUES(?,?,?)", {pid, ids[index], position++})) {
                m_db.rollback(); return false;
            }
        }
    }
    if (!m_db.commit()) { m_db.rollback(); return false; }
    emit changed();
    return true;
}
// The scan worker supplies stem availability so importing never opens containers.
bool importScannedLibraryTracks(const QVector<TrackInfo>& tracks, const QVector<bool>& stemsReady) {
    if (tracks.size() != stemsReady.size()) return false;
    auto db = QSqlDatabase::database("stemboks-library");
    if (!db.isOpen() || !db.transaction()) return false;
    for (qsizetype i = 0; i < tracks.size(); ++i) if (upsertTrack(db, tracks[i], stemsReady[i]) < 0) {
        db.rollback(); return false;
    }
    if (!db.commit()) { qWarning() << db.lastError(); db.rollback(); return false; }
    emit Library::instance().changed();
    return true;
}
void Library::setAnalysis(qint64 id, double bpm, double firstBeat, const QString& key) {
    if (execute(m_db, "UPDATE tracks SET bpm=?,first_beat=?,key=? WHERE id=?", {bpm, firstBeat, key, id})) {
        QSqlQuery q(m_db); q.prepare("SELECT path FROM tracks WHERE id=?"); q.addBindValue(id);
        if (q.exec() && q.next()) saveGrid(q.value(0).toString(), bpm, firstBeat);
    }
}
void Library::setStemsReady(qint64 id, bool ready) { if (execute(m_db, "UPDATE tracks SET stems_ready=? WHERE id=?", {ready, id})) emit changed(); }
void Library::recordPlay(qint64 id) {
    if (!m_db.transaction()) return;
    const bool ok = execute(m_db, "INSERT INTO history(tid,played_at) VALUES(?,?)", {id, QDateTime::currentMSecsSinceEpoch()})
        && execute(m_db, "UPDATE tracks SET play_count=play_count+1 WHERE id=?", {id});
    if (ok && m_db.commit()) emit changed(); else m_db.rollback();
}
QVector<LibraryTrack> Library::history(int limit) const { return queryTracks("JOIN history h ON h.tid=t.id ORDER BY h.played_at DESC,h.id DESC LIMIT ?", {qMax(0, limit)}); }

void Library::saveHotcues(const QString& path, const std::array<Hotcue, kNumHotcues>& cues, double cue) {
    if (!m_db.transaction()) return;
    bool ok = execute(m_db, "DELETE FROM hotcues WHERE path=?", {path});
    for (int i = 0; i < kNumHotcues; ++i)
        ok &= execute(m_db, "INSERT INTO hotcues(path,idx,position,label) VALUES(?,?,?,?)", {path, i, cues[i].positionSec, cues[i].label});
    ok &= execute(m_db, "INSERT INTO cues(path,cue) VALUES(?,?) ON CONFLICT(path) DO UPDATE SET cue=excluded.cue", {path, cue});
    if (!ok || !m_db.commit()) { m_db.rollback(); return; }
    mirror(m_db, "hotcues-locaux.tsv", "SELECT h.path,h.idx,h.position,h.label,COALESCE(c.cue,0) FROM hotcues h LEFT JOIN cues c ON c.path=h.path ORDER BY h.path,h.idx");
    emit changed();
}
void Library::loadHotcues(const QString& path, std::array<Hotcue, kNumHotcues>* cues, double* cue) {
    if (cues) *cues = {};
    if (cue) *cue = 0;
    std::array<bool, kNumHotcues> found{};
    bool cueFound = false;
    if (m_db.isOpen()) {
        QSqlQuery q(m_db); q.prepare("SELECT idx,position,label FROM hotcues WHERE path=?"); q.addBindValue(path);
        if (q.exec()) while (q.next()) {
            const int i = q.value(0).toInt();
            if (i >= 0 && i < kNumHotcues) { found[i] = true; if (cues) (*cues)[i] = {q.value(1).toDouble(), q.value(2).toString()}; }
        }
        q.prepare("SELECT cue FROM cues WHERE path=?"); q.addBindValue(path);
        if (q.exec() && q.next()) { cueFound = true; if (cue) *cue = q.value(0).toDouble(); }
    }
    for (const auto& row : readTsv("hotcues-locaux.tsv")) {
        if (row.size() != 5 || row[0] != path) continue;
        bool valid, positionOk, cueOk; const int i = row[1].toInt(&valid);
        const double position = row[2].toDouble(&positionOk), storedCue = row[4].toDouble(&cueOk);
        if (valid && i >= 0 && i < kNumHotcues && positionOk && !found[i] && cues) (*cues)[i] = {position, row[3]};
        if (!cueFound && cue && cueOk) *cue = storedCue;
    }
}
bool Library::loadGrid(const QString& path, double* bpm, double* firstBeatSec) const {
    QSqlQuery query(m_db);
    query.prepare("SELECT bpm,first_beat FROM grids WHERE path=?");
    query.addBindValue(path);
    if (!query.exec() || !query.next()) return false;
    if (bpm) *bpm = query.value(0).toDouble();
    if (firstBeatSec) *firstBeatSec = query.value(1).toDouble();
    return true;
}

void Library::saveGrid(const QString& path, double bpm, double beat) {
    if (!m_db.transaction()) return;
    const bool ok = execute(m_db, "INSERT INTO grids(path,bpm,first_beat) VALUES(?,?,?) ON CONFLICT(path) DO UPDATE SET bpm=excluded.bpm,first_beat=excluded.first_beat", {path, bpm, beat})
        && execute(m_db, "UPDATE tracks SET bpm=?,first_beat=? WHERE path=?", {bpm, beat, path});
    if (!ok || !m_db.commit()) { m_db.rollback(); return; }
    mirror(m_db, "grilles-locales.tsv", "SELECT path,bpm,first_beat FROM grids ORDER BY path"); emit changed();
}
int Library::scanDirectory(const QString& dir) {
    if (!open()) return 0;
    QStringList paths, extensions = Decoder::supportedExtensions() + QStringList{"stem.m4a", "mp4"};
    for (auto& extension : extensions) { extension = extension.toLower(); if (extension.startsWith("*.")) extension.remove(0, 2); else if (extension.startsWith('.')) extension.remove(0, 1); }
    QDirIterator it(dir, QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next(), name = QFileInfo(path).fileName();
        if (std::any_of(extensions.cbegin(), extensions.cend(), [&](const QString& ext) { return name.endsWith('.' + ext, Qt::CaseInsensitive); })) paths.append(path);
    }
    int added = 0, done = 0;
    emit scanProgress(0, paths.size());
    for (const auto& path : paths) {
        const bool isNew = trackIdForPath(path) < 0;
        TrackInfo info = Decoder::readTags(path); info.path = path;
        if (addOrUpdateTrack(info) >= 0 && isNew) ++added;
        emit scanProgress(++done, paths.size());
    }
    emit changed(); return added;
}
QStringList Library::removableVolumes() {
    QStringList paths;
    for (const auto& volume : QStorageInfo::mountedVolumes()) {
        if (!volume.isValid() || !volume.isReady() || volume.isRoot()) continue;
        const QString path = QDir::fromNativeSeparators(volume.rootPath());
#ifdef Q_OS_MACOS
        const bool removable = path.startsWith("/Volumes/") && !QFileInfo(path).fileName().startsWith("Macintosh HD", Qt::CaseInsensitive);
#elif defined(Q_OS_WIN)
        const bool removable = path.size() >= 2 && path[1] == ':' && !path.startsWith("C:", Qt::CaseInsensitive);
#else
        const bool removable = path == "/media" || path.startsWith("/media/") || path == "/run/media" || path.startsWith("/run/media/") || path == "/mnt" || path.startsWith("/mnt/");
#endif
        if (removable) paths.append(path);
    }
    paths.removeDuplicates(); paths.sort(); return paths;
}

} // namespace stemboks
