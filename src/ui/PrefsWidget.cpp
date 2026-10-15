#include "PrefsWidget.h"
#include "UiHelpers.h"
#include "../engine/AudioEngine.h"
#include "../engine/Effects.h"
#include <QApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QScrollArea>
#include <QSlider>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTimer>
#include <QScreen>
#include <QStandardPaths>
#include <functional>

namespace stemboks {

namespace {
QWidget* makeSegment(const QStringList& labels, int current, const std::function<void(int)>& callback) {
    auto* widget = new QWidget; auto* layout = new QHBoxLayout(widget); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(2);
    auto* group = new QButtonGroup(widget); group->setExclusive(true);
    for (int i = 0; i < labels.size(); ++i) {
        auto* button = Ui::button(labels[i]); button->setProperty("segment", true); button->setCheckable(true);
        button->setChecked(i == current); group->addButton(button, i); layout->addWidget(button);
    }
    QObject::connect(group, &QButtonGroup::idClicked, widget, callback); layout->addStretch(); return widget;
}
QFormLayout* form(QWidget* page) {
    auto* layout = new QFormLayout(page); layout->setContentsMargins(24, 20, 24, 20); layout->setSpacing(14);
    layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow); return layout;
}
void segment(QFormLayout* layout, const QString& caption, const QString& group, const QString& key, const QStringList& labels, int def = 0, QVector<int> values = {}) {
    const int value = AppSettings::instance().getInt(group, key, def);
    const int index = values.isEmpty() ? value : values.indexOf(value);
    layout->addRow(caption, makeSegment(labels, std::max(0, index), [group, key, values](int i) {
        Ui::setting(group, key, values.isEmpty() ? i : values[i]);
        if (key == "time_display") Ui::setting("[Controls]", "ShowDurationRemaining", i == 0 ? 1 : 0);
    }));
}
QSlider* slider(QFormLayout* layout, const QString& caption, const QString& group, const QString& key, int min, int max, int def) {
    auto* row = new QWidget; auto* hl = new QHBoxLayout(row); hl->setContentsMargins(0, 0, 0, 0);
    auto* s = new QSlider(Qt::Horizontal); s->setRange(min, max); s->setValue(AppSettings::instance().getInt(group, key, def));
    auto* value = Ui::label(QString::number(s->value()), "StemboxPrefsCaption"); value->setMinimumWidth(38);
    hl->addWidget(s, 1); hl->addWidget(value);
    QObject::connect(s, &QSlider::valueChanged, s, [group, key, value](int v) { value->setText(QString::number(v)); Ui::setting(group, key, v); });
    layout->addRow(caption, row); return s;
}
QComboBox* combo(QFormLayout* layout, const QString& caption, const QString& group, const QString& key, const QStringList& options, const QString& def = {}) {
    auto* c = new QComboBox; c->addItems(options); c->setCurrentText(AppSettings::instance().get(group, key, def));
    QObject::connect(c, &QComboBox::textActivated, c, [group, key](const QString& value) { Ui::setting(group, key, value); });
    layout->addRow(caption, c); return c;
}
void directory(QFormLayout* layout, const QString& caption, const QString& group, const QString& key, const QString& def = {}) {
    auto* b = Ui::button(AppSettings::instance().get(group, key, def));
    QObject::connect(b, &QPushButton::clicked, b, [b, group, key] {
        const auto path = QFileDialog::getExistingDirectory(b, "Choose folder", b->text());
        if (!path.isEmpty()) { Ui::setting(group, key, path); b->setText(path); }
    }); layout->addRow(caption, b);
}
}

PrefsWidget::PrefsWidget(QWidget* parent) : QWidget(parent) {
    setObjectName("StemboxPrefs"); auto* root = new QHBoxLayout(this); root->setContentsMargins(0, 0, 0, 0);
    auto* left = new QVBoxLayout; auto* back = Ui::button("‹ MIX"); left->addWidget(back); connect(back, &QPushButton::clicked, this, &PrefsWidget::closed);
    m_sidebar = new QListWidget; m_sidebar->setObjectName("StemboxPrefsSidebar"); m_sidebar->setFixedWidth(185);
    m_sidebar->addItems({"Stemboks", "Controls", "Pads", "Mixer", "Recording", "Display", "Settings", "About"}); left->addWidget(m_sidebar); root->addLayout(left);
    m_stack = new QStackedWidget; root->addWidget(m_stack, 1);
    for (auto* page : {pageStembox(), pageControls(), pagePads(), pageMixer(), pageRecording(), pageDisplay(), pageSettings(), pageAbout()}) {
        auto* scroll = new QScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame); scroll->setWidget(page); m_stack->addWidget(scroll);
    }
    connect(m_sidebar, &QListWidget::currentRowChanged, m_stack, &QStackedWidget::setCurrentIndex); m_sidebar->setCurrentRow(0);
}

QWidget* PrefsWidget::pageStembox() {
    auto* p = new QWidget; auto* f = form(p); const QString g = "[Stembox]";
    segment(f, "Mix view", g, "mix_mode", {"LARGE", "SPLIT"});
    segment(f, "Preview style", g, "preview_style", {"WAVEFORM", "GRID"});
    segment(f, "Waveform color", g, "waveform_color", {"BLUE", "RGB", "3BAND"}, 1);
    segment(f, "End of track warning", g, "track_end_warning", {"20 S", "30 S", "40 S"}, 30, {20, 30, 40});
    segment(f, "Play marker", g, "play_marker_position", {"LEFT", "CENTER"}, 1);
    segment(f, "Track time display", g, "time_display", {"REMAIN", "TIME"}, AppSettings::instance().getInt("[Controls]", "ShowDurationRemaining", 1) ? 0 : 1);
    segment(f, "Theme", g, "theme", {"DARK", "LIGHT"});
    segment(f, "Key colors", g, "key_colors", {"MATCHING", "CAMELOT"});
    segment(f, "Hotcue pads", g, "hotcue_mode", {"PLAY", "HOLD"});
    slider(f, "Backspin, LONG to SHORT", g, "backspin", 1, 8, 4); return p;
}

QWidget* PrefsWidget::pageControls() {
    auto* p = new QWidget; auto* f = form(p); const QString g = "[Stembox]";
    auto* controllers = Ui::label("", "StemboxAboutLine"); controllers->setWordWrap(true);
    const auto updateControllers = [controllers] {
        const auto names = AudioEngine::instance().connectedControllers();
        controllers->setText(names.isEmpty() ? "No controller connected" : names.join('\n'));
    };
    updateControllers();
    connect(&AudioEngine::instance(), &AudioEngine::devicesChanged, p, updateControllers);
    f->addRow("Controllers", controllers);
    auto* configure = Ui::button("Configure sound and controllers");
    connect(configure, &QPushButton::clicked, p, [p] { AudioEngine::instance().showPreferences(p); });
    f->addRow(configure);
    auto* caption = Ui::label("Choose audio devices, output channels, latency and controller mappings in Mixxx preferences.", "StemboxPrefsCaption");
    caption->setWordWrap(true);
    f->addRow(caption);
    auto* rescan = Ui::button("Rescan controllers");
    connect(rescan, &QPushButton::clicked, p, [] { AudioEngine::instance().rescanControllers(); });
    f->addRow(rescan);
    return p;
}

QWidget* PrefsWidget::pagePads() {
    auto* page = new QWidget; auto* root = new QVBoxLayout(page); auto* tabs = new QTabWidget; root->addWidget(tabs);
    auto* fx = new QWidget; auto* f = form(fx);
    QStringList effects = FxNames::beatFx(); effects << "vinylbrake"; effects.removeDuplicates();
    const QStringList labels{"1/16", "1/8", "1/4", "1/2", "1", "2", "4"}; const QVector<double> values{.0625,.125,.25,.5,1,2,4};
    for (int i = 1; i <= 8; ++i) {
        auto* row = new QWidget; auto* l = new QHBoxLayout(row); l->setContentsMargins(0, 0, 0, 0);
        auto* name = new QComboBox; for (const auto& effect : effects) name->addItem(effect == "vinylbrake" ? "VINYL BRAKE" : effect == "enigmajet" ? "ENIGMA JET" : effect.toUpper(), effect);
        const QString key = QString("padfx_name_%1").arg(i); const QString configured = Ui::effectName(AppSettings::instance().get("[Stembox]", key, "ECHO"));
        name->setCurrentIndex(std::max(0, name->findData(configured))); l->addWidget(name, 1);
        connect(name, &QComboBox::textActivated, row, [key, i](const QString& value) {
            Ui::setting("[Stembox]", key, value);
            Ui::setting("[Stembox]", QString("padfx_bparam_%1").arg(i), 1);
        });
        auto* prev = Ui::button("◀"); auto* value = Ui::label(AppSettings::instance().get("[Stembox]", QString("padfx_vlabel_%1").arg(i), "1/2"), "StemboxAboutLine"); auto* next = Ui::button("▶");
        l->addWidget(prev); l->addWidget(value); l->addWidget(next);
        const auto change = [i, values, labels, value](int delta) {
            const double current = AppSettings::instance().getDouble("[Stembox]", QString("padfx_bval_%1").arg(i), .5);
            int index = values.indexOf(current); if (index < 0) index = 3; index = (index + delta + values.size()) % values.size();
            Ui::setting("[Stembox]", QString("padfx_bval_%1").arg(i), QString::number(values[index]));
            Ui::setting("[Stembox]", QString("padfx_vlabel_%1").arg(i), labels[index]); value->setText(labels[index]);
        };
        connect(prev, &QPushButton::clicked, row, [change] { change(-1); }); connect(next, &QPushButton::clicked, row, [change] { change(1); });
        auto* release = Ui::button("RELEASE FX"); release->setCheckable(true); release->setChecked(AppSettings::instance().getInt("[Stembox]", QString("padfx_releasefx_%1").arg(i)));
        l->addWidget(release); connect(release, &QPushButton::clicked, row, [i](bool on) { Ui::setting("[Stembox]", QString("padfx_releasefx_%1").arg(i), on ? 1 : 0); });
        f->addRow(QString("PAD %1").arg(i), row);
    }
    segment(f, "FX type", "[Stembox]", "padfx_type", {"HOLD", "TOGGLE"});
    slider(f, "Release duration, seconds", "[Stembox]", "padfx_release_s", 1, 8, 3); tabs->addTab(fx, "PAD FX");
    auto* sampler = new QWidget; auto* sf = form(sampler);
    for (int i = 1; i <= 8; ++i) {
        const QString key = QString("sampler_%1_path").arg(i); auto* pick = Ui::button(AppSettings::instance().get("[Stembox]", key, "Choose audio file"));
        connect(pick, &QPushButton::clicked, sampler, [pick, key] {
            const QString path = QFileDialog::getOpenFileName(pick, "Load sampler sound", {}, "Audio (*.wav *.mp3 *.flac *.aif *.aiff *.m4a *.ogg)");
            if (!path.isEmpty()) { Ui::setting("[Stembox]", key, path); pick->setText(path); }
        }); sf->addRow(QString("SLOT %1").arg(i), pick);
    } tabs->addTab(sampler, "SAMPLER");
    auto* drums = new QWidget; auto* df = form(drums);
    combo(df, "Kit", "[Stembox]", "drum_kit", {"LATIN HOUSE", "TECH HOUSE", "UK GARAGE"}, "LATIN HOUSE");
    df->addRow(Ui::label("Bundled drums use sampler slots 5 to 8.", "StemboxPrefsCaption"));
    auto* pads = new QWidget; auto* pl = new QHBoxLayout(pads);
    const QStringList drumNames{"KICK", "SNARE", "CLAP", "HAT"};
    for (int i = 0; i < 4; ++i) { auto* b = Ui::button(drumNames[i]); pl->addWidget(b); connect(b, &QPushButton::pressed, drums, [i] { Ui::pulse(QString("sampler.%1.play").arg(i + 5)); }); }
    df->addRow(pads); tabs->addTab(drums, "DRUM STATION"); return page;
}

QWidget* PrefsWidget::pageMixer() {
    auto* p = new QWidget; auto* f = form(p); const QString g = "[Mixer]";
    segment(f, "Crossfader", g, "xfader_disable", {"ON", "DISABLED"});
    segment(f, "Crossfader reverse", g, "xfader_reverse", {"NORMAL", "HAMMER STYLE"});
    segment(f, "Tempo fader", "[Controls]", "RateDir", {"NORMAL", "INVERTED"});
    segment(f, "Crossfader mode", g, "xfader_mode", {"CONSTANT POWER", "ADDITIVE"}, 1);
    segment(f, "Crossfader curve", g, "xfader_curve", {"LINEAR", "MEDIUM", "SCRATCH"}, 1);
    slider(f, "EQ sensitivity", g, "eq_sensitivity", 25, 200, 100);
    auto* eq = new QWidget; auto* el = new QHBoxLayout(eq);
    const QStringList bands{"45 Hz", "90 Hz", "180 Hz", "360 Hz", "720 Hz", "1.5 kHz", "6 kHz", "12 kHz"};
    for (int i = 0; i < 8; ++i) {
        auto* col = new QVBoxLayout; auto* s = new QSlider(Qt::Vertical); s->setRange(-120, 120); s->setValue(qRound(AppSettings::instance().getDouble(g, QString("eq_gain_%1").arg(i + 1)) * 10));
        s->setMinimumHeight(130); auto* value = Ui::label(QString::number(s->value() / 10.0, 'f', 1), "StemboxPrefsCaption");
        col->addWidget(value); col->addWidget(s, 1, Qt::AlignHCenter); col->addWidget(Ui::label(bands[i], "StemboxPrefsCaption")); el->addLayout(col);
        connect(s, &QSlider::valueChanged, p, [i, value, g](int v) { value->setText(QString::number(v / 10.0, 'f', 1)); Ui::setting(g, QString("eq_gain_%1").arg(i + 1), QString::number(v / 10.0)); });
    }
    f->addRow("Master EQ", eq); segment(f, "EQ curve", g, "eq_curve", {"ISO", "EQ"}, 1);
    segment(f, "Auto gain", g, "auto_gain", {"OFF", "ON"}, 1); segment(f, "Master limiter", g, "master_limiter", {"OFF", "ON"}, 1); return p;
}

QWidget* PrefsWidget::pageRecording() {
    auto* p = new QWidget; auto* f = form(p); const QString g = "[Recording]";
    auto* record = Ui::button("START RECORDING", "StemboxRecordBtn"); record->setCheckable(true); f->addRow(record);
    auto* duration = Ui::label("00:00", "StemboxAboutLine"); f->addRow("Recorded", duration);
    const QString def = QStandardPaths::writableLocation(QStandardPaths::MusicLocation) + "/Stemboks/Recordings";
    directory(f, "Directory", g, "Directory", def); segment(f, "Format", g, "Format", {"WAV", "MP3"}); segment(f, "Bit depth", g, "BitDepth", {"16", "24", "32 FLOAT"});
    connect(record, &QPushButton::clicked, p, [record, duration, def](bool on) {
        auto& engine = AudioEngine::instance(); auto& s = AppSettings::instance();
        const QString dir = s.get("[Recording]", "Directory", def); const bool mp3 = s.getInt("[Recording]", "Format") == 1;
        if (on && !QDir().mkpath(dir)) { record->setChecked(false); duration->setText("Cannot create recording folder"); return; }
        const QString path = dir + "/Stemboks-" + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz") + (mp3 ? ".mp3" : ".wav");
        const int depth = QVector<int>{16,24,32}[std::clamp(s.getInt("[Recording]", "BitDepth"), 0, 2)];
        engine.setRecording(on, path, mp3, depth); record->setChecked(engine.recording());
        if (on && !engine.recording()) duration->setText("Recording could not start");
    });
    auto* timer = new QTimer(p); timer->setInterval(500);
    connect(timer, &QTimer::timeout, p, [record, duration] {
        auto& engine = AudioEngine::instance(); record->setChecked(engine.recording()); record->setText(engine.recording() ? "STOP RECORDING" : "START RECORDING");
        if (engine.recording()) duration->setText(Ui::time(engine.recordedSeconds()));
    }); timer->start(); return p;
}

QWidget* PrefsWidget::pageDisplay() {
    auto* p = new QWidget; auto* f = form(p); const QString g = "[Stembox]";
    segment(f, "Force second screen", g, "force_second_screen", {"OFF", "ON"});
    segment(f, "Frame rate", g, "frame_rate", {"30 FPS", "60 FPS"}, 60, {30,60});
    auto* actions = new QWidget; auto* al = new QHBoxLayout(actions);
    auto* minimize = Ui::button("Minimize"); auto* fullscreen = Ui::button("Full screen"); al->addWidget(minimize); al->addWidget(fullscreen);
    connect(minimize, &QPushButton::clicked, this, [this] { window()->showMinimized(); });
    connect(fullscreen, &QPushButton::clicked, this, [this] { window()->isFullScreen() ? window()->showNormal() : window()->showFullScreen(); }); f->addRow(actions);
    slider(f, "Brightness, %", g, "brightness", 20, 150, 100); slider(f, "Contrast, %", g, "contrast", 20, 150, 100);
    QStringList screens; for (auto* screen : QApplication::screens()) screens.append(screen->name());
    combo(f, "Preferred screen", g, "preferred_screen", screens, screens.value(0));
    segment(f, "Screen usage", g, "screen_usage", {"1 SCREEN", "2 SCREENS"});
    auto* adapt = Ui::button("Readapt to this screen"); f->addRow(adapt);
    connect(adapt, &QPushButton::clicked, this, [this] { if (auto* screen = window()->screen()) window()->resize(screen->availableGeometry().size() * 0.9); });
    segment(f, "Interface size", g, "interface_size", {"60%", "80%", "100%", "125%", "140%"}, 100, {60,80,100,125,140}); return p;
}

QWidget* PrefsWidget::pageSettings() {
    auto* p = new QWidget; auto* f = form(p); const QString g = "[Stembox]";
    segment(f, "Load on a playing deck", g, "load_lock", {"UNLOCKED", "LOCKED"}, 1);
    segment(f, "Vinyl start/stop sound", g, "vinyl_start_stop", {"OFF", "ON"});
    segment(f, "Touch waveform", g, "touch_waveform", {"NAVIGATE", "TOUCH CUE"});
    segment(f, "Search keyboard", g, "keyboard_azerty", {"QWERTY", "AZERTY"});
    segment(f, "Multi metadata in split view", g, "multi_meta", {"OFF", "ON"});
    segment(f, "Remove from tag list once played", g, "tag_remove_played", {"OFF", "ON"});
    segment(f, "Color FX buttons", g, "colorfx_select_only", {"ON / OFF", "SELECT ONLY"}); return p;
}

QWidget* PrefsWidget::pageAbout() {
    auto* p = new QWidget; auto* f = form(p);
    f->addRow(Ui::label("STEMBOKS 1.8.9", "StemboxAboutCompany"));
    f->addRow(Ui::label("Clean-room clone of the Stembox DJ interface.", "StemboxAboutLine"));
    auto* note = Ui::label("Independent implementation. No affiliation with Mixystore or Stembox.", "StemboxAboutMuted"); note->setWordWrap(true); f->addRow(note);
    auto* license = Ui::label("GNU GPL version 3. You may use, modify and redistribute this software under the included LICENSE. No warranty.", "StemboxAboutLine"); license->setWordWrap(true); f->addRow(license);
    auto* libs = Ui::label("Mixxx 2.6 engine / Qt 6 / SQLite\nPortAudio / PortMidi / hidapi\nSoundTouch / Rubber Band / FFmpeg\nONNX Runtime, optional stem separation", "StemboxAboutSmall"); f->addRow("Libraries", libs); return p;
}

} // namespace stemboks
