#include "DeckWidget.h"
#include "WaveformWidget.h"
#include "UiHelpers.h"
#include "../engine/AudioEngine.h"
#include <QApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSlider>
#include <QFileInfo>
#include <QSignalBlocker>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QMouseEvent>
#include "../library/Library.h"

namespace stemboks {

DeckWidget::DeckWidget(Deck* deck, QWidget* parent) : QWidget(parent), m_deck(deck), m_prefix(QString("deck.%1.").arg(deck->index() + 1)) {
    setObjectName("StemboxDeck"); setMinimumWidth(0);
    auto* root = new QVBoxLayout(this); root->setContentsMargins(5, 2, 5, 2); root->setSpacing(2);
    auto* info = new QWidget(this); info->setFixedHeight(44);
    auto* il = new QHBoxLayout(info); il->setContentsMargins(0, 0, 0, 0);
    m_artwork = Ui::label("●", "StemboxDeckCover"); m_artwork->setFixedSize(40, 40); m_artwork->setAlignment(Qt::AlignCenter);
    il->addWidget(m_artwork);
    auto* names = new QVBoxLayout; names->setSpacing(0);
    m_title = Ui::label("Not Loaded.", "TitleText"); m_artist = Ui::label("", "StemboxCaption");
    names->addWidget(m_title); names->addWidget(m_artist); il->addLayout(names, 1);
    const auto infoBox = [&](const QString& caption, QLabel*& value) {
        auto* box = new QWidget; box->setObjectName("StemboxInfoBox"); box->setFixedWidth(caption == "TIME" ? 65 : 55);
        auto* l = new QVBoxLayout(box); l->setContentsMargins(3, 0, 3, 0); l->setSpacing(0);
        l->addWidget(Ui::label(caption, "StemboxCaption")); value = Ui::label("--", "StemboxDeckStatVal"); l->addWidget(value); il->addWidget(box);
    };
    infoBox("BPM", m_bpm); infoBox("KEY", m_key); infoBox("TIME", m_duration); root->addWidget(info);
    auto* main = new QWidget; main->setFixedHeight(68);
    auto* ml = new QHBoxLayout(main); ml->setContentsMargins(0, 0, 0, 0); ml->setSpacing(5);
    m_play = Ui::button(QString("PLAYER %1\nPLAY").arg(deck->index() + 1), "StemboxPlayerBox"); m_play->setFixedWidth(78); m_play->setStyleSheet("font-size:11px;");
    ml->addWidget(m_play); connect(m_play, &QPushButton::clicked, this, [this] { ControlRegistry::instance().toggle(m_prefix + "transport.playing"); });
    m_trackNo = Ui::label("TRACK\n--", "StemboxCaption"); ml->addWidget(m_trackNo);
    auto* transport = new QVBoxLayout; transport->setSpacing(3);
    m_cue = Ui::button("CUE", "DeckCueBtn"); m_sync = Ui::button("SYNC", "DeckSyncBtn"); m_sync->setCheckable(true);
    transport->addWidget(m_cue); transport->addWidget(m_sync); ml->addLayout(transport);
    connect(m_cue, &QPushButton::pressed, this, [this] { ControlRegistry::instance().set(m_prefix + "transport.cue", 1); });
    connect(m_cue, &QPushButton::released, this, [this] { ControlRegistry::instance().set(m_prefix + "transport.cue", 0); });
    connect(m_sync, &QPushButton::toggled, this, [this](bool enabled) { ControlRegistry::instance().set(m_prefix + "sync.enabled", enabled); });
    auto* timeBox = new QVBoxLayout; timeBox->setSpacing(0);
    m_timeCaption = Ui::label("REMAIN", "StemboxCaption"); m_time = Ui::label("00:00.00", "StemboxTimeBig");
    timeBox->addWidget(m_timeCaption); timeBox->addWidget(m_time); ml->addLayout(timeBox, 1);
    auto* tempoBox = new QVBoxLayout; tempoBox->setSpacing(0);
    m_tempoPct = Ui::label("TEMPO 100%", "StemboxCaption"); m_tempoRange = Ui::button("±6", "DeckTempoRange");
    tempoBox->addWidget(m_tempoPct); tempoBox->addWidget(m_tempoRange); ml->addLayout(tempoBox);
    connect(m_tempoRange, &QPushButton::clicked, this, [this] {
        const QVector<int> ranges{6, 10, 16, 100}; auto& r = ControlRegistry::instance();
        r.set(m_prefix + "tempo.range", ranges[(ranges.indexOf(int(r.get(m_prefix + "tempo.range", 6))) + 1) % ranges.size()]);
    });
    auto* tempo = new QSlider(Qt::Vertical); tempo->setRange(-1000, 1000); tempo->setFixedWidth(18); tempo->setToolTip("Tempo fader");
    connect(tempo, &QSlider::valueChanged, this, [this](int v) { ControlRegistry::instance().set(m_prefix + "tempo.fader", v / 1000.0); }); ml->addWidget(tempo);
    m_bpmBig = Ui::label("0.0", "StemboxBpmBig"); ml->addWidget(m_bpmBig); root->addWidget(main);
    m_bpm->installEventFilter(this); m_bpm->setToolTip("Double-click: edit beatgrid");
    m_bpmBig->installEventFilter(this); m_bpmBig->setToolTip("Double-click: edit beatgrid");
    auto* status = new QWidget; status->setFixedHeight(26); auto* sl = new QHBoxLayout(status); sl->setContentsMargins(0, 0, 0, 0); sl->setSpacing(4);
    m_mt = Ui::button("MT", "DeckMtBtn"); m_mt->setCheckable(true);
    m_quantize = Ui::button("QUANTIZE", "DeckQuantBtn"); m_quantize->setCheckable(true);
    sl->addWidget(m_mt); sl->addWidget(m_quantize);
    connect(m_mt, &QPushButton::toggled, this, [this](bool enabled) { ControlRegistry::instance().set(m_prefix + "key.lock", enabled); });
    connect(m_quantize, &QPushButton::toggled, this, [this](bool enabled) { ControlRegistry::instance().set(m_prefix + "grid.quantize", enabled); });
    m_status = Ui::label("Not Loaded.", "StemboxNotLoaded"); sl->addWidget(m_status, 1);
    m_colorFx = Ui::button("FILTER", "DeckColorFxBtn"); sl->addWidget(m_colorFx);
    const QStringList colors{"filter", "space", "decho", "crush", "pitch", "noise"};
    connect(m_colorFx, &QPushButton::clicked, this, [this, colors] {
        auto& r = ControlRegistry::instance(); const auto name = r.getString(m_prefix + "colorfx.name", "filter");
        r.setString(m_prefix + "colorfx.name", colors[(colors.indexOf(name) + 1) % colors.size()]);
        if (!r.getBool("app.colorFxSelectOnly")) r.set(m_prefix + "colorfx.enabled", 1);
    });
    auto* amount = new QSlider(Qt::Horizontal); amount->setRange(-100, 100); amount->setFixedWidth(70); amount->setToolTip("Color FX amount");
    connect(amount, &QSlider::valueChanged, this, [this](int v) { ControlRegistry::instance().set(m_prefix + "colorfx.amount", v / 100.0); });
    sl->addWidget(amount); m_colorFxVal = Ui::label("0%", "StemboxCaption"); sl->addWidget(m_colorFxVal); root->addWidget(status);
    buildPads(); refresh();
}

void DeckWidget::buildPads() {
    auto* root = qobject_cast<QVBoxLayout*>(layout());
    auto* grid = new QGridLayout; grid->setSpacing(3);
    for (int i = 0; i < 8; ++i) {
        auto* pad = Ui::button(QString(QChar('A' + i)), "StemboxPad"); pad->setFixedHeight(38); pad->setMinimumWidth(0);
        m_pads.append(pad); grid->addWidget(pad, i / 4, i % 4);
        connect(pad, &QPushButton::pressed, this, [this, i] { triggerPad(i, false); });
        connect(pad, &QPushButton::released, this, [this, i] { triggerPad(i, true); });
        pad->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(pad, &QPushButton::customContextMenuRequested, this, [this, i] {
            if (m_padMode == 0) Ui::pulse(m_prefix + QString("hotcue.%1.set").arg(i + 1));
            else if (m_padMode == 1 && i == 7) Ui::pulse(m_prefix + "loop.setOut");
        });
    }
    root->addLayout(grid);
    auto* modes = new QHBoxLayout; modes->setSpacing(2);
    const QStringList names{"HOTCUE", "BEATLOOP", "BEATJUMP", "KEYSHIFT", "STEMS", "SAMPLER", "PAD FX"};
    for (int i = 0; i < names.size(); ++i) {
        auto* b = Ui::button(names[i], "StemboxPadMode"); b->setCheckable(true); b->setFixedHeight(22); b->setMinimumWidth(0);
        modes->addWidget(b); m_padModes.append(b);
        connect(b, &QPushButton::toggled, this, [this, i](bool checked) {
            if (checked) { ControlRegistry::instance().set(m_prefix + "pads.mode", i); refresh(); }
        });
    }
    root->addLayout(modes);
}

void DeckWidget::triggerPad(int pad, bool release) {
    auto& r = ControlRegistry::instance(); const auto track = m_deck->track();
    if (m_padMode == 0) {
        const QString base = m_prefix + QString("hotcue.%1.").arg(pad + 1);
        if (release) {
            r.set(base + "activate", 0); return;
        }
        if (QApplication::keyboardModifiers().testFlag(Qt::ShiftModifier) || (track && track->hotcues[pad].positionSec < 0)) Ui::pulse(base + "set");
        else r.set(base + "activate", 1);
    } else if (m_padMode == 1 && !release) {
        const double sizes[] = {0.5, 1, 2, 4, 8, 16, 32};
        if (pad == 7) Ui::pulse(m_prefix + (QApplication::keyboardModifiers().testFlag(Qt::ShiftModifier) || (track && track->loopInSec >= 0 && track->loopOutSec < 0) ? "loop.setOut" : "loop.setIn"));
        else { const bool same = r.getBool(m_prefix + "loop.enabled") && r.get(m_prefix + "loop.size") == sizes[pad]; r.set(m_prefix + "loop.size", sizes[pad]); if (same) r.set(m_prefix + "loop.enabled", 0); else Ui::pulse(m_prefix + "loop.active"); }
    } else if (m_padMode == 2 && !release) {
        r.set(m_prefix + "jump.size", double(4 << (pad / 2))); Ui::pulse(m_prefix + (pad % 2 ? "jump.forward" : "jump.backward"));
    } else if (m_padMode == 3 && !release) {
        const int shifts[] = {-7, -5, -3, -1, 1, 3, 5, 7};
        const int shift = shifts[pad]; r.set(m_prefix + "key.shift", r.get(m_prefix + "key.shift") == shift ? 0 : shift);
    } else if (m_padMode == 4 && !release) {
        const QStringList names{"vocals", "drums", "bass", "other"};
        if (pad < 4) r.toggle(m_prefix + "stems." + names[pad] + ".mute");
        else {
            const int solo = pad - 4; bool alreadySolo = !r.getBool(m_prefix + "stems." + names[solo] + ".mute");
            for (int i = 0; i < 4; ++i) if (i != solo && !r.getBool(m_prefix + "stems." + names[i] + ".mute")) alreadySolo = false;
            for (int i = 0; i < 4; ++i) r.set(m_prefix + "stems." + names[i] + ".mute", !alreadySolo && i != solo);
        }
    } else if (m_padMode == 5) {
        if (!release) r.toggle(QString("sampler.%1.play").arg(pad + 1));
    } else if (m_padMode == 6) {
        auto& s = AppSettings::instance(); const QString unit = QString("fx.unit.%1.").arg(m_deck->index() % 2 + 1);
        const bool toggle = s.getInt("[Stembox]", "padfx_type") == 1;
        if (release) { if (!toggle) r.set(unit + "enabled", 0); return; }
        const auto name = Ui::effectName(s.get("[Stembox]", QString("padfx_name_%1").arg(pad + 1), "ECHO"));
        if (name == "vinylbrake") { Ui::pulse(m_prefix + "transport.brake"); return; }
        r.setString(unit + "name", name); r.setString(m_prefix + "fx.name", name);
        r.set(unit + "parameter", s.getDouble("[Stembox]", QString("padfx_bval_%1").arg(pad + 1), 0.5));
        r.set(unit + "mix", 1); r.set(m_prefix + QString("fx.unit%1").arg(m_deck->index() % 2 + 1), 1);
        r.set(m_prefix + "fx.enabled", 1); if (toggle) r.toggle(unit + "enabled"); else r.set(unit + "enabled", 1);
    }
    refresh();
}

void DeckWidget::refresh() {
    auto& r = ControlRegistry::instance(); const auto track = m_deck->track();
    m_padMode = qBound(0, int(r.get(m_prefix + "pads.mode")), int(m_padModes.size()) - 1);
    const bool remain = r.get("app.timeDisplay") == 0;
    const double duration = m_deck->durationSec();
    const QString title = track ? r.getString(m_prefix + "track.title", track->info.title) : "Not Loaded.";
    m_title->setText(title); m_title->setToolTip(title);
    m_artist->setText(track ? r.getString(m_prefix + "track.artist", track->info.artist) : QString());
    m_duration->setText(Ui::time(duration)); m_timeCaption->setText(remain ? "REMAIN" : "TIME");
    m_time->setText(Ui::time(remain ? duration - m_deck->positionSec() : m_deck->positionSec(), true));
    const double bpm = track ? (track->analysis.bpm > 0 ? track->analysis.bpm : track->info.bpm) : 0;
    m_bpm->setText(QString::number(bpm, 'f', 1)); m_bpmBig->setText(QString::number(m_deck->effectiveBpm(), 'f', 1));
    const auto camelot = track ? track->analysis.camelot() : QString();
    m_key->setText(track ? (r.getBool("app.keyColors") ? camelot : track->analysis.keyName()) : "#");
    const auto other = AudioEngine::instance().deck(m_deck->index() ^ 1)->track();
    const bool match = track && other && Ui::compatible(camelot, other->analysis.camelot());
    QString keyColor = "#e8e8e8";
    if (r.getBool("app.keyColors") && !camelot.isEmpty()) keyColor = QColor::fromHsv((camelot.chopped(1).toInt() * 30) % 360, 170, 255).name();
    else if (match) keyColor = "#66bb6a";
    m_key->setStyleSheet("color:" + keyColor + ";");
    m_tempoPct->setText(QString("TEMPO %1%").arg(r.get(m_prefix + "tempo.ratio", 1) * 100, 0, 'f', 1));
    m_tempoRange->setText(QString("±%1").arg(r.get(m_prefix + "tempo.range", 6)));
    const QString loadStatus = r.getString(m_prefix + "load.status");
    const QString engineStatus = r.getString("app.engineStatus");
    m_status->setText(!loadStatus.isEmpty() && loadStatus != "Ready" ? loadStatus : !engineStatus.isEmpty() ? engineStatus : track ? (track->hasStems ? "STEMS READY" : title) : "Not Loaded.");
    m_status->setStyleSheet(track ? (track->hasStems ? "color:#66bb6a;" : "color:#b8b8b8;") : "color:#ff2d20;");
    m_trackNo->setText(track ? QString("TRACK\n%1").arg(m_deck->index() + 1) : "TRACK\n--");
    m_play->setText(QString("PLAYER %1\n%2").arg(m_deck->index() + 1).arg(m_deck->playing() ? "PAUSE" : "PLAY"));
    {
        const QSignalBlocker syncBlocker(m_sync), mtBlocker(m_mt), quantizeBlocker(m_quantize);
        m_sync->setChecked(r.getBool(m_prefix + "sync.enabled")); m_mt->setChecked(r.getBool(m_prefix + "key.lock"));
        m_quantize->setChecked(r.getBool(m_prefix + "grid.quantize"));
    }
    m_colorFx->setText(r.getString(m_prefix + "colorfx.name", "filter").toUpper());
    m_colorFxVal->setText(QString("%1%").arg(qRound(r.get(m_prefix + "colorfx.amount") * 100)));
    if (track && !track->info.artwork.isNull()) {
        if (m_artwork->property("path").toString() != track->info.path) {
            m_artwork->setPixmap(QPixmap::fromImage(track->info.artwork).scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            m_artwork->setProperty("path", track->info.path);
        }
    } else { m_artwork->setText("●"); m_artwork->setProperty("path", QString()); }
    for (int i = 0; i < m_padModes.size(); ++i) {
        const QSignalBlocker blocker(m_padModes[i]);
        m_padModes[i]->setChecked(i == m_padMode);
    }
    const QStringList stemNames{"vocals", "drums", "bass", "other"};
    const QStringList loopNames{"1/2", "1", "2", "4", "8", "16", "32", "IN / OUT"};
    for (int i = 0; i < 8; ++i) {
        bool set = false; QString text;
        if (m_padMode == 0) {
            text = QString(QChar('A' + i)); set = track && track->hotcues[i].positionSec >= 0;
            if (set) text += "  " + (track->hotcues[i].label.isEmpty() ? Ui::time(track->hotcues[i].positionSec) : track->hotcues[i].label);
        } else if (m_padMode == 1) { text = loopNames[i]; set = r.getBool(m_prefix + "loop.enabled") && i < 7 && r.get(m_prefix + "loop.size") == (i == 0 ? 0.5 : double(1 << (i - 1))); }
        else if (m_padMode == 2) text = QString("%1 %2").arg(i % 2 ? "▶" : "◀").arg(4 << (i / 2));
        else if (m_padMode == 3) { const int shifts[] = {-7,-5,-3,-1,1,3,5,7}; text = QString("%1%2").arg(shifts[i] > 0 ? "+" : "").arg(shifts[i]); set = r.get(m_prefix + "key.shift") == shifts[i]; }
        else if (m_padMode == 4) { const QString stem = stemNames[i % 4]; text = (i >= 4 ? "SOLO " : "") + stem.toUpper(); if (i < 4) text += QString(" %1%").arg(qRound(r.get(m_prefix + "stems." + stem + ".gain", 1) * 100)); set = track && track->hasStems && !r.getBool(m_prefix + "stems." + stem + ".mute"); }
        else if (m_padMode == 5) { const auto path = AppSettings::instance().get("[Stembox]", QString("sampler_%1_path").arg(i + 1)); text = QString("%1 %2").arg(i + 1).arg(path.isEmpty() && i >= 4 ? QStringList{"KICK", "SNARE", "CLAP", "HAT"}[i - 4] : QFileInfo(path).completeBaseName()); set = AudioEngine::instance().samplerPlaying(i); }
        else { auto& s = AppSettings::instance(); text = s.get("[Stembox]", QString("padfx_name_%1").arg(i + 1), "ECHO") + " " + s.get("[Stembox]", QString("padfx_vlabel_%1").arg(i + 1)); }
        m_pads[i]->setText(text); Ui::property(m_pads[i], "set", set);
    }
}

bool DeckWidget::eventFilter(QObject* obj, QEvent* event) {
    if ((obj == m_bpm || obj == m_bpmBig) && event->type() == QEvent::MouseButtonDblClick) {
        openGridEdit();
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

// Beatgrid editor (stembox_gridedit.xml): BPM fine-tune, halve/double,
// shift the grid, or drop the first beat at the playhead. Persists via
// Library::saveGrid (DB + grilles-locales.tsv, like the original).
void DeckWidget::openGridEdit() {
    auto track = m_deck->track();
    if (!track) return;
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Beatgrid — Player %1").arg(m_deck->index() + 1));
    auto* root = new QVBoxLayout(&dlg);
    auto* bpm = new QDoubleSpinBox;
    bpm->setRange(40.0, 250.0); bpm->setDecimals(2); bpm->setSingleStep(0.01);
    bpm->setValue(track->analysis.bpm > 0 ? track->analysis.bpm : track->info.bpm);
    auto* bpmRow = new QHBoxLayout;
    bpmRow->addWidget(Ui::label("BPM", "StemboxPrefsCaption")); bpmRow->addWidget(bpm, 1);
    auto* halve = Ui::button("÷2"); auto* dbl = Ui::button("×2");
    connect(halve, &QPushButton::clicked, bpm, [bpm] { bpm->setValue(bpm->value() / 2); });
    connect(dbl, &QPushButton::clicked, bpm, [bpm] { bpm->setValue(bpm->value() * 2); });
    bpmRow->addWidget(halve); bpmRow->addWidget(dbl);
    root->addLayout(bpmRow);

    auto* firstBeat = new QDoubleSpinBox;
    firstBeat->setRange(-10.0, 60.0); firstBeat->setDecimals(3); firstBeat->setSingleStep(0.001);
    firstBeat->setValue(track->analysis.firstBeatSec);
    auto* fbRow = new QHBoxLayout;
    fbRow->addWidget(Ui::label("First beat (s)", "StemboxPrefsCaption")); fbRow->addWidget(firstBeat, 1);
    root->addLayout(fbRow);

    auto* shiftRow = new QHBoxLayout;
    const auto shiftBtn = [&](const QString& text, double seconds) {
        auto* b = Ui::button(text); shiftRow->addWidget(b);
        connect(b, &QPushButton::clicked, firstBeat, [firstBeat, seconds] { firstBeat->setValue(firstBeat->value() + seconds); });
    };
    shiftBtn("−1 beat", 0); shiftBtn("−10 ms", -0.010); shiftBtn("+10 ms", 0.010); shiftBtn("+1 beat", 0);
    // beat-sized shifts depend on the BPM being edited; recompute on click
    connect(qobject_cast<QPushButton*>(shiftRow->itemAt(0)->widget()), &QPushButton::clicked, firstBeat,
            [firstBeat, bpm] { if (bpm->value() > 0) firstBeat->setValue(firstBeat->value() - 60.0 / bpm->value()); });
    connect(qobject_cast<QPushButton*>(shiftRow->itemAt(3)->widget()), &QPushButton::clicked, firstBeat,
            [firstBeat, bpm] { if (bpm->value() > 0) firstBeat->setValue(firstBeat->value() + 60.0 / bpm->value()); });
    root->addLayout(shiftRow);

    auto* here = Ui::button("Set first beat at playhead");
    connect(here, &QPushButton::clicked, firstBeat, [this, firstBeat, bpm] {
        double pos = m_deck->positionSec();
        if (bpm->value() > 0) { double beat = 60.0 / bpm->value(); pos = std::fmod(pos, beat); }
        firstBeat->setValue(pos);
    });
    root->addWidget(here);

    auto* buttons = new QHBoxLayout; buttons->addStretch();
    auto* cancel = Ui::button("Cancel"); auto* ok = Ui::button("Save grid");
    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    buttons->addWidget(cancel); buttons->addWidget(ok);
    root->addLayout(buttons);

    if (dlg.exec() != QDialog::Accepted) return;
    m_deck->applyBeatgrid(bpm->value(), firstBeat->value());
    ControlRegistry::instance().set(m_prefix + "grid.changed", 1);
}

} // namespace stemboks
