#include "ControlRegistry.h"
#include <QMutexLocker>

ControlRegistry& ControlRegistry::instance() {
    static ControlRegistry inst;
    return inst;
}

double ControlRegistry::get(const QString& name, double def) const {
    QMutexLocker lock(&m_mutex);
    return m_values.value(name, def);
}

void ControlRegistry::set(const QString& name, double value) {
    {
        QMutexLocker lock(&m_mutex);
        if (m_values.value(name, 0.0) == value && m_values.contains(name)) return;
        m_values[name] = value;
    }
    emit changed(name, value);
}

void ControlRegistry::setSilent(const QString& name, double value) {
    QMutexLocker lock(&m_mutex);
    m_values[name] = value;
}

QString ControlRegistry::getString(const QString& name, const QString& def) const {
    QMutexLocker lock(&m_mutex);
    return m_strings.value(name, def);
}

void ControlRegistry::setString(const QString& name, const QString& value) {
    {
        QMutexLocker lock(&m_mutex);
        if (m_strings.value(name) == value) return;
        m_strings[name] = value;
    }
    emit stringChanged(name, value);
}
