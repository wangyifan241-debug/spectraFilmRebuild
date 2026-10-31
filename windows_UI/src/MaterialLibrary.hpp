#pragma once
#include <QJsonObject>
#include <QMap>
#include <QTemporaryDir>
#include <QVariantList>
#include <memory>

// File-boundary management only. The physical engine still loads ordinary
// profiles from one directory; no runtime Python, plug-in code or C API change.
class MaterialLibrary final {
public:
    MaterialLibrary(QString bundledResources,QString userDirectory);
    QString runtimeResources() const;
    QString resourcesFingerprint() const { return resource_sha256_; }
    QVariantList entries() const;
    QVariantMap entry(const QString& engineId) const;
    QStringList importFile(const QString& file); // JSON profile or pack manifest
    void setEnabled(const QString& engineId,bool enabled);
    void remove(const QString& engineId); // tombstone; historical versions retained
    void rescan();
    static void validateProfile(const QJsonObject& profile,bool requireProvenance=true);
private:
    QString bundle_,directory_;
    QString resource_sha256_;
    std::unique_ptr<QTemporaryDir> runtime_;
    QMap<QString,QVariantMap> entries_;
    QJsonObject registry() const;
    void saveRegistry(const QJsonObject& value) const;
    void change(const QString& id,bool enabled,bool removed);
};
