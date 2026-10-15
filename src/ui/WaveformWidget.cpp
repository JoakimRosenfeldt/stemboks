#include "WaveformWidget.h"
#include "../engine/Deck.h"
#include "UiHelpers.h"
#include <QPainter>
#include <QMouseEvent>
#include <QDateTime>
#include <QSvgRenderer>
#include <cmath>

namespace stemboks {

WaveformWidget::WaveformWidget(Deck* deck, QWidget* parent) : QWidget(parent), m_deck(deck) {
    setMinimumSize(0, 64); setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setCursor(Qt::PointingHandCursor);
}

void WaveformWidget::paintEvent(QPaintEvent*) {
    QPainter p(this); p.fillRect(rect(), Qt::black);
    const auto track = m_deck->track();
    if (!track) {
        QSvgRenderer logo(QStringLiteral(":/icons/stemboks_logo.svg"));
        const double w = std::min(width() * 0.5, 340.0), h = w * 136 / 340;
        logo.render(&p, QRectF((width() - w) / 2, (height() - h) / 2, w, h)); return;
    }
    const double duration = m_deck->durationSec(), position = m_deck->positionSec();
    const double window = m_overview ? std::max(duration, 0.01) : m_zoomSec;
    const double marker = m_markerCenter ? 0.5 : 0.25;
    const double start = m_overview ? 0 : position - window * marker;
    const double scale = width() / window;
    const auto xAt = [&](double sec) { return (sec - start) * scale; };
    const auto& wave = track->analysis.waveform;
    const double center = height() / 2.0, amplitude = std::max(1.0, center - 22);
    if (track->loopInSec >= 0 && track->loopOutSec > track->loopInSec) {
        p.fillRect(QRectF(xAt(track->loopInSec), 0, (track->loopOutSec - track->loopInSec) * scale, height()),
                   QColor(40, 200, 80, ControlRegistry::instance().getBool(deckKey(m_deck->index(), "loop.enabled")) ? 65 : 25));
    }
    p.setPen(QColor(32, 32, 32)); p.drawLine(0, int(center), width(), int(center));
    for (int x = 0; x < width(); ++x) {
        const int first = int(std::floor((start + x / scale) * 100));
        const int last = int(std::ceil((start + (x + 1) / scale) * 100));
        WaveformData::Peak peak;
        for (int i = std::max(0, first); i < std::min(int(wave.peaks.size()), std::max(first + 1, last)); ++i) {
            peak.low = std::max(peak.low, wave.peaks[i].low);
            peak.mid = std::max(peak.mid, wave.peaks[i].mid);
            peak.high = std::max(peak.high, wave.peaks[i].high);
        }
        const float low = std::clamp(peak.low, 0.f, 1.f), mid = std::clamp(peak.mid, 0.f, 1.f), high = std::clamp(peak.high, 0.f, 1.f);
        if (m_colorMode == ThreeBand) {
            const QColor colors[] = {QColor("#054ce3"), QColor("#de7002"), QColor("#faf4e8")};
            const float levels[] = {low, mid, high};
            for (int band = 0; band < 3; ++band) {
                const double y = (band + 0.5) * height() / 3.0;
                p.setPen(colors[band]); p.drawLine(QPointF(x, y - levels[band] * height() / 6), QPointF(x, y + levels[band] * height() / 6));
            }
        } else {
            const float peakLevel = std::max({low, mid, high});
            QColor color("#29b6f6");
            if (m_colorMode == Rgb && peakLevel > 0) color = QColor::fromRgbF(low / peakLevel, (mid / peakLevel) * 0.55f, high / peakLevel);
            p.setPen(color); p.drawLine(QPointF(x, center - peakLevel * amplitude), QPointF(x, center + peakLevel * amplitude));
        }
    }
    const double bpm = track->analysis.bpm > 0 ? track->analysis.bpm : track->info.bpm;
    if (bpm > 0) {
        const double beat = 60 / bpm, offset = track->analysis.firstBeatSec;
        const qint64 first = qint64(std::ceil((std::max(0.0, start) - offset) / beat));
        for (qint64 n = first; offset + n * beat < std::min(duration, start + window); ++n) {
            const double x = xAt(offset + n * beat);
            p.setPen(QColor(255, 255, 255, n % 4 == 0 ? 110 : 40));
            p.drawLine(QPointF(x, 18), QPointF(x, height()));
            if (n % 4 == 0 && !m_overview) p.drawText(QPointF(x + 3, height() - 5), QString::number(n / 4 + 1));
        }
    }
    const QColor colors[] = {QColor("#ef5350"), QColor("#ff9800"), QColor("#ffc107"), QColor("#66bb6a"), QColor("#26c6da"), QColor("#42a5f5"), QColor("#ab47bc"), QColor("#ec407a")};
    for (int i = 0; i < kNumHotcues; ++i) {
        if (track->hotcues[i].positionSec < 0) continue;
        const double x = xAt(track->hotcues[i].positionSec);
        if (x < 0 || x > width()) continue;
        p.setPen(colors[i]); p.drawLine(QPointF(x, 0), QPointF(x, height()));
        p.fillRect(QRectF(x, 0, 22, 18), colors[i]); p.setPen(Qt::black);
        p.drawText(QRectF(x, 0, 22, 18), Qt::AlignCenter, QString(QChar('A' + i)));
    }
    p.setPen(QPen(QColor("#ff9800"), 2));
    p.drawLine(QPointF(xAt(position), 0), QPointF(xAt(position), height()));
    if (!m_overview && m_deck->playing() && duration - position < ControlRegistry::instance().get("app.trackEndWarning", 30)
        && (QDateTime::currentMSecsSinceEpoch() / 400) % 2 == 0) {
        p.setPen(QPen(QColor("#ff2d20"), 3)); p.drawRect(rect().adjusted(1, 1, -2, -2));
    }
}

void WaveformWidget::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton || !m_deck->hasTrack()) return;
    m_dragging = true; m_dragStartX = e->position().x(); m_dragStartSec = m_deck->positionSec();
    const bool touch = ControlRegistry::instance().getBool("app.touchWaveform");
    if (touch && !m_overview) ControlRegistry::instance().set(deckKey(m_deck->index(), "jog.touch"), 1);
    const double duration = m_deck->durationSec();
    const double pos = m_overview ? duration * e->position().x() / std::max(1, width())
        : m_dragStartSec + (e->position().x() / std::max(1, width()) - (m_markerCenter ? 0.5 : 0.25)) * m_zoomSec;
    if (touch && !m_overview) m_deck->scratchTo(std::clamp(pos, 0.0, duration));
    else m_deck->seekSec(std::clamp(pos, 0.0, duration));
    m_dragStartSec = m_deck->positionSec(); update();
}
void WaveformWidget::mouseMoveEvent(QMouseEvent* e) {
    if (!m_dragging) return;
    const bool touch = ControlRegistry::instance().getBool("app.touchWaveform");
    const double pos = m_overview ? m_deck->durationSec() * e->position().x() / std::max(1, width())
        : m_dragStartSec - (e->position().x() - m_dragStartX) * m_zoomSec / std::max(1, width());
    if (touch && !m_overview) m_deck->scratchTo(std::clamp(pos, 0.0, m_deck->durationSec()));
    else m_deck->seekSec(std::clamp(pos, 0.0, m_deck->durationSec()));
    update();
}
void WaveformWidget::mouseReleaseEvent(QMouseEvent*) {
    m_dragging = false; ControlRegistry::instance().set(deckKey(m_deck->index(), "jog.touch"), 0);
}

} // namespace stemboks
