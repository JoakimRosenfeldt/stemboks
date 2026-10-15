#pragma once
#include "Types.h"
#include <QObject>
#include <memory>

class BaseTrackPlayer;
namespace mixxx { class CoreServices; }

namespace stemboks {

// Metadata and transport facade. Mixxx owns playback, analysis, and cue state.
class Deck : public QObject {
    Q_OBJECT
public:
    explicit Deck(int index, QObject* parent = nullptr);
    ~Deck() override;
    int index() const { return m_index; }
    void attach(::BaseTrackPlayer* player, ::mixxx::CoreServices* core);
    void detach();
    void loadPath(const QString& path);
    void loadTrack(LoadedTrackPtr track);
    void unload();
    LoadedTrackPtr track() const;
    bool hasTrack() const { return bool(track()); }
    double positionSec() const;
    double durationSec() const;
    double effectiveBpm() const;
    bool playing() const;
    void play();
    void pause();
    void cuePress();
    void cueRelease();
    void seekSec(double sec);
    void jogBy(double deltaBeats);
    void scratchTo(double posSec);
    void applyBeatgrid(double bpm, double firstBeatSec);
    void persistCues();

signals:
    void trackLoaded(int deck);
    void trackEnded(int deck);
    void metadataChanged(int deck);
    void loadFailed(int deck, const QString& reason);

private:
    void refreshTrack();
    QString group() const;
    int m_index;
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace stemboks
