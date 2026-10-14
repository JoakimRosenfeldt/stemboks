#pragma once
#include <QWidget>

class Deck;
class QLabel;
class QPushButton;

// CDJ-style deck bar as in Stembox's stembox_cdj_deck.xml + stembox_deck_bottom.xml:
// top strip (artwork disc, BPM/KEY/TIME boxes, info), main row (PLAYER n, TRACK --,
// CUE/SYNC, REMAIN/TIME big digits, TEMPO ±range, BPM readout), status row
// (MT, QUANTIZE, "Not Loaded."/title, COLOR FX selector), 8 pads A-H with pad modes
// (HOTCUE, BEATLOOP, BEATJUMP, KEYSHIFT, STEMS, SAMPLER, PAD FX).
class DeckWidget : public QWidget {
    Q_OBJECT
public:
    explicit DeckWidget(Deck* deck, QWidget* parent = nullptr);
    void refresh(); // called by UI timer
private:
    Deck* m_deck;
    QLabel *m_artist, *m_artwork, *m_duration, *m_timeCaption;
    QLabel *m_title, *m_time, *m_bpm, *m_key, *m_tempoPct, *m_bpmBig, *m_status, *m_trackNo, *m_colorFxVal;
    QPushButton* m_colorFx;
    QPushButton *m_cue, *m_sync, *m_mt, *m_quantize, *m_tempoRange, *m_play;
    QVector<QPushButton*> m_pads;
    QVector<QPushButton*> m_padModes;
    int m_padMode = 0;
    QString m_prefix;
    void buildPads();
    void triggerPad(int pad, bool release);
};
