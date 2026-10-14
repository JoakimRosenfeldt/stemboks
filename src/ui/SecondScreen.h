#pragma once
#include <QWidget>

class Deck; class WaveformWidget; class QLabel;

// Second-display CDJ screen (one deck, big waveform + track info),
// shown when Display >Screen usage is "2 SCREENS" / force_second_screen.
class SecondScreen : public QWidget {
    Q_OBJECT
public:
    explicit SecondScreen(Deck* deck, QWidget* parent = nullptr);
    void refresh();
private:
    Deck* m_deck; WaveformWidget* m_wave; QLabel *m_title, *m_time, *m_bpm;
};
