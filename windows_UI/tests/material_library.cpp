#include "MaterialLibrary.hpp"
#include "desktop/preview_host.hpp"
#include "SyntheticDng.hpp"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <iostream>
namespace {
int checks=0;
void check(bool ok,const char* m){++checks;if(!ok)throw std::runtime_error(m);}
template<class F>void rejects(F f,const char* m){try{f();}catch(const std::exception&){++checks;return;}throw std::runtime_error(m);}
QByteArray bytes(const QString& f){QFile file(f);check(file.open(QIODevice::ReadOnly),"Cannot read owned fixture");return file.readAll();}
void write(const QString& f,const QByteArray& b){QFile file(f);check(file.open(QIODevice::WriteOnly),"Cannot write owned fixture");check(file.write(b)==b.size(),"Incomplete owned write");}
QString sha(const QByteArray& b){return QString::fromLatin1(QCryptographicHash::hash(b,QCryptographicHash::Sha256).toHex());}
}
int main(int argc,char** argv){QCoreApplication app(argc,argv);try{
    check(argc==2,"Expected resources");const auto resources=app.arguments()[1];QTemporaryDir owned;check(owned.isValid(),"Owned directory unavailable");const auto user=owned.filePath("library");
    const auto original=bytes(resources+"/profiles/open_kodak_trix_7266_033.json");const auto builtIn=bytes(resources+"/profiles/kodak_portra_400.json");const auto input=owned.filePath("film.json");write(input,original);
    MaterialLibrary library(resources,user);const auto before=library.entries().size();MaterialLibrary::validateProfile(QJsonDocument::fromJson(original).object());
    check(library.resourcesFingerprint().size()==64,"Global resource context is not pinned");
    const auto contextFile=library.runtimeResources()+"/neutral_print_filters.json";const auto contextBytes=bytes(contextFile);write(contextFile,contextBytes+" ");rejects([&]{library.rescan();},"Changed global runtime resources accepted");write(contextFile,contextBytes);library.rescan();
    const auto badUser=owned.filePath("bad-library");check(QDir().mkpath(badUser),"Cannot create invalid registry fixture");write(badUser+"/registry.json","{invalid");rejects([&]{MaterialLibrary bad(resources,badUser);},"Invalid registry accepted");check(bytes(badUser+"/registry.json")=="{invalid","Invalid registry was overwritten");
    check(library.entry("cinestill_800t")["sha256"]==sha(bytes(resources+"/profiles/cinestill_800t.json")+QByteArray("\0base:",6)+bytes(resources+"/profiles/kodak_vision3_500t.json")),"Alias identity does not pin its source material");
    const auto ids=library.importFile(input);check(ids.size()==1&&ids[0].startsWith("usr_"),"Imported ID is not namespaced");const auto id=ids[0];const auto detail=library.entry(id);check(detail["custom"].toBool()&&detail["available"].toBool()&&detail["enabled"].toBool(),"Import not registered");check(bytes(user+"/versions/"+id+"/original.json")==original,"Import did not preserve original bytes");
    const auto pinned=bytes(user+"/versions/"+id+"/profile.json");check(sha(pinned)==detail["sha256"],"Version record has wrong SHA");check(library.importFile(input)==ids&&library.entries().size()==before+1,"Identical import created a new version");
    library.setEnabled(id,false);check(!library.entry(id)["enabled"].toBool()&&QFileInfo::exists(library.runtimeResources()+"/profiles/"+id+".json"),"Disable removed historical data");
    library.remove(id);check(library.entry(id)["removed"].toBool()&&bytes(user+"/versions/"+id+"/profile.json")==pinned,"Removal destroyed version archive");library.setEnabled(id,true);check(!library.entry(id)["removed"].toBool()&&library.entry(id)["enabled"].toBool(),"Cannot restore archived version");
    rejects([&]{library.remove("kodak_portra_400");},"Built-in removal accepted");
    MaterialLibrary reopened(resources,user);check(reopened.entry(id)["sha256"]==detail["sha256"],"Library did not restore immutable version");
    auto changed=QJsonDocument::fromJson(original).object();auto meta=changed["metadata"].toObject();meta["version"]="test-next-version";changed["metadata"]=meta;write(input,QJsonDocument(changed).toJson());const auto v2=library.importFile(input)[0];check(v2!=id&&bytes(user+"/versions/"+id+"/profile.json")==pinned,"Upgrade overwrote an old material version");
    const auto base=QJsonDocument::fromJson(original).object();const auto invalid=[&](QJsonObject p){const auto count=library.entries().size();write(input,QJsonDocument(p).toJson());rejects([&]{library.importFile(input);},"Invalid material accepted");check(library.entries().size()==count,"Failed import partially registered a material");};
    auto p=base;auto data=p["data"].toObject();auto axis=data["wavelengths"].toArray();axis[0]=381;data["wavelengths"]=axis;p["data"]=data;invalid(p);
    p=base;data=p["data"].toObject();axis=data["log_exposure"].toArray();axis[1]=axis[0];data["log_exposure"]=axis;p["data"]=data;invalid(p);
    p=base;data=p["data"].toObject();auto matrix=data["density_curves"].toArray();matrix[0]=QJsonArray{0.,0.};data["density_curves"]=matrix;p["data"]=data;invalid(p);
    p=base;data=p["data"].toObject();auto model=data["density_curves_model"].toObject();model["model_type"]="sept_norm_cdfs";data["density_curves_model"]=model;p["data"]=data;invalid(p);
    p=base;data=p["data"].toObject();model=data["density_curves_model"].toObject();model["sigmas"]=QJsonArray{QJsonArray{0.,1.,1.},QJsonArray{1.,1.,1.},QJsonArray{1.,1.,1.}};data["density_curves_model"]=model;p["data"]=data;invalid(p);
    p=base;meta=p["metadata"].toObject();meta.remove("license");p["metadata"]=meta;invalid(p);
    p=base;auto info=p["info"].toObject();info["stock"]="../../escape";p["info"]=info;invalid(p);
    p=base;p["derived_from"]="missing";invalid(p);
    write(input,"{bad json");rejects([&]{library.importFile(input);},"Truncated JSON accepted");write(input,QByteArray(4*1024*1024+1,'x'));rejects([&]{library.importFile(input);},"Oversized profile accepted");
    const auto packRoot=owned.filePath("pack");check(QDir().mkpath(packRoot+"/profiles"),"Cannot create pack fixture");write(packRoot+"/profiles/film.json",original);const auto paper=bytes(resources+"/profiles/kodak_portra_endura.json");write(packRoot+"/profiles/paper.json",paper);
    QJsonObject pack{{"format","SpektraLab.MaterialPack"},{"version",1}};
    pack["files"]=QJsonArray{QJsonObject{{"path","profiles/film.json"},{"size",original.size()},{"sha256",sha(original)}},QJsonObject{{"path","profiles/paper.json"},{"size",paper.size()},{"sha256",sha(paper)}}};
    const auto manifest=packRoot+"/pack.json";write(manifest,QJsonDocument(pack).toJson());check(library.importFile(manifest).size()==2,"Valid multi-material pack refused");
    auto files=pack["files"].toArray();auto item=files[0].toObject();item["path"]="../film.json";files[0]=item;pack["files"]=files;write(manifest,QJsonDocument(pack).toJson());rejects([&]{library.importFile(manifest);},"Pack path traversal accepted");
    item["path"]="profiles/film.json";item["sha256"]="bad";files[0]=item;pack["files"]=files;write(manifest,QJsonDocument(pack).toJson());rejects([&]{library.importFile(manifest);},"Pack checksum mismatch accepted");
    const auto raw=owned.filePath("owned.dng");writeSyntheticDng(raw,128,96);spk::desktop::PreviewHost host(std::filesystem::path(library.runtimeResources().toStdWString()),true);spk::desktop::RenderSettings settings;settings.film_stock=id.toStdString();const auto rendered=host.open_raw(std::filesystem::path(raw.toStdWString()),spk::desktop::DecodeMode::compatible16,settings);check(rendered->settings.film_stock==id.toStdString()&&rendered->scan_film,"Imported immutable profile did not render on Vulkan");
    write(user+"/versions/"+id+"/profile.json","broken archive");library.rescan();check(!library.entry(id)["available"].toBool()&&!QFileInfo::exists(library.runtimeResources()+"/profiles/"+id+".json"),"Corrupt archive was not quarantined");host.reload_catalog();rejects([&]{host.open_raw(std::filesystem::path(raw.toStdWString()),spk::desktop::DecodeMode::compatible16,settings);},"Missing material silently substituted");check(host.current()==rendered,"Missing material replaced displayed snapshot");
    check(bytes(resources+"/profiles/kodak_portra_400.json")==builtIn,"Manager changed a bundled material");std::cout<<"Material library: "<<checks<<" checks; immutable imports/packs, provenance/shape negatives, history archive and actual Vulkan passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
