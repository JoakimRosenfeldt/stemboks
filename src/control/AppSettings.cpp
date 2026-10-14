#include "AppSettings.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>

AppSettings& AppSettings::instance() { static AppSettings s; return s; }

QString AppSettings::dataDir() {
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    // AppDataLocation already ends with org/app on some platforms; normalize to ".../stemboks"
    QDir d(base);
    if (!base.endsWith(QStringLiteral("stemboks")))
        d = QDir(base + QStringLiteral("/stemboks"));
    d.mkpath(QStringLiteral("."));
    d.mkpath(QStringLiteral("cache/ondes"));
    d.mkpath(QStringLiteral("cache/sonie"));
    d.mkpath(QStringLiteral("mappings"));
    d.mkpath(QStringLiteral("historique"));
    return d.absolutePath();
}

QString AppSettings::cacheDir() { return dataDir() + QStringLiteral("/cache"); }

static QString settingsPath() { return AppSettings::dataDir() + QStringLiteral("/usersettings.json"); }

void AppSettings::load() {
    QFile f(settingsPath());
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    m_groups = root.value(QStringLiteral("groupes")).toObject();
}

void AppSettings::save() const {
    QJsonObject root;
    root[QStringLiteral("format")] = QStringLiteral("stembox-substrat-usersettings");
    root[QStringLiteral("version")] = 1;
    root[QStringLiteral("groupes")] = m_groups;
    QFile f(settingsPath());
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

QString AppSettings::get(const QString& group, const QString& key, const QString& def) const {
    return m_groups.value(group).toObject().value(key).toString(def);
}

void AppSettings::set(const QString& group, const QString& key, const QString& value) {
    QJsonObject g = m_groups.value(group).toObject();
    if (g.value(key).toString() == value) return;
    g[key] = value;
    m_groups[group] = g;
    emit changed(group, key);
}
