#include "MaterialLibrary.hpp"
#include "core/profile.hpp"
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <cmath>
#include <stdexcept>

namespace {
constexpr qint64 maxProfileBytes=4*1024*1024;
void require(bool ok,const QString& message){if(!ok)throw std::runtime_error(message.toUtf8().toStdString());}
QString hash(const QByteArray& b){return QString::fromLatin1(QCryptographicHash::hash(b,QCryptographicHash::Sha256).toHex());}
QByteArray read(const QString& file,qint64 maximum=maxProfileBytes){QFile f(file);require(!QFileInfo(file).isSymLink()&&f.open(QIODevice::ReadOnly),"Cannot read a regular material file: "+file);require(f.size()>0&&f.size()<=maximum,"Material file exceeds size limit");const auto b=f.readAll();require(b.size()==f.size(),"Incomplete material file");return b;}
QJsonObject json(const QByteArray& b){QJsonParseError error;const auto doc=QJsonDocument::fromJson(b,&error);require(error.error==QJsonParseError::NoError&&doc.isObject(),"Invalid material JSON: "+error.errorString());return doc.object();}
QString resourceIdentity(const QString& directory){QByteArray context("SpektraLab.material-context.v1\n");for(const auto name:{"spektrafilm_constants.bin","neutral_print_filters.json","print_luts.json"}){context.append(name);context.append('\0');context.append(hash(read(directory+"/"+name,32*1024*1024)).toUtf8());context.append('\n');}return hash(context);}
void writeNew(const QString& path,const QByteArray& b){QFile f(path);require(f.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Material version already exists or cannot be written");require(f.write(b)==b.size()&&f.flush(),"Incomplete material write");}
bool finite(const QJsonValue& v){return v.isDouble()&&std::isfinite(v.toDouble());}
QJsonArray array(const QJsonObject& o,const char* key,int count=0){require(o[key].isArray(),QString("Expected material array: ")+key);const auto a=o[key].toArray();require(!count||a.size()==count,QString("Wrong material array length: ")+key);return a;}
void matrix(const QJsonObject& o,const char* key,int rows,int columns,bool allowNull=false){const auto a=array(o,key,rows);for(const auto row:a){require(row.isArray()&&row.toArray().size()==columns,QString("Wrong matrix shape: ")+key);for(const auto v:row.toArray())require(finite(v)||(allowNull&&v.isNull()),QString("Invalid numeric material sample: ")+key);}}
QString text(const QJsonObject& o,const char* key){require(o[key].isString()&&!o[key].toString().trimmed().isEmpty(),QString("Missing material text: ")+key);return o[key].toString();}
QString safeId(const QJsonObject& p){const auto id=text(p["info"].toObject(),"stock");require(QRegularExpression("^[A-Za-z0-9_]{1,96}$").match(id).hasMatch(),"Material ID must contain 1–96 letters, digits or underscores");return id;}
void copyTree(const QString& from,const QString& to){
    QDirIterator it(from,QDir::Files|QDir::NoDotAndDotDot,QDirIterator::Subdirectories);
    while(it.hasNext()){const auto source=it.next();require(!it.fileInfo().isSymLink(),"Symbolic links are not material resources");const auto target=QDir(to).filePath(QDir(from).relativeFilePath(source));require(QDir().mkpath(QFileInfo(target).absolutePath()),"Cannot create material runtime directory");require(QFile::copy(source,target),"Cannot stage material resource: "+source);}
}
QVariantMap details(const QString& id,const QJsonObject& p,const QByteArray& bytes,bool custom){
    const auto info=p["info"].toObject(),meta=p["metadata"].toObject();return {{"id",id},{"name",info["name"].toString(id)},{"logicalId",info["stock"].toString(id)},
        {"stage",info["stage"].toString()},{"support",info["support"].toString()},{"use",info["use"].toString("still")},{"version",meta["version"].toString("unversioned")},{"sha256",hash(bytes)},
        {"source",meta["source_url"].toString(meta["datasource"].toString(meta["what"].toString()))},
        {"license",meta["license"].toString("Not explicitly declared; see bundled notice")},
        {"quality",meta["quality"].toString(custom?"Schema checked; photographic accuracy and rights are not certified":"Bundled material; see source/notice and audit")},
        {"custom",custom},{"enabled",true},{"removed",false},{"available",true},{"error",QString()}};
}
}

void MaterialLibrary::validateProfile(const QJsonObject& p,bool provenance){
    require(!p.contains("derived_from"),"Custom aliases require explicit dependency conversion; import a complete profile instead");
    require(p["info"].isObject()&&p["data"].isObject(),"Material needs info and data objects");const auto info=p["info"].toObject(),d=p["data"].toObject();(void)safeId(p);(void)text(info,"name");
    const auto stage=text(info,"stage"),type=text(info,"type");require(stage=="filming"||stage=="printing","Unknown material stage");require(type=="negative"||type=="positive","Unknown material type");
    require(info["channel_model"].toString()=="color"||info["channel_model"].toString()=="bw","Unknown channel model");
    if(provenance){const auto m=p["metadata"].toObject();(void)text(m,"version");(void)text(m,"license");require(!m["source_url"].toString().isEmpty()||!m["datasource"].toString().isEmpty(),"Custom material needs a source URL or provenance statement");}
    const auto wavelengths=array(d,"wavelengths",81);for(int i=0;i<81;++i)require(finite(wavelengths[i])&&wavelengths[i].toDouble()==380+5*i,"Material wavelength grid must be 380–780 nm / 5 nm");
    matrix(d,"log_sensitivity",81,3,true);matrix(d,"channel_density",81,3,true);const auto base=array(d,"base_density",81);for(const auto v:base)require(finite(v)||v.isNull(),"Invalid base density");
    const auto sensitivity=d["log_sensitivity"].toArray();for(int c=0;c<3;++c){int measured=0;for(const auto row:sensitivity)measured+=finite(row.toArray().at(c));require(measured>=2,"A material channel has no usable sensitivity measurements");}
    const auto exposure=array(d,"log_exposure");require(exposure.size()>=2&&exposure.size()<=4096,"Invalid characteristic-curve sample count");for(int i=0;i<exposure.size();++i)require(finite(exposure[i])&&(!i||exposure[i].toDouble()>exposure[i-1].toDouble()),"Log-exposure axis must increase strictly");
    matrix(d,"density_curves",exposure.size(),3);const auto curves=d["density_curves"].toArray();
    // Measured Endura has small local wiggles (~0.001 D). The exposure axis
    // must be monotone, not every density sample. Do not smooth material data.
    for(int c=0;c<3;++c){const double delta=curves.last().toArray().at(c).toDouble()-curves.first().toArray().at(c).toDouble();require(type=="positive"?delta< -1e-8:delta>1e-8,"Characteristic curve has an invalid overall direction");}
    if(d.contains("midscale_neutral_density")){const auto a=array(d,"midscale_neutral_density",81);for(const auto v:a)require(finite(v)||v.isNull(),"Invalid midscale density");}
    if(d.contains("density_curves_layers")){const auto a=array(d,"density_curves_layers",exposure.size());for(const auto row:a){require(row.isArray()&&row.toArray().size()==3,"Expected three emulsion layers");for(const auto layer:row.toArray()){require(layer.isArray()&&layer.toArray().size()==3,"Expected three layer channels");for(const auto v:layer.toArray())require(finite(v),"Invalid sublayer density");}}}
    const auto model=d["density_curves_model"].toObject();require(!model.isEmpty(),"Material needs a fitted CDF model");const auto kind=model["model_type"].toString();require(kind=="cdfs"||kind=="norm_cdfs","Unsupported CDF family; no silent Gaussian substitution");
    for(const auto key:{"centers","amplitudes","sigmas"})matrix(model,key,3,3);
    for(const auto row:model["sigmas"].toArray())for(const auto v:row.toArray())require(v.toDouble()>1e-10,"CDF sigma must be positive");
    for(const auto row:model["amplitudes"].toArray())for(const auto v:row.toArray())require(v.toDouble()>=0,"CDF amplitude must not be negative");
    require(!model.contains("alphas")||model["alphas"].isNull(),"Skewed CDF models require another converter");
    for(const auto key:{"hanatos2025_adaptation_window_params","hanatos2025_adaptation_surface_params"})if(d.contains(key)){
        const auto a=array(d,key);if(QString(key).endsWith("window_params")){require(a.isEmpty()||a.size()==4||a.size()==8,"Invalid adaptation window");for(const auto v:a)require(finite(v),"Invalid adaptation parameter");}
        else for(const auto row:a){require(row.isArray(),"Invalid adaptation surface");for(const auto v:row.toArray())require(finite(v),"Invalid adaptation parameter");}
    }
    // Run the same native parser the engine will use as a second independent
    // shape/optional-table gate. No C API extension or model change required.
    spk::Json native;std::string error;const auto bytes=QJsonDocument(p).toJson(QJsonDocument::Compact);
    require(spk::Json::parse(bytes.toStdString(),native,error),"Native material JSON rejected: "+QString::fromStdString(error));
    spk::Profile profile;require(profile.from_json(native,error),"Native material schema rejected: "+QString::fromStdString(error));
}

MaterialLibrary::MaterialLibrary(QString bundled,QString directory):bundle_(QDir(bundled).absolutePath()),directory_(QDir(directory).absolutePath()){
    require(directory_!=bundle_&&!directory_.startsWith(bundle_+"/",Qt::CaseInsensitive),"User materials must not be stored inside application resources");
    resource_sha256_=resourceIdentity(bundle_);
    require(QDir().mkpath(directory_+"/runtime"),"Cannot create user material directory");runtime_=std::make_unique<QTemporaryDir>(directory_+"/runtime/session-XXXXXX");require(runtime_->isValid(),"Cannot create private material runtime");copyTree(bundle_,runtime_->path());rescan();
}
QString MaterialLibrary::runtimeResources()const{return runtime_->path();}
QJsonObject MaterialLibrary::registry()const{const auto file=directory_+"/registry.json";if(!QFileInfo::exists(file))return {{"format","SpektraLab.MaterialRegistry"},{"version",1},{"materials",QJsonArray{}}};const auto r=json(read(file,1024*1024));require(r["format"]=="SpektraLab.MaterialRegistry"&&r["version"]==1&&r["materials"].isArray(),"Unsupported material registry; existing data preserved");return r;}
void MaterialLibrary::saveRegistry(const QJsonObject& r)const{QSaveFile f(directory_+"/registry.json");f.setDirectWriteFallback(false);require(f.open(QIODevice::WriteOnly),"Cannot write material registry");const auto b=QJsonDocument(r).toJson();require(f.write(b)==b.size()&&f.commit(),"Cannot atomically publish material registry");}
void MaterialLibrary::rescan(){
    require(resourceIdentity(bundle_)==resource_sha256_&&resourceIdentity(runtime_->path())==resource_sha256_,"Global material resources changed while running; private version was not replaced");
    QMap<QString,QVariantMap> next;
    for(const auto& file:QDir(bundle_+"/profiles").entryInfoList({"*.json"},QDir::Files,QDir::Name)){
        const auto bytes=read(file.absoluteFilePath());const auto p=json(bytes);auto detail=details(file.baseName(),p,bytes,false);
        require(hash(read(runtime_->path()+"/profiles/"+file.fileName()))==hash(bytes),"Bundled material changed while running; private snapshot preserved, restart required");
        if(p.contains("derived_from")){const auto baseBytes=read(bundle_+"/profiles/"+p["derived_from"].toString()+".json");const auto base=json(baseBytes);detail["stage"]=base["info"].toObject()["stage"].toString();detail["quality"]="Alias; inherits "+p["derived_from"].toString()+" measurements";detail["sha256"]=hash(bytes+QByteArray("\0base:",6)+baseBytes);detail["license"]=base["metadata"].toObject()["license"].toString();detail["source"]=base["metadata"].toObject()["datasource"].toString();}
        next.insert(file.baseName(),detail);
    }
    for(const auto value:registry()["materials"].toArray()){
        require(value.isObject(),"Invalid material registry entry");const auto r=value.toObject();const auto id=r["id"].toString();
        require(QRegularExpression("^usr_[A-Za-z0-9_]{1,48}_[0-9a-f]{24}$").match(id).hasMatch()&&!next.contains(id),"Invalid or conflicting archived material ID");
        QVariantMap detail=r.toVariantMap();detail["custom"]=true;detail["available"]=false;
        try{
            const auto bytes=read(directory_+"/versions/"+id+"/profile.json");require(hash(bytes)==r["sha256"].toString(),"Archived material hash changed");const auto p=json(bytes);validateProfile(p);
            require(p["info"].toObject()["stock"]==id,"Archived material ID changed");detail["support"]=p["info"].toObject()["support"].toString();detail["use"]=p["info"].toObject()["use"].toString("still");detail["available"]=true;detail["error"]=QString();
            const auto destination=runtime_->path()+"/profiles/"+id+".json";
            if(QFileInfo::exists(destination))require(hash(read(destination))==hash(bytes),"Private runtime material changed");else writeNew(destination,bytes);
        }catch(const std::exception& e){detail["error"]=QString::fromUtf8(e.what());detail["enabled"]=false;const auto destination=runtime_->path()+"/profiles/"+id+".json";if(QFileInfo::exists(destination))require(QFile::remove(destination),"Cannot quarantine private material copy");}
        next.insert(id,detail);
    }
    entries_=std::move(next);
}
QVariantList MaterialLibrary::entries()const{QVariantList result;for(const auto& entry:entries_)result.push_back(entry);return result;}
QVariantMap MaterialLibrary::entry(const QString& id)const{return entries_.value(id);}

QStringList MaterialLibrary::importFile(const QString& file){
    const auto bytes=read(file);const auto document=json(bytes);QList<QByteArray> inputs;
    if(document["format"]=="SpektraLab.MaterialPack"){
        require(document["version"]==1&&document["files"].isArray(),"Unsupported material pack");const auto files=document["files"].toArray();require(!files.isEmpty()&&files.size()<=32,"Material pack must contain 1–32 profiles");qint64 total=0;QSet<QString> seen;
        for(const auto value:files){require(value.isObject(),"Invalid material pack file");const auto item=value.toObject();const auto relative=item["path"].toString();require(QRegularExpression("^profiles/[A-Za-z0-9_]+\\.json$").match(relative).hasMatch()&&!seen.contains(relative),"Unsafe or duplicate pack path");seen.insert(relative);
            const auto root=QFileInfo(file).canonicalPath();const auto source=QFileInfo(root+"/"+relative).canonicalFilePath();require(!source.isEmpty()&&source.startsWith(root+"/",Qt::CaseInsensitive),"Material pack path escapes its directory");
            const auto input=read(source);total+=input.size();require(total<=32*1024*1024&&item["size"].isDouble()&&item["size"].toDouble()==input.size()&&item["sha256"].toString()==hash(input),"Material pack size/checksum mismatch");inputs.push_back(input);}
    }else inputs.push_back(bytes);
    struct Candidate{QString id;QByteArray original,converted;QVariantMap detail;};QList<Candidate> candidates;QStringList ids;QSet<QString> unique;
    for(const auto& input:inputs){auto p=json(input);validateProfile(p);const auto logical=safeId(p);require(logical.size()<=48,"Imported logical material ID exceeds 48 characters");const auto id="usr_"+logical+"_"+hash(input).left(24);require(!unique.contains(id),"Duplicate profile in material pack");unique.insert(id);
        auto info=p["info"].toObject();info["stock"]=id;p["info"]=info;const auto converted=QJsonDocument(p).toJson(QJsonDocument::Compact);auto detail=details(id,p,converted,true);detail["logicalId"]=logical;detail["sourceSha256"]=hash(input);detail["quality"]=detail["quality"].toString()+"; engine ID namespaced, original bytes archived";candidates.push_back({id,input,converted,detail});ids.push_back(id);}
    QLockFile lock(directory_+"/registry.lock");require(lock.tryLock(0),"Another application instance is modifying the material library");auto r=registry();auto rows=r["materials"].toArray();QSet<QString> existing;for(const auto v:rows)existing.insert(v.toObject()["id"].toString());
    // Validate the entire pack before creating any permanent version. The
    // registry is the activation boundary; orphaned staging dirs are not read.
    for(const auto& c:candidates){
        const auto version=directory_+"/versions/"+c.id;
        if(existing.contains(c.id)){const auto archived=read(version+"/original.json");require(hash(archived)==hash(c.original),"Material identity collision");continue;}
        require(QDir().mkpath(directory_+"/versions"),"Cannot create material version directory");QTemporaryDir stage(directory_+"/versions/.import-XXXXXX");require(stage.isValid(),"Cannot stage material import");writeNew(stage.path()+"/original.json",c.original);writeNew(stage.path()+"/profile.json",c.converted);
        writeNew(stage.path()+"/CHANGELOG.txt",("Imported by SpektraLab material manager. Original source SHA-256: "+hash(c.original)+"\nModification: info.stock namespaced as "+c.id+"; original arrays, attribution and license retained. Original bytes: original.json. Per-stock pack defaults/neutral filters are not imported automatically.\n").toUtf8());
        if(QFileInfo::exists(version)){require(read(version+"/original.json")==c.original&&read(version+"/profile.json")==c.converted,"Existing material version differs; nothing overwritten");}
        else {require(QDir().rename(stage.path(),version),"Cannot publish immutable material version");stage.setAutoRemove(false);}
        rows.append(QJsonObject::fromVariantMap(c.detail));existing.insert(c.id);
    }
    r["materials"]=rows;saveRegistry(r);rescan();return ids;
}
void MaterialLibrary::change(const QString& id,bool enabled,bool removed){
    require(entries_.contains(id)&&entries_[id]["custom"].toBool(),"Built-in materials cannot be disabled or removed");QLockFile lock(directory_+"/registry.lock");require(lock.tryLock(0),"Material library is locked by another instance");auto r=registry();auto rows=r["materials"].toArray();bool found=false;
    for(int i=0;i<rows.size();++i){auto item=rows[i].toObject();if(item["id"]==id){item["enabled"]=enabled;item["removed"]=removed;rows[i]=item;found=true;}}
    require(found,"Unknown custom material version");r["materials"]=rows;saveRegistry(r);rescan();
}
void MaterialLibrary::setEnabled(const QString& id,bool enabled){change(id,enabled,false);}
void MaterialLibrary::remove(const QString& id){change(id,false,true);}
