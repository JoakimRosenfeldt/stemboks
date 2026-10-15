#pragma once

#include "Deck.h"

#include <QObject>
#include <QStringList>
#include <QVector>
#include <array>
#include <memory>

class QWidget;
namespace mixxx {
class CoreServices;
}

namespace stemboks {

class AudioEngine : public QObject {
    Q_OBJECT
public:
    static AudioEngine& instance();
    ~AudioEngine() override;

    Deck* deck(int i) { return m_decks.at(i).get(); }
    ::mixxx::CoreServices* core() const;
    bool start();
    void stop();
    int sampleRate() const;

    struct DeviceInfo { QString name; unsigned int id; };
    QVector<DeviceInfo> outputDevices() const;
    QStringList apiNames() const;
    void showPreferences(QWidget* parent = nullptr);
    QStringList connectedControllers() const;
    void rescanControllers();

    void loadSampler(int slot, const QString& path);
    void triggerSampler(int slot, bool on);
    bool samplerPlaying(int slot) const;

    void setRecording(bool on, const QString& path, bool mp3, int bitDepth);
    bool recording() const;
    double recordedSeconds() const;
    float masterLevelL() const;
    float masterLevelR() const;

signals:
    void started();
    void devicesChanged();

private:
    AudioEngine();
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::array<std::unique_ptr<Deck>, kNumDecks> m_decks;
};

} // namespace stemboks
