#pragma once
#include <QObject>
#include <QHash>
#include <QString>
#include <QMutex>

// Central dotted-name control registry, mirroring Stembox's CH-03 design:
// UI, controllers and the audio engine communicate only through here.
// Names look like "deck.1.transport.playing", "mixer.crossfader.position".
class ControlRegistry : public QObject {
    Q_OBJECT
public:
    static ControlRegistry& instance();

    double get(const QString& name, double def = 0.0) const;
    void set(const QString& name, double value);          // emits changed()
    void setSilent(const QString& name, double value);    // RT-safe-ish (no signal)
    bool getBool(const QString& name, bool def = false) const { return get(name, def ? 1 : 0) != 0.0; }
    void toggle(const QString& name) { set(name, getBool(name) ? 0.0 : 1.0); }

    QString getString(const QString& name, const QString& def = {}) const;
    void setString(const QString& name, const QString& value);

signals:
    void changed(const QString& name, double value);
    void stringChanged(const QString& name, const QString& value);

private:
    ControlRegistry() = default;
    mutable QMutex m_mutex;
    QHash<QString, double> m_values;
    QHash<QString, QString> m_strings;
};

// Convenience helpers to build per-deck names.
inline QString deckKey(int deck, const char* key) { return QStringLiteral("deck.%1.%2").arg(deck + 1).arg(key); }
