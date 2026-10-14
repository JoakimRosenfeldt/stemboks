#include "SecondScreen.h"
#include "WaveformWidget.h"
#include "UiHelpers.h"
#include "../engine/Deck.h"
#include <QVBoxLayout>
#include <QHBoxLayout>

SecondScreen::SecondScreen(Deck* deck, QWidget* parent) : QWidget(parent, Qt::Window), m_deck(deck) {
    setWindowTitle("Stemboks, second screen"); auto* root = new QVBoxLayout(this);
    m_title = Ui::label("Not Loaded.", "TitleText"); root->addWidget(m_title);
    m_wave = new WaveformWidget(deck); root->addWidget(m_wave, 1);
    auto* info = new QHBoxLayout; m_time = Ui::label("00:00.00", "StemboxTimeBig"); m_bpm = Ui::label("0.0 BPM", "StemboxBpmBig");
    info->addWidget(m_time); info->addStretch(); info->addWidget(m_bpm); root->addLayout(info); resize(1000, 500); refresh();
}
void SecondScreen::refresh() {
    const auto track = m_deck->track(); auto& r = ControlRegistry::instance();
    m_title->setText(track ? track->info.title + " — " + track->info.artist : "Not Loaded.");
    m_time->setText(Ui::time(r.get("app.timeDisplay") == 0 ? m_deck->durationSec() - m_deck->positionSec() : m_deck->positionSec(), true));
    m_bpm->setText(QString("%1 BPM").arg(m_deck->effectiveBpm(), 0, 'f', 1));
    m_wave->setColorMode(static_cast<WaveformWidget::ColorMode>(std::clamp(int(r.get("app.waveformColor", 1)), 0, 2)));
    m_wave->setMarkerCenter(r.getBool("app.playMarkerCenter", true)); m_wave->update();
}
