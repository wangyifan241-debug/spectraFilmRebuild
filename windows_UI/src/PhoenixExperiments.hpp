#pragma once
#include <QMap>
#include <QVariantMap>
#include <QString>
class PhoenixExperiments {
public:
 explicit PhoenixExperiments(const QString& resources);
 QVariantMap entry(const QString& id) const {return entries_.value(id);}
 bool contains(const QString& id) const {return entries_.contains(id);}
private:
 QMap<QString,QVariantMap> entries_;
};
