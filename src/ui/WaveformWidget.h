#pragma once
#include "../engine/Types.h"
#include <QWidget>

// Large scrolling waveform (BLUE / RGB / 3BAND), beatgrid ticks, hotcue markers,
// loop region, play marker at LEFT or CENTER, track-end warning blink.
class Deck;
class WaveformWidget : public QWidget {
    Q_OBJECT
public:
    explicit WaveformWidget(Deck* deck, QWidget* parent = nullptr);
    enum ColorMode { Blue = 0, Rgb = 1, ThreeBand = 2 };
    void setColorMode(ColorMode m) { m_colorMode = m; update(); }
    void setMarkerCenter(bool c) { m_markerCenter = c; }
    void setOverview(bool o) { m_overview = o; }   // whole-track strip
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
private:
    Deck* m_deck;
    ColorMode m_colorMode = Rgb;
    bool m_markerCenter = true, m_overview = false, m_dragging = false;
    double m_zoomSec = 12.0; // visible seconds in scrolling mode
};
