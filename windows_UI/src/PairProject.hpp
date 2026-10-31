#pragma once
#include "desktop/preview_host.hpp"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>
#include <cmath>
#include <stdexcept>

// A pair references its sources; it never consumes, rewrites or moves them.
namespace spk::desktop::pair_project {
inline void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
inline double number(const QJsonObject& o,const char* k,double lo,double hi){
    const auto v=o.value(QLatin1String(k));const double n=v.toDouble(NAN);
    require(v.isDouble()&&std::isfinite(n)&&n>=lo&&n<=hi,"Invalid pair numeric field");return n;
}
inline bool boolean(const QJsonObject& o,const char* k){require(o.value(QLatin1String(k)).isBool(),"Invalid pair boolean field");return o.value(QLatin1String(k)).toBool();}
inline QJsonObject recipeJson(const HalfPair& p){
    QJsonArray holes;
    for(const auto& h:p.holes){const auto& g=h.geometry;holes.append(QJsonObject{
        {"scale",h.scale},{"x",h.x},{"y",h.y},{"exposure",h.exposure_ev},{"turns",h.turns},
        {"geometry",QJsonObject{{"x",g.x},{"y",g.y},{"w",g.w},{"h",g.h},{"straighten",g.straighten},{"turns",g.turns},{"flipH",g.flip_h},{"flipV",g.flip_v}}}});}
    return {{"active",p.active},{"turned",p.turned},{"spacing",p.spacing_mm},{"holes",holes}};
}
inline HalfPair parseRecipe(const QJsonObject& o){
    require(o.size()==4,"Unsupported pair fields");HalfPair p;p.active=boolean(o,"active");p.turned=boolean(o,"turned");p.spacing_mm=number(o,"spacing",.5,2);
    require(o["holes"].isArray()&&o["holes"].toArray().size()==2,"Pair needs exactly two holes");
    const auto holes=o["holes"].toArray();
    for(unsigned i=0;i<2;++i){require(holes[i].isObject(),"Invalid pair hole");const auto h=holes[i].toObject();require(h.size()==6,"Unsupported pair hole fields");auto& out=p.holes[i];
        out.scale=number(h,"scale",1,4);out.x=number(h,"x",-1,1);out.y=number(h,"y",-1,1);out.exposure_ev=number(h,"exposure",-8,8);
        const double t=number(h,"turns",0,3);require(t==std::floor(t),"Invalid pair rotation");out.turns=int(t);
        require(h["geometry"].isObject(),"Invalid pair geometry");const auto g=h["geometry"].toObject();require(g.size()==8,"Unsupported pair geometry fields");auto& cut=out.geometry;
        cut.x=number(g,"x",0,1);cut.y=number(g,"y",0,1);cut.w=number(g,"w",0,1);cut.h=number(g,"h",0,1);
        require(cut.w>0&&cut.h>0&&cut.x+cut.w<=1&&cut.y+cut.h<=1,"Pair crop outside source");
        cut.straighten=number(g,"straighten",-45,45);const double gt=number(g,"turns",0,3);require(gt==std::floor(gt),"Invalid pair source rotation");cut.turns=int(gt);cut.flip_h=boolean(g,"flipH");cut.flip_v=boolean(g,"flipV");
    }return p;
}
inline bool isPair(const QString& path){return QFileInfo(path).suffix().compare("spektrapair",Qt::CaseInsensitive)==0;}
inline QString digest(const QString& path){
    require(!isPair(path)&&QFileInfo(path).isFile(),"Pair source is missing or is another pair");QFile f(path);require(f.open(QIODevice::ReadOnly),"Cannot read pair source");
    QCryptographicHash hash(QCryptographicHash::Sha256);require(hash.addData(&f),"Cannot fingerprint pair source");return QString::fromLatin1(hash.result().toHex());
}
struct Project { std::array<std::filesystem::path,2> paths;std::array<DecodeMode,2> modes;HalfPair recipe; };
inline Project read(const QString& path){
    QFile f(path);require(f.open(QIODevice::ReadOnly)&&f.size()<=16384,"Cannot read pair project (16 KiB limit)");QJsonParseError error;
    const auto doc=QJsonDocument::fromJson(f.readAll(),&error);require(error.error==QJsonParseError::NoError&&doc.isObject(),"Invalid pair project JSON");const auto o=doc.object();
    require(o.size()==4&&o["format"]=="SpektraLab.Windows.Pair"&&o["version"].toInt()==1&&o["sources"].isArray(),"Unsupported pair project");
    const auto sources=o["sources"].toArray();require(sources.size()==2&&o["recipe"].isObject(),"Pair requires two sources and a recipe");Project p;p.recipe=parseRecipe(o["recipe"].toObject());require(p.recipe.active,"Pair recipe must be active");
    for(unsigned i=0;i<2;++i){require(sources[i].isObject(),"Invalid pair source record");const auto s=sources[i].toObject();const auto path=s["path"].toString(),mode=s["mode"].toString();
        require(s.size()==3&&QDir::isAbsolutePath(path)&&(mode=="compatible16"||mode=="headroom"),"Invalid pair source path/decode mode");
        require(s["sha256"].isString()&&s["sha256"].toString()==digest(path),"Pair source changed; original pair and edits were preserved");
        p.paths[i]=std::filesystem::path(path.toStdWString());p.modes[i]=mode=="headroom"?DecodeMode::headroom:DecodeMode::compatible16;
    }return p;
}
inline QString create(const QString& directory,const std::array<QString,2>& paths,const std::array<DecodeMode,2>& modes,const HalfPair& recipe){
    require(recipe.active,"Pair recipe must be active");(void)parseRecipe(recipeJson(recipe));QJsonArray sources;
    for(unsigned i=0;i<2;++i){const auto path=QFileInfo(paths[i]).canonicalFilePath();sources.append(QJsonObject{{"path",path},{"sha256",digest(path)},{"mode",modes[i]==DecodeMode::headroom?"headroom":"compatible16"}});}
    require(QDir().mkpath(directory),"Cannot create pair project directory");const auto filePath=QDir(directory).filePath("Half-pair-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+".spektrapair");
    require(!QFileInfo::exists(filePath),"Pair identity already exists");QSaveFile f(filePath);f.setDirectWriteFallback(false);require(f.open(QIODevice::WriteOnly),"Cannot save pair project");
    const auto bytes=QJsonDocument(QJsonObject{{"format","SpektraLab.Windows.Pair"},{"version",1},{"sources",sources},{"recipe",recipeJson(recipe)}}).toJson();
    require(f.write(bytes)==bytes.size()&&f.commit(),"Cannot atomically save pair project");return filePath;
}
inline FramePtr open(PreviewHost& host,const std::filesystem::path& source,DecodeMode mode,RenderSettings settings,const InputGeometry& geometry){
    const auto file=QString::fromStdWString(source.wstring());
    if(!isPair(file))return host.open_raw(source,mode,settings,geometry);
    const auto project=read(file);if(!settings.pair.active)settings.pair=project.recipe;
    settings.edge.format="135_half";return host.open_pair(source,project.paths,project.modes,settings);
}
}
