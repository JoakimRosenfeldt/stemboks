#include "AudioEngine.h"
#include "MixxxControlBridge.h"
#include "control/AppSettings.h"

#include <QApplication>
#include <QByteArray>
#include <QDebug>
#include <QCryptographicHash>
#include <QDialog>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QSaveFile>
#include <QWindow>

#include "control/pollingcontrolproxy.h"
#include "controllers/controller.h"
#include "controllers/controllermanager.h"
#include "coreservices.h"
#include "mixer/playermanager.h"
#include "recording/defs_recording.h"
#include "recording/recordingmanager.h"
#include "soundio/soundmanager.h"
#include "util/cmdlineargs.h"

namespace stemboks {

namespace {
QString resourceDirectory() {
    const QDir appDir(QCoreApplication::applicationDirPath());
    QStringList candidates{
        appDir.filePath(QStringLiteral("../Resources/mixxx")),
        appDir.filePath(QStringLiteral("mixxx")),
        appDir.filePath(QStringLiteral("../share/stemboks/mixxx"))};
#ifdef STEMBOKS_MIXXX_RESOURCE_DIR
    candidates.append(QString::fromUtf8(STEMBOKS_MIXXX_RESOURCE_DIR));
#endif
    for (const auto& path : candidates) {
        if (QFileInfo::exists(QDir(path).filePath(QStringLiteral("schema.xml")))) {
            return QDir(path).absolutePath();
        }
    }
    return {};
}

double controlValue(const QString& group, const QString& name) {
    return PollingControlProxy(group, name, ControlFlag::AllowMissingOrInvalid).get();
}
}

struct AudioEngine::Impl {
    std::unique_ptr<::mixxx::CoreServices> core;
    std::unique_ptr<MixxxControlBridge> bridge;
    std::shared_ptr<QDialog> preferences;
    double recordedSeconds = 0;
};

AudioEngine& AudioEngine::instance() {
    static AudioEngine engine;
    return engine;
}

AudioEngine::AudioEngine() : m_impl(std::make_unique<Impl>()) {
    for (int i = 0; i < kNumDecks; ++i) {
        m_decks[i] = std::make_unique<Deck>(i, this);
    }
}

AudioEngine::~AudioEngine() {
    stop();
}

::mixxx::CoreServices* AudioEngine::core() const {
    return m_impl->core.get();
}

bool AudioEngine::start() {
    if (core()) return true;
    auto* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    if (!app) return false;

    const QString resources = resourceDirectory();
    if (resources.isEmpty()) {
        qCritical() << "Mixxx resources were not found";
        return false;
    }
    const QString settingsPath = AppSettings::dataDir() + QStringLiteral("/mixxx-engine");
    if (!QDir().mkpath(settingsPath)) {
        qCritical() << "Cannot create Mixxx settings directory" << settingsPath;
        return false;
    }

    app->setProperty("stemboksEmbedded", true);
    QList<QByteArray> arguments{
        QCoreApplication::applicationFilePath().toLocal8Bit(),
        QByteArrayLiteral("--settings-path"), settingsPath.toLocal8Bit(),
        QByteArrayLiteral("--resource-path"), resources.toLocal8Bit()};
    QVector<char*> argv;
    argv.reserve(arguments.size());
    for (auto& argument : arguments) argv.append(argument.data());
    auto& args = CmdlineArgs::Instance();
    if (!args.parse(argv.size(), argv.data())) return false;

    m_impl->core = std::make_unique<::mixxx::CoreServices>(args, app);
    const auto settings = core()->getSettings();
    settings->setValue(ConfigKey(QStringLiteral("[App]"), QStringLiteral("num_decks")), kNumDecks);
    settings->setValue(ConfigKey(QStringLiteral("[App]"), QStringLiteral("num_samplers")), kNumSamplers);
    core()->initialize(app);

    if (!core()->getPlayerManager() || !core()->getSoundManager() ||
        !core()->getControllerManager() || !core()->getRecordingManager()) {
        qCritical() << "Mixxx services could not initialize";
        stop();
        return false;
    }
    const auto players = core()->getPlayerManager();
    const auto sound = core()->getSoundManager();
    const auto controllers = core()->getControllerManager();
    const auto recorder = core()->getRecordingManager();
    players->slotChangeNumDecks(kNumDecks);
    players->slotChangeNumSamplers(kNumSamplers);
    for (int i = 0; i < kNumDecks; ++i) {
        m_decks[i]->attach(players->getDeckBase(i), core());
    }
    // Upstream preference constructors initialize rate, mixer and analyzer controls.
    m_impl->preferences = core()->makeDlgPreferences();
    m_impl->bridge = std::make_unique<MixxxControlBridge>(core(), this);

    connect(sound.get(), &SoundManager::devicesUpdated, this, &AudioEngine::devicesChanged);
    connect(sound.get(), &SoundManager::devicesSetup, this, &AudioEngine::devicesChanged);
    connect(controllers.get(), &::ControllerManager::devicesChanged, this, &AudioEngine::devicesChanged);
    connect(recorder.get(), &RecordingManager::durationRecorded, this, [this](const QString& duration) {
        const auto parts = duration.split(QChar(':'));
        double seconds = 0;
        for (const auto& part : parts) seconds = seconds * 60 + part.toDouble();
        m_impl->recordedSeconds = seconds;
    });
    connect(recorder.get(), &RecordingManager::isRecording, this, [this](bool active) {
        if (active) m_impl->recordedSeconds = 0;
    });

    const auto status = sound->setupDevices();
    if (status != SoundDeviceStatus::Ok) {
        qWarning() << "Mixxx audio setup:" << sound->getLastErrorMessage(status);
    }
    controllers->setUpDevices();
    emit started();
    return true;
}

void AudioEngine::stop() {
    if (!core()) return;
    if (m_impl->preferences) {
        m_impl->preferences->close();
        m_impl->preferences.reset();
    }
    m_impl->bridge.reset();
    for (auto& deck : m_decks) deck->detach();
    if (const auto recorder = core()->getRecordingManager()) recorder->stopRecording();
    if (const auto sound = core()->getSoundManager()) sound->closeActiveConfig();
    m_impl->core.reset();
    m_impl->recordedSeconds = 0;
}

int AudioEngine::sampleRate() const {
    if (!core()) return 44100;
    const int activeRate = static_cast<int>(controlValue(QStringLiteral("[App]"), QStringLiteral("samplerate")));
    return activeRate > 0 ? activeRate : static_cast<int>(core()->getSoundManager()->getConfig().getSampleRate().value());
}

QVector<AudioEngine::DeviceInfo> AudioEngine::outputDevices() const {
    QVector<DeviceInfo> result;
    if (!core()) return result;
    const auto sound = core()->getSoundManager();
    for (const auto& device : sound->getDeviceList(sound->getConfig().getAPI(), true, false)) {
        if (device->getDeviceId().portAudioIndex >= 0) {
            result.append({device->getDisplayName(), static_cast<unsigned int>(device->getDeviceId().portAudioIndex)});
        }
    }
    return result;
}

QStringList AudioEngine::apiNames() const {
    if (!core()) return {};
    return core()->getSoundManager()->getHostAPIList();
}

void AudioEngine::showPreferences(QWidget* parent) {
    if (!core()) return;
    if (!m_impl->preferences) m_impl->preferences = core()->makeDlgPreferences();
    auto& dialog = m_impl->preferences;
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->show();
    if (parent && parent->window()->windowHandle() && dialog->windowHandle()) {
        dialog->windowHandle()->setTransientParent(parent->window()->windowHandle());
    }
    dialog->raise();
    dialog->activateWindow();
}

QStringList AudioEngine::connectedControllers() const {
    QStringList result;
    if (!core()) return result;
    for (const auto* controller : core()->getControllerManager()->getControllers()) {
        result.append(controller->getName());
    }
    result.removeDuplicates();
    return result;
}

void AudioEngine::rescanControllers() {
    if (core()) core()->getControllerManager()->setUpDevices();
}

void AudioEngine::loadSampler(int slot, const QString& path) {
    if (!core() || slot < 0 || slot >= kNumSamplers) return;
    QString location = path;
    if (path.startsWith(QStringLiteral(":"))) {
        QFile resource(path);
        if (!resource.open(QIODevice::ReadOnly)) {
            qWarning() << "Cannot open sampler resource" << path;
            return;
        }
        const QByteArray bytes = resource.readAll();
        const QString directory = AppSettings::cacheDir() + QStringLiteral("/samplers");
        if (bytes.isEmpty() || !QDir().mkpath(directory)) return;
        const QString hash = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
        location = directory + QChar('/') + hash + QChar('.') + QFileInfo(path).suffix();
        if (!QFileInfo::exists(location)) {
            QSaveFile file(location);
            if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
                qWarning() << "Cannot cache sampler resource" << path << file.errorString();
                return;
            }
        }
    }
    core()->getPlayerManager()->slotLoadToSampler(location, slot + 1);
}

void AudioEngine::triggerSampler(int slot, bool on) {
    if (!core() || slot < 0 || slot >= kNumSamplers) return;
    PollingControlProxy(PlayerManager::groupForSampler(slot), QStringLiteral("play")).set(on ? 1 : 0);
}

bool AudioEngine::samplerPlaying(int slot) const {
    return core() && slot >= 0 && slot < kNumSamplers &&
        controlValue(PlayerManager::groupForSampler(slot), QStringLiteral("play")) > 0;
}

void AudioEngine::setRecording(bool on, const QString& path, bool mp3, int bitDepth) {
    if (!core()) return;
    const auto recorder = core()->getRecordingManager();
    if (!on) {
        recorder->stopRecording();
        return;
    }
    if (recording()) return;
    const QString directory = QFileInfo(path).absolutePath();
    if (!QDir().mkpath(directory)) {
        qWarning() << "Cannot create recording directory" << directory;
        return;
    }
    const auto settings = core()->getSettings();
    settings->setValue(ConfigKey(RECORDING_PREF_KEY, QStringLiteral("Directory")), directory);
    settings->setValue(ConfigKey(RECORDING_PREF_KEY, QStringLiteral("Encoding")),
        mp3 ? QStringLiteral(ENCODING_MP3) : QStringLiteral(ENCODING_WAVE));
    settings->setValue(ConfigKey(RECORDING_PREF_KEY, QStringLiteral("WAV_BITS")),
        bitDepth == 32 ? 2 : bitDepth == 24 ? 1 : 0);
    m_impl->recordedSeconds = 0;
    recorder->startRecording();
}

bool AudioEngine::recording() const {
    return core() && controlValue(QStringLiteral(RECORDING_PREF_KEY), QStringLiteral("status")) != RECORD_OFF;
}

double AudioEngine::recordedSeconds() const {
    return m_impl->recordedSeconds;
}

float AudioEngine::masterLevelL() const {
    return core() ? static_cast<float>(controlValue(QStringLiteral("[Main]"), QStringLiteral("vu_meter_left"))) : 0;
}

float AudioEngine::masterLevelR() const {
    return core() ? static_cast<float>(controlValue(QStringLiteral("[Main]"), QStringLiteral("vu_meter_right"))) : 0;
}

} // namespace stemboks
