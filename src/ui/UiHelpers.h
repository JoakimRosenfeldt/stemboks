#pragma once
#include "../control/AppSettings.h"
#include "../control/ControlRegistry.h"
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <algorithm>
#include <cmath>

namespace Ui {
inline QString time(double seconds, bool hundredths = false) {
    const qint64 ticks = qint64(std::max(0.0, seconds) * 100);
    QString text = QStringLiteral("%1:%2").arg(ticks / 6000, 2, 10, QLatin1Char('0'))
        .arg((ticks / 100) % 60, 2, 10, QLatin1Char('0'));
    if (hundredths) text += QStringLiteral(".%1").arg(ticks % 100, 2, 10, QLatin1Char('0'));
    return text;
}
inline QLabel* label(const QString& text, const char* name, QWidget* parent = nullptr) {
    auto* l = new QLabel(text, parent); l->setObjectName(QString::fromLatin1(name));
    l->setMinimumWidth(0); return l;
}
inline QPushButton* button(const QString& text, const char* name = "", QWidget* parent = nullptr) {
    auto* b = new QPushButton(text, parent); b->setObjectName(QString::fromLatin1(name));
    b->setFocusPolicy(Qt::NoFocus); return b;
}
inline void property(QWidget* widget, const char* name, bool value) {
    if (widget->property(name).isValid() && widget->property(name).toBool() == value) return;
    widget->setProperty(name, value); widget->style()->unpolish(widget); widget->style()->polish(widget);
}
inline void setting(const QString& group, const QString& key, const QString& value) {
    auto& s = AppSettings::instance(); s.set(group, key, value); s.save();
    ControlRegistry::instance().set("app.settingsDirty", 1);
}
inline void setting(const QString& group, const QString& key, int value) {
    setting(group, key, QString::number(value));
}
inline bool compatible(const QString& a, const QString& b) {
    if (a.isEmpty() || b.isEmpty()) return false;
    bool okA = false, okB = false;
    const int n = a.chopped(1).toInt(&okA), m = b.chopped(1).toInt(&okB);
    if (!okA || !okB || n < 1 || n > 12 || m < 1 || m > 12) return false;
    return n == m || (a.back() == b.back() && (std::abs(n - m) == 1 || std::abs(n - m) == 11));
}
inline QString effectName(QString name) {
    return name.toLower().remove(' ').remove('-');
}
inline void pulse(const QString& key) {
    auto& r = ControlRegistry::instance(); r.set(key, 0); r.set(key, 1);
}
}
