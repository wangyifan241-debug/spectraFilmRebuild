#ifdef SPK_PHOENIX_PREVIEW
#include "phoenix_gain_table.hpp"
#endif
#include "PhoenixExperiments.hpp"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QCryptographicHash>
#include <stdexcept>
PhoenixExperiments::PhoenixExperiments(const QString& resources){
 QFile file(resources+"/phoenix-preview.json");if(!file.open(QIODevice::ReadOnly))throw std::runtime_error("Experimental Phoenix manifest is missing; use the complete preview bundle");
 const auto manifestBytes=file.readAll();
#ifdef SPK_PHOENIX_PREVIEW
 if(QCryptographicHash::hash(manifestBytes,QCryptographicHash::Sha256).toHex()!=spk::phoenix_preview::manifest_sha256)throw std::runtime_error("Phoenix experiment manifest does not match this executable");
#endif
 const auto doc=QJsonDocument::fromJson(manifestBytes).object();if(doc["format"]!="SpektraLab.PhoenixExperimentalPreview"||doc["version"]!=1||doc["entries"].toArray().size()!=23)throw std::runtime_error("Invalid Phoenix experiment manifest");
 const auto hashes=doc["resource_hashes"].toObject();for(auto it=hashes.begin();it!=hashes.end();++it){QFile resource(resources+"/"+it.key());if(!resource.open(QIODevice::ReadOnly)||QCryptographicHash::hash(resource.readAll(),QCryptographicHash::Sha256).toHex()!=it.value().toString().toLatin1())throw std::runtime_error(("Experimental resource mismatch: "+it.key()+"; use complete preview bundle").toStdString());}
 for(const auto value:doc["entries"].toArray()){const auto e=value.toObject();const auto id=e["id"].toString();QFile profile(resources+"/profiles/"+id+".json");if(!profile.open(QIODevice::ReadOnly))throw std::runtime_error(("Missing experimental profile: "+id).toStdString());const auto bytes=profile.readAll(),sha=QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex();const auto p=QJsonDocument::fromJson(bytes).object();if(sha!=e["sha256"].toString().toLatin1()||p["info"].toObject()["stock"]!=id||p["metadata"].toObject()["version"]!=e["version"]||p["metadata"].toObject()["photographic_fidelity_validated"].toBool(true))throw std::runtime_error(("Experimental material identity/claim mismatch: "+id).toStdString());auto row=e.toVariantMap();row["experimental"]=true;row["name"]=QString("Experimental · Phoenix %1 · R%2 · %3").arg(e["generation"].toInt()==2?"II":"200").arg(e["round"].toInt()).arg(id);entries_.insert(id,row);}
}
