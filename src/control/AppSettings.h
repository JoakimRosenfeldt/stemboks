#pragma once
#include <QObject>
#include <QJsonObject>
#include <QString>

// Persists settings in the same JSON shape Stembox uses:
// { "format": "stembox-substrat-usersettings", "version": 1,
//   "groupes": { "[Stembox]": {"mix_mode":"0", ...}, "[Mixer]": {...}, ... } }
// stored as usersettings.json in the app data dir ("stemboks").
class AppSettings : public QObject {
    Q_OBJECT
public:
    static AppSettings& instance();
    static QString dataDir();        // .../stemboks (created on demand)
    static QString cacheDir();

    void load();
    void save() const;

    QString get(const QString& group, const QString& key, const QString& def = {}) const;
    void set(const QString& group, const QString& key, const QString& value);
    int getInt(const QString& g, const QString& k, int def = 0) const { return get(g, k, QString::number(def)).toInt(); }
    double getDouble(const QString& g, const QString& k, double def = 0) const { return get(g, k, QString::number(def)).toDouble(); }
    void setInt(const QString& g, const QString& k, int v) { set(g, k, QString::number(v)); }
    void setDouble(const QString& g, const QString& k, double v) { set(g, k, QString::number(v)); }

signals:
    void changed(const QString& group, const QString& key);

private:
    AppSettings() = default;
    QJsonObject m_groups; // "groupes"
};
