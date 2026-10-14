#include "MainWindow.h"
#include "DeckWidget.h"
#include "WaveformWidget.h"
#include "LibraryWidget.h"
#include "PrefsWidget.h"
#include "StemsManagerWidget.h"
#include "SecondScreen.h"
#include "UiHelpers.h"
#include "../engine/AudioEngine.h"
#include "../engine/Analysis.h"
#include "../engine/Decoder.h"
#include "../stems/StemManager.h"
#include <QApplication>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QScreen>
#include <QSignalBlocker>
#include <QDir>
#include <QSlider>
#include <QStackedWidget>
#include <QTemporaryFile>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>
#include <QtConcurrent>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Stemboks"); setAcceptDrops(true); resize(1440, 900);
    auto* root = new QWidget; root->setObjectName("StemboxRoot"); setCentralWidget(root);
    auto* layout = new QVBoxLayout(root); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(0);
    m_timer = new QTimer(this); m_body = new QStackedWidget; m_body->setObjectName("StemboxBody");
    applySettingsToRegistry();
    m_library = new LibraryWidget; m_prefs = new PrefsWidget; m_stems = new StemsManagerWidget;
    buildNav(); layout->addWidget(m_body, 1); buildMix();
    m_body->addWidget(m_library); m_body->addWidget(m_prefs); m_body->addWidget(m_stems);
    connect(m_library, &LibraryWidget::loadToDeck, this, &MainWindow::loadTrackToDeck);
    connect(m_library, &LibraryWidget::closed, this, &MainWindow::showMix);
    connect(m_library, &LibraryWidget::openStemsManager, this, [this] { m_body->setCurrentWidget(m_stems); });
    connect(m_prefs, &PrefsWidget::closed, this, &MainWindow::showMix);
    connect(m_stems, &StemsManagerWidget::closed, this, [this] { m_library->showPage(LibraryWidget::Source); m_body->setCurrentWidget(m_library); });
    connect(m_body, &QStackedWidget::currentChanged, this, [this](int) {
        const bool overlay = m_body->currentWidget() == m_prefs || m_body->currentWidget() == m_stems;
        m_deckBars->setVisible(!overlay); m_mixerStrip->setVisible(!overlay && m_mixerStrip->property("expanded").toBool());
        ControlRegistry::instance().set("app.libraryOpen", m_body->currentWidget() == m_library);
    });
    connect(&AppSettings::instance(), &AppSettings::changed, this, [this](const QString&, const QString&) {
        if (!m_applyingSettings) ControlRegistry::instance().set("app.settingsDirty", 1);
    });
    auto* hotcueSave = new QTimer(this); hotcueSave->setSingleShot(true); hotcueSave->setInterval(300);
    connect(hotcueSave, &QTimer::timeout, this, &MainWindow::saveHotcues);
    connect(&ControlRegistry::instance(), &ControlRegistry::changed, this, [hotcueSave](const QString& name, double value) {
        if (value && name.startsWith("deck.") && ((name.contains(".hotcue.") && (name.endsWith(".set") || name.endsWith(".clear"))) || name.endsWith(".transport.cue"))) hotcueSave->start();
    });
    connect(&StemManager::instance(), &StemManager::jobFinished, this, [this](const QString& path, bool ok) {
        if (!ok) return; auto& library = Library::instance(); library.setStemsReady(library.trackIdForPath(path), true);
        for (int i = 0; i < 4; ++i) {
            auto* deck = AudioEngine::instance().deck(i); const auto track = deck->track();
            if (track && track->info.path == path && !deck->playing()) loadTrackToDeck(i, path);
        }
    });
    connect(m_timer, &QTimer::timeout, this, [this] {
        auto& r = ControlRegistry::instance(); if (r.getBool("app.settingsDirty")) applySettingsToRegistry();
        for (auto& ui : m_ui) { ui.bar->refresh(); ui.wave->update(); }
        if (m_second && m_second->isVisible()) m_second->refresh();
    });
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] { saveHotcues(); AppSettings::instance().save(); AudioEngine::instance().setRecording(false, {}, false, 16); });
    connect(qApp, &QGuiApplication::screenAdded, this, [this](QScreen*) { updateSecondScreen(); });
    connect(qApp, &QGuiApplication::screenRemoved, this, [this](QScreen*) { updateSecondScreen(); });
    applySettingsToRegistry(); showMix(); m_timer->start();
}

void MainWindow::buildNav() {
    auto* root = qobject_cast<QVBoxLayout*>(centralWidget()->layout());
    const auto browse = [this](LibraryWidget::Page page, int target) {
        m_library->setTargetDeck(target); m_library->showPage(page); m_body->setCurrentWidget(m_library);
    };
    const auto row = [this, browse](bool right, bool large) {
        auto* widget = new QWidget; widget->setFixedHeight(46); widget->setObjectName(large ? "StemboxNavRowLarge" : "StemboxNavRow");
        auto* l = new QHBoxLayout(widget); l->setContentsMargins(6, 3, 6, 3); l->setSpacing(5);
        if (large) l->addSpacing(192);
        if (!right || large) { auto* b = Ui::button("EXIT", "StemboxNavExit"); l->addWidget(b, 1); connect(b, &QPushButton::clicked, this, &QWidget::close); }
        if (right || large) { auto* b = Ui::button("MENU", "StemboxNavMenu"); l->addWidget(b, 1); connect(b, &QPushButton::clicked, this, [this] { if (m_body->currentWidget() == m_prefs) showMix(); else m_body->setCurrentWidget(m_prefs); }); }
        const QStringList labels{"BROWSE", "TAG LIST", "SEARCH"}; const char* names[] = {"StemboxNavBrowse", "StemboxNavPlaylist", "StemboxNavDisplay"};
        const LibraryWidget::Page pages[] = {LibraryWidget::Source, LibraryWidget::TagList, LibraryWidget::Search};
        for (int i = 0; i < labels.size(); ++i) {
            auto* b = Ui::button(labels[i], names[i]); l->addWidget(b, 1);
            connect(b, &QPushButton::clicked, this, [browse, page = pages[i], right] { browse(page, right ? 1 : 0); });
        }
        if (large) l->addSpacing(192); return widget;
    };
    m_navLarge = row(false, true); root->addWidget(m_navLarge);
    m_navSplit = new QWidget; m_navSplit->setObjectName("StemboxNavRowWrap"); auto* split = new QHBoxLayout(m_navSplit); split->setContentsMargins(0, 0, 0, 0); split->setSpacing(40);
    split->addWidget(row(false, false), 1); split->addWidget(row(true, false), 1); root->addWidget(m_navSplit);
}

void MainWindow::buildMix() {
    m_mixPage = new QWidget; m_waveLayout = new QVBoxLayout(m_mixPage); m_waveLayout->setContentsMargins(0, 0, 0, 0); m_waveLayout->setSpacing(0);
    m_deckBars = new QWidget; auto* bars = new QHBoxLayout(m_deckBars); bars->setContentsMargins(0, 0, 0, 0); bars->setSpacing(0);
    for (int side = 0; side < 2; ++side) {
        auto* column = new QWidget; auto* col = new QVBoxLayout(column); col->setContentsMargins(0, 0, 0, 0); col->setSpacing(0);
        auto* selectors = new QWidget; selectors->setObjectName("StemboxDeckSelectors"); auto* tabs = new QHBoxLayout(selectors); tabs->setContentsMargins(0, 0, 0, 0);
        m_waveStacks[side] = new QStackedWidget; m_barStacks[side] = new QStackedWidget;
        for (int alternate = 0; alternate < 2; ++alternate) {
            const int index = side + alternate * 2; auto* deck = AudioEngine::instance().deck(index);
            m_ui[index].wave = new WaveformWidget(deck); m_ui[index].bar = new DeckWidget(deck);
            m_waveStacks[side]->addWidget(m_ui[index].wave); m_barStacks[side]->addWidget(m_ui[index].bar);
            auto* b = Ui::button(QString("DECK %1").arg(index + 1)); tabs->addWidget(b);
            connect(b, &QPushButton::clicked, this, [this, index] { selectDeck(index); });
        }
        col->addWidget(selectors); col->addWidget(m_waveStacks[side], 1); m_waveLayout->addWidget(column, 1); bars->addWidget(m_barStacks[side], 1);
        if (side == 0) {
            auto* barDivider = new QWidget; barDivider->setFixedWidth(40); auto* barLayout = new QVBoxLayout(barDivider); barLayout->setContentsMargins(0, 0, 0, 0);
            auto* mixerButton = Ui::button("MIXER"); mixerButton->setStyleSheet("font-size:8px;padding:3px 0px;"); barLayout->addStretch(); barLayout->addWidget(mixerButton); barLayout->addStretch(); bars->addWidget(barDivider);
            connect(mixerButton, &QPushButton::clicked, this, [this] { const bool show = !m_mixerStrip->property("expanded").toBool(); m_mixerStrip->setProperty("expanded", show); m_mixerStrip->setVisible(show); });
            m_waveColumns = new QWidget; m_waveColumns->setFixedWidth(40); auto* divider = new QVBoxLayout(m_waveColumns); divider->setContentsMargins(0, 0, 0, 0);
            divider->addStretch(); auto* toggle = Ui::button("M\nI\nX", "StemboxMixerToggle"); toggle->setToolTip("Show mixer, M"); divider->addWidget(toggle); divider->addStretch();
            connect(toggle, &QPushButton::clicked, this, [this] { const bool show = !m_mixerStrip->property("expanded").toBool(); m_mixerStrip->setProperty("expanded", show); m_mixerStrip->setVisible(show); }); m_waveLayout->addWidget(m_waveColumns);
        }
    }
    m_body->addWidget(m_mixPage); buildMixerStrip(centralWidget());
    qobject_cast<QVBoxLayout*>(centralWidget()->layout())->addWidget(m_deckBars);
}

void MainWindow::buildMixerStrip(QWidget* parent) {
    m_mixerStrip = new QWidget(parent); m_mixerStrip->setObjectName("StemboxMixerStrip"); m_mixerStrip->setProperty("expanded", false);
    auto* row = new QHBoxLayout(m_mixerStrip); row->setContentsMargins(6, 4, 6, 4);
    const auto addSlider = [this](QVBoxLayout* col, const QString& title, const QString& key, int minimum, int maximum, double def, double factor) {
        col->addWidget(Ui::label(title, "StemboxCaption")); auto* s = new QSlider(Qt::Horizontal); s->setRange(minimum, maximum); s->setValue(qRound(ControlRegistry::instance().get(key, def) * factor)); col->addWidget(s);
        connect(s, &QSlider::valueChanged, this, [key, factor](int value) { ControlRegistry::instance().set(key, value / factor); });
        connect(m_timer, &QTimer::timeout, s, [s, key, factor] { if (!s->isSliderDown()) { const QSignalBlocker block(s); s->setValue(qRound(ControlRegistry::instance().get(key) * factor)); } });
    };
    for (int deck = 0; deck < 4; ++deck) {
        auto* col = new QVBoxLayout; col->setSpacing(1); col->addWidget(Ui::label(QString("CH %1").arg(deck + 1), "StemboxCaption"));
        addSlider(col, "GAIN", deckKey(deck, "level.gain"), 0, 200, 1, 100);
        for (const char* eq : {"low", "mid", "high"}) addSlider(col, QString::fromLatin1(eq).toUpper(), QString("deck.%1.eq.%2").arg(deck + 1).arg(eq), -100, 100, 0, 100);
        addSlider(col, "FILTER", deckKey(deck, "colorfx.amount"), -100, 100, 0, 100);
        addSlider(col, "VOLUME", deckKey(deck, "level.volume"), 0, 100, 1, 100);
        auto* pfl = Ui::button("PFL"); pfl->setCheckable(true); col->addWidget(pfl);
        connect(pfl, &QPushButton::clicked, this, [deck] { ControlRegistry::instance().toggle(deckKey(deck, "level.pfl")); });
        auto* vu = new QProgressBar; vu->setRange(0, 100); vu->setTextVisible(false); vu->setFixedHeight(8); col->addWidget(vu);
        connect(m_timer, &QTimer::timeout, vu, [vu, pfl, deck] { vu->setValue(qRound(ControlRegistry::instance().get(deckKey(deck, "level")) * 100)); pfl->setChecked(ControlRegistry::instance().getBool(deckKey(deck, "level.pfl"))); }); row->addLayout(col, 1);
    }
    auto* master = new QVBoxLayout;
    addSlider(master, "CROSSFADER", "mixer.crossfader.position", -100, 100, 0, 100);
    addSlider(master, "MASTER", "mixer.master.gain", 0, 200, 1, 100);
    addSlider(master, "HEAD MIX", "mixer.head.mix", 0, 100, 0, 100);
    addSlider(master, "HEAD LEVEL", "mixer.head.gain", 0, 200, 1, 100);
    auto* vu = new QProgressBar; vu->setRange(0, 100); vu->setTextVisible(false); master->addWidget(vu);
    connect(m_timer, &QTimer::timeout, vu, [vu] { auto& e = AudioEngine::instance(); vu->setValue(qRound(std::max(e.masterLevelL(), e.masterLevelR()) * 100)); });
    auto* hide = Ui::button("HIDE MIXER"); master->addWidget(hide); connect(hide, &QPushButton::clicked, this, [this] { m_mixerStrip->setProperty("expanded", false); m_mixerStrip->hide(); });
    row->addLayout(master, 2); qobject_cast<QVBoxLayout*>(parent->layout())->addWidget(m_mixerStrip); m_mixerStrip->hide();
}

void MainWindow::selectDeck(int deck) {
    if (deck < 0 || deck > 3) return; m_selectedDeck = deck; const int side = deck % 2;
    m_waveStacks[side]->setCurrentIndex(deck / 2); m_barStacks[side]->setCurrentIndex(deck / 2);
    m_library->setTargetDeck(deck); ControlRegistry::instance().set("app.focusedDeck", deck + 1);
}
void MainWindow::showMix() { m_body->setCurrentWidget(m_mixPage); }

void MainWindow::applySettingsToRegistry() {
    if (m_applyingSettings) return; m_applyingSettings = true;
    auto& s = AppSettings::instance(); auto& r = ControlRegistry::instance(); const QString g = "[Stembox]";
    const QStringList padNames{"ROLL", "ECHO", "VINYL BRAKE", "REVERB", "ENIGMA JET", "PHASER", "FLANGER", "TRANS"};
    const QStringList padLabels{"1/2", "1", "", "50%", "+1/16", "1/16", "1/16", "1/16"};
    const double padValues[] = {.5, 1, 0, .5, .0625, .0625, .0625, .0625};
    const int padEffects[] = {-2,2,9,1,4,5,6,0}, padParams[] = {0,1,0,1,1,1,1,2};
    for (int i = 0; i < 8; ++i) {
        const auto defaultSetting = [&](const QString& key, const QString& value) { const auto full = key + QString::number(i + 1); if (s.get(g, full).isNull()) s.set(g, full, value); };
        defaultSetting("padfx_name_", padNames[i]); defaultSetting("padfx_vlabel_", padLabels[i]); defaultSetting("padfx_bval_", QString::number(padValues[i]));
        defaultSetting("padfx_effect_", QString::number(padEffects[i])); defaultSetting("padfx_bparam_", QString::number(padParams[i])); defaultSetting("padfx_releasefx_", "0");
        r.setString(QString("app.padfxName.%1").arg(i + 1), s.get(g, QString("padfx_name_%1").arg(i + 1)));
        r.set(QString("app.padfxValue.%1").arg(i + 1), s.getDouble(g, QString("padfx_bval_%1").arg(i + 1)));
        r.set(QString("mixer.eq.%1").arg(i + 1), s.getDouble("[Mixer]", QString("eq_gain_%1").arg(i + 1)));
    }
    struct Mirror { const char* group; const char* setting; const char* control; double def; };
    const Mirror mirrors[] = {
        {"[Mixer]", "eq_curve", "app.eqCurve", 1}, {"[Mixer]", "eq_sensitivity", "app.eqSensitivity", 100},
        {"[Mixer]", "auto_gain", "app.autoGain", 1}, {"[Mixer]", "master_limiter", "app.masterLimiter", 1},
        {"[Mixer]", "xfader_mode", "app.xfaderMode", 1}, {"[Mixer]", "xfader_curve", "app.xfaderCurve", 1},
        {"[Mixer]", "xfader_disable", "app.xfaderDisable", 0}, {"[Mixer]", "xfader_reverse", "app.xfaderReverse", 0},
        {"[Controls]", "RateDir", "app.tempoInvert", 0}, {"[Stembox]", "hotcue_mode", "app.hotcueMode", 0},
        {"[Stembox]", "backspin", "app.backspin", 4}, {"[Stembox]", "track_end_warning", "app.trackEndWarning", 30},
        {"[Stembox]", "frame_rate", "app.frameRate", 60}, {"[Stembox]", "key_colors", "app.keyColors", 0},
        {"[Stembox]", "touch_waveform", "app.touchWaveform", 0}, {"[Stembox]", "brightness", "app.brightness", 100},
        {"[Stembox]", "contrast", "app.contrast", 100}, {"[Stembox]", "mix_mode", "app.mixMode", 0},
        {"[Stembox]", "waveform_color", "app.waveformColor", 1}, {"[Stembox]", "preview_style", "app.previewStyle", 0},
        {"[Stembox]", "load_lock", "app.loadLock", 1}, {"[Stembox]", "colorfx_select_only", "app.colorFxSelectOnly", 0},
        {"[Stembox]", "padfx_type", "app.padfxType", 0}, {"[Stembox]", "padfx_release_s", "app.padfxReleaseSeconds", 3},
        {"[Stembox]", "theme", "app.theme", 0}, {"[Stembox]", "interface_size", "app.interfaceSize", 100},
        {"[Stembox]", "vinyl_start_stop", "app.vinylStartStop", 0}, {"[Stembox]", "multi_meta", "app.multiMeta", 0},
        {"[Stembox]", "tag_remove_played", "app.tagRemovePlayed", 0}, {"[Stembox]", "keyboard_azerty", "app.keyboardAzerty", 0},
        {"[Stembox]", "force_second_screen", "app.forceSecondScreen", 0}, {"[Stembox]", "screen_usage", "app.screenUsage", 0}
    };
    for (const auto& mirror : mirrors) r.set(mirror.control, s.getDouble(mirror.group, mirror.setting, mirror.def));
    r.set("app.playMarkerCenter", s.getInt(g, "play_marker_position", 1) == 1);
    r.set("app.timeDisplay", s.getInt(g, "time_display", s.getInt("[Controls]", "ShowDurationRemaining", 1) ? 0 : 1));
    const bool split = s.getInt(g, "mix_mode") == 1;
    if (m_navLarge) { m_navLarge->setVisible(!split); m_navSplit->setVisible(split); }
    if (m_waveLayout) {
        m_waveLayout->setDirection(split ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom); m_waveColumns->setVisible(split);
        for (auto* tabs : m_mixPage->findChildren<QWidget*>("StemboxDeckSelectors")) tabs->setVisible(split);
    }
    for (auto& ui : m_ui) if (ui.wave) {
        ui.wave->setColorMode(static_cast<WaveformWidget::ColorMode>(std::clamp(s.getInt(g, "waveform_color", 1), 0, 2)));
        ui.wave->setMarkerCenter(s.getInt(g, "play_marker_position", 1) == 1);
    }
    const QString themePath = s.getInt(g, "theme") == 1 ? ":/theme/style-stemboks-light.qss" : ":/theme/style-stemboks.qss";
    if (qApp->property("stemboksTheme").toString() != themePath) { QFile style(themePath); if (style.open(QIODevice::ReadOnly)) qApp->setStyleSheet(QString::fromUtf8(style.readAll())); qApp->setProperty("stemboksTheme", themePath); }
    QFont font("Open Sans"); font.setPointSizeF(11 * std::clamp(s.getInt(g, "interface_size", 100), 60, 140) / 100.0); qApp->setFont(font);
    m_timer->setInterval(1000 / std::clamp(s.getInt(g, "frame_rate", 60), 30, 60));
    const QStringList drums{"kick", "snare", "clap", "hihat"};
    for (int i = 0; i < 8; ++i) {
        QString path = s.get(g, QString("sampler_%1_path").arg(i + 1));
        if (path.isEmpty() && i >= 4) path = ":/drums/" + drums[i - 4] + ".wav";
        if (path == m_samplerPaths[i]) continue; m_samplerPaths[i] = path;
        if (path.isEmpty()) { AudioEngine::instance().loadSampler(i, {}, {}); continue; }
        auto* watcher = new QFutureWatcher<AudioBufferPtr>(this);
        connect(watcher, &QFutureWatcher<AudioBufferPtr>::finished, this, [this, watcher, i, path] {
            if (m_samplerPaths[i] == path) AudioEngine::instance().loadSampler(i, watcher->result(), QFileInfo(path).completeBaseName()); watcher->deleteLater();
        });
        const int rate = AudioEngine::instance().sampleRate();
        watcher->setFuture(QtConcurrent::run([path, rate] {
            if (!path.startsWith(":/")) return Decoder::decodeFile(path, rate);
            QFile resource(path); QTemporaryFile temp; temp.setFileTemplate(QDir::tempPath() + "/stemboks-drum-XXXXXX.wav");
            if (!resource.open(QIODevice::ReadOnly) || !temp.open()) return AudioBufferPtr{};
            temp.write(resource.readAll()); temp.flush(); return Decoder::decodeFile(temp.fileName(), rate);
        }));
    }
    if (m_mixPage) updateSecondScreen(); s.save(); r.setSilent("app.settingsDirty", 0); m_applyingSettings = false;
}

void MainWindow::loadTrackToDeck(int index, const QString& path) {
    if (index < 0 || index >= 4 || path.isEmpty()) return;
    auto* deck = AudioEngine::instance().deck(index); auto& registry = ControlRegistry::instance();
    if (deck->playing() && registry.getBool("app.loadLock", true)) {
        QMessageBox::information(this, "Deck locked", QString("Pause deck %1 before loading a track. Change this in Settings.").arg(index + 1)); return;
    }
    const int generation = ++m_loadGeneration[index], rate = AudioEngine::instance().sampleRate();
    const bool preview = registry.getBool(deckKey(index, "load.preview")); registry.setSilent(deckKey(index, "load.preview"), 0);
    if (!preview && registry.getBool(deckKey(index, "preview.active"))) {
        registry.set(deckKey(index, "level.volume"), registry.get(deckKey(index, "preview.previousVolume"), 1));
        registry.set(deckKey(index, "level.pfl"), registry.get(deckKey(index, "preview.previousPfl")));
        registry.setSilent(deckKey(index, "preview.active"), 0);
    }
    registry.setString(deckKey(index, "load.status"), "Loading...");
    struct Result { LoadedTrackPtr track; QString error; };
    auto* watcher = new QFutureWatcher<Result>(this);
    connect(watcher, &QFutureWatcher<Result>::finished, this, [this, watcher, index, generation, preview] {
        const auto result = watcher->result(); watcher->deleteLater(); if (m_loadGeneration[index] != generation) return;
        auto& r = ControlRegistry::instance();
        if (!result.track) { r.setString(deckKey(index, "load.status"), result.error); QMessageBox::warning(this, "Cannot load track", result.error); return; }
        auto* deck = AudioEngine::instance().deck(index);
        if (deck->playing() && r.getBool("app.loadLock", true)) { r.setString(deckKey(index, "load.status"), "Load cancelled: deck is playing"); return; }
        if (const auto previous = deck->track()) Library::instance().saveHotcues(previous->info.path, previous->hotcues, previous->cueSec);
        auto& library = Library::instance(); const auto& track = result.track;
        library.loadHotcues(track->info.path, &track->hotcues, &track->cueSec);
        const qint64 id = library.addOrUpdateTrack(track->info); library.setAnalysis(id, track->analysis.bpm, track->analysis.firstBeatSec, track->analysis.camelot());
        library.setStemsReady(id, track->hasStems); library.recordPlay(id);
        deck->loadTrack(track); if (preview) deck->play(); r.setString(deckKey(index, "load.status"), "Ready"); selectDeck(index);
        if (r.getBool("app.tagRemovePlayed")) { for (const auto& t : library.tagList()) if (t.id == id) { library.toggleTag(id); break; } }
    });
    watcher->setFuture(QtConcurrent::run([path, rate] {
        Result result; auto track = std::make_shared<LoadedTrack>();
        track->audio = Decoder::decodeFile(path, rate, &result.error);
        if (!track->audio || track->audio->isEmpty()) { if (result.error.isEmpty()) result.error = "No audio could be decoded: " + path; return result; }
        track->info = Decoder::readTags(path); track->info.path = path; track->info.durationSec = track->audio->durationSec();
        if (track->info.title.isEmpty()) track->info.title = QFileInfo(path).completeBaseName();
        WaveformData cached; const bool cache = Analysis::loadWaveformCache(path, &cached);
        track->analysis = Analysis::analyze(*track->audio);
        if (cache) track->analysis.waveform = cached; else Analysis::saveWaveformCache(path, track->analysis.waveform);
        if (track->info.bpm > 0) track->analysis.bpm = track->info.bpm;
        track->hasStems = StemManager::loadStems(path, rate, &track->stems); result.track = track; return result;
    }));
}

void MainWindow::saveHotcues() {
    for (int i = 0; i < 4; ++i) if (const auto track = AudioEngine::instance().deck(i)->track()) Library::instance().saveHotcues(track->info.path, track->hotcues, track->cueSec);
}
void MainWindow::updateSecondScreen() {
    const auto& s = AppSettings::instance(); const auto screens = QApplication::screens();
    if ((!s.getInt("[Stembox]", "force_second_screen") && s.getInt("[Stembox]", "screen_usage") != 1) || screens.size() < 2) {
        if (m_second) m_second->hide(); return;
    }
    if (!m_second) m_second = new SecondScreen(AudioEngine::instance().deck(1), this);
    QScreen* target = nullptr; const QString preferred = s.get("[Stembox]", "preferred_screen");
    for (auto* display : screens) if (display != screen()) { if (!target) target = display; if (display->name() == preferred) target = display; }
    if (!target) { m_second->hide(); return; }
    m_second->winId(); m_second->windowHandle()->setScreen(target); m_second->setGeometry(target->availableGeometry()); m_second->show();
}
void MainWindow::closeEvent(QCloseEvent* event) {
    if (QMessageBox::question(this, "Exit Stemboks", "Close Stemboks?", QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) { event->ignore(); return; }
    saveHotcues(); AppSettings::instance().save(); if (m_second) m_second->close(); event->accept();
}
void MainWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) { showMix(); event->accept(); return; }
    if (event->key() >= Qt::Key_F1 && event->key() <= Qt::Key_F4 && m_body->currentWidget() == m_library) {
        m_library->loadSelected(event->key() - Qt::Key_F1); event->accept(); return;
    }
    if (qobject_cast<QLineEdit*>(QApplication::focusWidget())) { QMainWindow::keyPressEvent(event); return; }
    auto& r = ControlRegistry::instance(); const int key = event->key();
    if (key >= Qt::Key_1 && key <= Qt::Key_4) selectDeck(key - Qt::Key_1);
    else if (key == Qt::Key_Space) { if (!event->isAutoRepeat()) r.toggle(deckKey(m_selectedDeck, "transport.playing")); }
    else if (key == Qt::Key_Q || key == Qt::Key_W) { if (!event->isAutoRepeat()) r.toggle(deckKey(key == Qt::Key_Q ? 0 : 1, "transport.playing")); }
    else if (key == Qt::Key_A || key == Qt::Key_S) {
        if (!event->isAutoRepeat()) { const int index = key == Qt::Key_A ? 0 : 1; m_keyboardCue[index] = true; r.set(deckKey(index, "transport.cue"), 1); }
    } else if (key == Qt::Key_M) {
        const bool show = !m_mixerStrip->property("expanded").toBool(); m_mixerStrip->setProperty("expanded", show);
        m_mixerStrip->setVisible(show && m_body->currentWidget() != m_prefs && m_body->currentWidget() != m_stems);
    } else { QMainWindow::keyPressEvent(event); return; }
    event->accept();
}
void MainWindow::dragEnterEvent(QDragEnterEvent* event) { if (event->mimeData()->hasUrls()) event->acceptProposedAction(); }
void MainWindow::dropEvent(QDropEvent* event) {
    const auto urls = event->mimeData()->urls(); if (urls.isEmpty() || !urls.first().isLocalFile()) return;
    loadTrackToDeck(event->position().x() < width() / 2 ? 0 : 1, urls.first().toLocalFile()); event->acceptProposedAction();
}

void MainWindow::keyReleaseEvent(QKeyEvent* event) {
    if ((event->key() == Qt::Key_A || event->key() == Qt::Key_S) && !event->isAutoRepeat()) {
        const int index = event->key() == Qt::Key_A ? 0 : 1;
        if (m_keyboardCue[index]) { m_keyboardCue[index] = false; ControlRegistry::instance().set(deckKey(index, "transport.cue"), 0); event->accept(); return; }
    }
    QMainWindow::keyReleaseEvent(event);
}
