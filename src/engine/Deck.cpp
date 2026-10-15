#include "Deck.h"
#include "Decoder.h"
#include "../control/ControlRegistry.h"
#include "../library/Library.h"
#include "../stems/MixxxStemSource.h"
#include <coreservices.h>
#include <control/controlobject.h>
#include <mixer/basetrackplayer.h>
#include <mixer/playermanager.h>
#include <track/track.h>
#include <track/cue.h>
#include <track/beats.h>
#include <track/keyutils.h>
#include <library/coverartcache.h>
#include <waveform/waveform.h>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QFutureWatcher>
#include <QTimer>
#include <QtConcurrent>
#include <algorithm>
#include <cmath>

namespace stemboks {
struct Deck::Impl {
    ::BaseTrackPlayer* player = nullptr;
    ::mixxx::CoreServices* core = nullptr;
    ::TrackPointer native;
    LoadedTrackPtr snapshot;
    QString sourcePath;
    QString pendingSource;
    QString pendingLocation;
    TrackInfo sourceInfo;
    TrackInfo pendingInfo;
    int generation = 0;
    const ::Waveform* waveform = nullptr;
    int completion = -1;
    QTimer refresh;
    QVector<QMetaObject::Connection> connections;
};

Deck::Deck(int index, QObject* parent) : QObject(parent), m_index(index), m_impl(std::make_unique<Impl>()) {
    m_impl->refresh.setInterval(100);
    connect(&m_impl->refresh, &QTimer::timeout, this, &Deck::refreshTrack);
}
Deck::~Deck() { detach(); }
QString Deck::group() const { return QStringLiteral("[Channel%1]").arg(m_index + 1); }
void Deck::attach(::BaseTrackPlayer* player, ::mixxx::CoreServices* core) {
    detach();
    m_impl->player = player;
    m_impl->core = core;
    if (!player) return;
    m_impl->connections.push_back(connect(player, &::BaseTrackPlayer::newTrackLoaded, this, [this](::TrackPointer native) {
        m_impl->native = native;
        m_impl->snapshot = std::make_shared<LoadedTrack>();
        m_impl->waveform = nullptr;
        m_impl->completion = -1;
        if (native) {
            m_impl->sourceInfo = native->getLocation() == m_impl->pendingLocation ? m_impl->pendingInfo : TrackInfo();
            m_impl->sourcePath = native->getLocation() == m_impl->pendingLocation
                    ? m_impl->pendingSource : native->getLocation();
            m_impl->pendingSource.clear(); m_impl->pendingLocation.clear();
        }
        const QString migrationKey = QString::fromLatin1(QCryptographicHash::hash(
                (m_impl->sourcePath + '\n' + (native ? native->getLocation() : QString())).toUtf8(),
                QCryptographicHash::Sha256).toHex());
        const ::ConfigKey migrated("[Stemboks Migration]", migrationKey);
        const bool imported = m_impl->core->getSettings()->getValue<bool>(migrated, false);
        // Existing Mixxx metadata wins. Remember deliberate cue/grid deletion after import.
        bool hasHotcues = false;
        if (native) for (const auto& cue : native->getCuePoints()) if (cue->getHotCue() >= 0) hasHotcues = true;
        if (native && !imported && !hasHotcues) {
            std::array<Hotcue, kNumHotcues> saved;
            double mainCue = 0;
            Library::instance().loadHotcues(m_impl->sourcePath, &saved, &mainCue);
            const double rate = native->getSampleRate().value();
            if (rate > 0) {
                for (int i = 0; i < kNumHotcues; ++i) if (saved[i].positionSec >= 0) {
                    auto cue = native->createAndAddCue(::mixxx::CueType::HotCue, i,
                            ::mixxx::audio::FramePos(std::round(saved[i].positionSec * rate)), {});
                    cue->setLabel(saved[i].label);
                }
                if (mainCue > 0) {
                    const auto position = ::mixxx::audio::FramePos(std::round(mainCue * rate));
                    if (auto cue = native->findCueByType(::mixxx::CueType::MainCue)) cue->setStartPosition(position);
                    else native->createAndAddCue(::mixxx::CueType::MainCue, -1, position, {});
                }
            }
        }
        if (native && !imported && !native->getBeats()) {
            double bpm = 0, firstBeat = 0;
            if (Library::instance().loadGrid(m_impl->sourcePath, &bpm, &firstBeat)) applyBeatgrid(bpm, firstBeat);
        }
        if (native) m_impl->core->getSettings()->set(migrated, ::ConfigValue(1));
        refreshTrack();
        if (native && m_impl->sourceInfo.artwork.isNull()) ::CoverArtCache::requestTrackCover(this, native);
        ControlRegistry::instance().setString(deckKey(m_index, "load.status"), QStringLiteral("Ready"));
        emit trackLoaded(m_index);
    }));
    m_impl->connections.push_back(connect(player, &::BaseTrackPlayer::playerEmpty, this, [this] {
        m_impl->native.reset(); m_impl->snapshot.reset();
        refreshTrack();
    }));
    m_impl->connections.push_back(connect(player, &::BaseTrackPlayer::loadingTrack, this, [this](::TrackPointer, ::TrackPointer) {
        ControlRegistry::instance().setString(deckKey(m_index, "load.status"), QStringLiteral("Loading..."));
    }));
    m_impl->connections.push_back(connect(player, &::BaseTrackPlayer::loadFailed, this, [this](::TrackPointer failed, const QString& reason) {
        if (failed && !m_impl->pendingLocation.isEmpty() && failed->getLocation() != m_impl->pendingLocation) return;
        m_impl->pendingSource.clear(); m_impl->pendingLocation.clear();
        ControlRegistry::instance().setString(deckKey(m_index, "load.status"), reason);
        emit loadFailed(m_index, reason);
    }));
    if (auto* covers = ::CoverArtCache::instance()) m_impl->connections.push_back(connect(covers, &::CoverArtCache::coverFound,
            this, [this](const QObject* requester, const ::CoverInfo& cover, const QPixmap& pixmap) {
        if (requester != this || !m_impl->native || !m_impl->snapshot
                || !(cover == m_impl->native->getCoverInfoWithLocation())) return;
        m_impl->snapshot->info.artwork = pixmap.toImage();
        emit metadataChanged(m_index);
    }));
    m_impl->refresh.start();
}
void Deck::detach() {
    ++m_impl->generation;
    m_impl->refresh.stop();
    for (const auto& connection : m_impl->connections) disconnect(connection);
    m_impl->connections.clear();
    m_impl->player = nullptr; m_impl->core = nullptr;
    m_impl->pendingSource.clear(); m_impl->pendingLocation.clear();
    m_impl->native.reset(); m_impl->snapshot.reset();
}
void Deck::loadPath(const QString& path) {
    if (!m_impl->core || path.isEmpty()) return;
    persistCues();
    const int generation = ++m_impl->generation;
    const bool autoplay = ControlRegistry::instance().getBool(deckKey(m_index, "load.autoplay"));
    ControlRegistry::instance().setSilent(deckKey(m_index, "load.autoplay"), 0);
    ControlRegistry::instance().setString(deckKey(m_index, "load.status"), QStringLiteral("Loading..."));
    struct Prepared { QString location; QString error; TrackInfo info; };
    auto* watcher = new QFutureWatcher<Prepared>(this);
    connect(watcher, &QFutureWatcher<Prepared>::finished, this, [this, watcher, path, generation, autoplay] {
        const auto prepared = watcher->result(); watcher->deleteLater();
        if (generation != m_impl->generation || !m_impl->core) return;
        if (prepared.location.isEmpty()) {
            ControlRegistry::instance().setString(deckKey(m_index, "load.status"), prepared.error);
            emit loadFailed(m_index, prepared.error); return;
        }
        if (playing() && ControlRegistry::instance().getBool("app.loadLock", true)) {
            const QString message = QStringLiteral("Load cancelled: deck is playing");
            ControlRegistry::instance().setString(deckKey(m_index, "load.status"), message);
            emit loadFailed(m_index, message); return;
        }
        m_impl->pendingSource = path; m_impl->pendingLocation = prepared.location; m_impl->pendingInfo = prepared.info;
        m_impl->core->getPlayerManager()->slotLoadLocationToPlayer(prepared.location, group(), autoplay);
    });
    watcher->setFuture(QtConcurrent::run([path] {
        Prepared prepared;
        prepared.location = prepareMixxxPlaybackPath(path, &prepared.error);
        if (!prepared.location.isEmpty()) prepared.info = Decoder::readTags(path);
        return prepared;
    }));
}
void Deck::loadTrack(LoadedTrackPtr track) { if (track) loadPath(track->info.path); else unload(); }
void Deck::unload() {
    ++m_impl->generation;
    persistCues();
    if (m_impl->player) m_impl->player->slotEjectTrack(1);
}
LoadedTrackPtr Deck::track() const { return m_impl->snapshot; }
double Deck::durationSec() const { return m_impl->native ? m_impl->native->getDuration() : 0; }
double Deck::positionSec() const {
    return m_impl->player ? ::ControlObject::get(::ConfigKey(group(), "playposition")) * durationSec() : 0;
}
double Deck::effectiveBpm() const {
    return m_impl->player ? ::ControlObject::get(::ConfigKey(group(), "bpm")) : 0;
}
bool Deck::playing() const { return m_impl->player && ::ControlObject::get(::ConfigKey(group(), "play")) != 0; }
void Deck::play() { if (m_impl->player) ::ControlObject::set(::ConfigKey(group(), "play"), 1); }
void Deck::pause() { if (m_impl->player) ::ControlObject::set(::ConfigKey(group(), "play"), 0); }
void Deck::cuePress() { if (m_impl->player) ::ControlObject::set(::ConfigKey(group(), "cue_default"), 1); }
void Deck::cueRelease() { if (m_impl->player) ::ControlObject::set(::ConfigKey(group(), "cue_default"), 0); }
void Deck::seekSec(double sec) {
    if (m_impl->player && durationSec() > 0)
        ::ControlObject::set(::ConfigKey(group(), "playposition"), std::clamp(sec / durationSec(), 0.0, 1.0));
}
void Deck::jogBy(double beats) {
    if (m_impl->player) ::ControlObject::set(::ConfigKey(group(), "beatjump"), beats);
}
void Deck::scratchTo(double sec) { seekSec(sec); }
void Deck::applyBeatgrid(double bpm, double firstBeatSec) {
    auto native = m_impl->native;
    if (!native || bpm <= 0 || !std::isfinite(bpm) || !std::isfinite(firstBeatSec)) return;
    const auto rate = native->getSampleRate();
    if (!rate.isValid()) return;
    if (!native->trySetBeats(std::make_shared<::mixxx::Beats>(
            ::mixxx::audio::FramePos(std::round(firstBeatSec * rate.value())),
            ::mixxx::Bpm(bpm), rate, QString()))) {
        ControlRegistry::instance().setString(deckKey(m_index, "load.status"), QStringLiteral("Unlock the beatgrid before editing"));
        refreshTrack(); return;
    }
    refreshTrack();
    Library::instance().saveGrid(m_impl->sourcePath, bpm, firstBeatSec);
}
void Deck::persistCues() {
    refreshTrack();
    const auto snapshot = m_impl->snapshot;
    if (snapshot) Library::instance().saveHotcues(snapshot->info.path, snapshot->hotcues, snapshot->cueSec);
}
void Deck::refreshTrack() {
    auto& registry = ControlRegistry::instance();
    const auto native = m_impl->native;
    if (!native || !m_impl->snapshot) {
        registry.setSilent(deckKey(m_index, "trackLoaded"), 0);
        return;
    }
    auto& snapshot = *m_impl->snapshot;
    auto& info = snapshot.info;
    const double oldBpm = info.bpm, oldDuration = info.durationSec, oldFirstBeat = snapshot.analysis.firstBeatSec;
    const QString oldKey = info.keyText, oldTitle = info.title, oldArtist = info.artist;
    const auto& original = m_impl->sourceInfo;
    info.path = m_impl->sourcePath.isEmpty() ? native->getLocation() : m_impl->sourcePath;
    const bool converted = native->getLocation() != info.path;
    // Mixxx guesses an untagged cache file's title from its hash filename.
    info.title = converted ? original.title : native->getTitle();
    if (info.title.isEmpty()) info.title = original.title;
    if (info.title.isEmpty()) info.title = QFileInfo(info.path).completeBaseName();
    info.artist = native->getArtist(); if (info.artist.isEmpty()) info.artist = original.artist;
    info.album = native->getAlbum(); if (info.album.isEmpty()) info.album = original.album;
    info.genre = native->getGenre(); if (info.genre.isEmpty()) info.genre = original.genre;
    if (!original.artwork.isNull() && info.artwork.isNull()) info.artwork = original.artwork;
    info.comment = native->getComment(); info.durationSec = native->getDuration();
    info.sampleRate = native->getSampleRate().value(); info.bitrate = native->getBitrate();
    info.bpm = native->getBpm(); info.keyText = native->getKeyText();
    auto& analysis = snapshot.analysis;
    analysis.bpm = info.bpm; analysis.ok = info.durationSec > 0;
    const auto nativeKey = native->getKey();
    analysis.keyIndex = nativeKey == ::mixxx::track::io::key::INVALID ? -1 : ::KeyUtils::keyToTonic(nativeKey);
    analysis.keyMinor = !::KeyUtils::keyIsMajor(nativeKey);
    const double rate = info.sampleRate;
    if (const auto beats = native->getBeats(); beats && rate > 0) {
        const auto first = beats->findNextBeat(::mixxx::audio::FramePos(0));
        if (first.isValid()) analysis.firstBeatSec = first.value() / rate;
    }
    snapshot.hotcues = {};
    snapshot.cueSec = 0; snapshot.loopInSec = snapshot.loopOutSec = -1;
    for (const auto& cue : native->getCuePoints()) {
        if (rate <= 0 || !cue->getPosition().isValid()) continue;
        const double seconds = cue->getPosition().value() / rate;
        const int hotcue = cue->getHotCue();
        if (hotcue >= 0 && hotcue < kNumHotcues) snapshot.hotcues[hotcue] = {seconds, cue->getLabel()};
        if (cue->getType() == ::mixxx::CueType::MainCue) snapshot.cueSec = seconds;
    }
    auto read = [this](const char* name) { return ::ControlObject::get(::ConfigKey(group(), QString::fromLatin1(name))); };
    if (rate > 0) {
        const double start = read("loop_start_position"), end = read("loop_end_position");
        if (start >= 0) snapshot.loopInSec = start / (2 * rate);
        if (end >= 0) snapshot.loopOutSec = end / (2 * rate);
    }
#ifdef __STEM__
    snapshot.hasStems = native->hasStem();
#endif
    auto waveform = native->getWaveformSummary();
    if (!waveform || waveform->getCompletion() == 0) waveform = native->getWaveform();
    if (waveform && (m_impl->waveform != waveform.get() || m_impl->completion != waveform->getCompletion())) {
        m_impl->waveform = waveform.get(); m_impl->completion = waveform->getCompletion();
        const int count = std::min(waveform->getDataSize(), waveform->getCompletion());
        auto& display = analysis.waveform;
        display.durationSec = info.durationSec;
        // Mixxx stores adjacent left/right visual samples.
        display.peaks.resize(std::max(0, int(std::ceil(info.durationSec * 100))));
        for (int i = 0; i < display.peaks.size(); ++i) {
            const int source = int(double(i) * (waveform->getDataSize() / 2) / display.peaks.size()) * 2;
            if (source + 1 >= count) { display.peaks[i] = {}; continue; }
            display.peaks[i] = {std::max(waveform->getLow(source), waveform->getLow(source + 1)) / 255.0f,
                    std::max(waveform->getMid(source), waveform->getMid(source + 1)) / 255.0f,
                    std::max(waveform->getHigh(source), waveform->getHigh(source + 1)) / 255.0f};
        }
    }
    registry.setString(deckKey(m_index, "track.title"), info.title);
    registry.setString(deckKey(m_index, "track.artist"), info.artist);
    registry.setString(deckKey(m_index, "track.path"), info.path);
    registry.setSilent(deckKey(m_index, "trackLoaded"), 1);
    registry.setSilent(deckKey(m_index, "stemsReady"), snapshot.hasStems);
    registry.setSilent(deckKey(m_index, "durationSec"), info.durationSec);
    registry.setSilent(deckKey(m_index, "positionSec"), positionSec());
    registry.setSilent(deckKey(m_index, "fileBpm"), info.bpm);
    for (int i = 0; i < kNumHotcues; ++i)
        registry.setSilent(deckKey(m_index, qPrintable(QString("hotcue.%1.present").arg(i + 1))), snapshot.hotcues[i].positionSec >= 0);
    if (oldBpm != info.bpm || oldDuration != info.durationSec || oldFirstBeat != analysis.firstBeatSec
            || oldKey != info.keyText || oldTitle != info.title || oldArtist != info.artist)
        emit metadataChanged(m_index);
}
} // namespace stemboks
