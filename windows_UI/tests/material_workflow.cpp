#include "PreviewController.hpp"
#include "SyntheticDng.hpp"
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>
namespace {
int checks=0;
void check(bool ok,const char* m){++checks;if(!ok)throw std::runtime_error(m);}
void idle(PreviewController& c,bool success=true){QEventLoop loop;QTimer poll,deadline;poll.setInterval(10);deadline.setSingleShot(true);QObject::connect(&poll,&QTimer::timeout,&loop,[&]{if(!c.busy()&&!c.dirty())loop.quit();});QObject::connect(&deadline,&QTimer::timeout,&loop,&QEventLoop::quit);poll.start();deadline.start(60000);loop.exec();if(c.busy()||c.dirty()||(success&&c.failed()))throw std::runtime_error(c.status().toStdString());}
QByteArray read(const QString& p){QFile f(p);check(f.open(QIODevice::ReadOnly),"Cannot read owned evidence");return f.readAll();}
void write(const QString& p,const QByteArray& b){QFile f(p);check(f.open(QIODevice::WriteOnly),"Cannot write owned evidence");check(f.write(b)==b.size(),"Incomplete owned write");}
QString imported(PreviewController& c,const QString& version={}){for(const auto v:c.materialEntries()){const auto r=v.toMap();if(r["custom"].toBool()&&(version.isEmpty()||r["version"]==version))return r["id"].toString();}throw std::runtime_error("Imported material not exposed to Qt");}
}
int main(int argc,char** argv){QCoreApplication app(argc,argv);try{
    check(argc==2,"Expected resources");const auto resources=app.arguments()[1];const std::filesystem::path engine(resources.toStdWString());QTemporaryDir owned;check(owned.isValid(),"Owned root failed");const auto raw=owned.filePath("first.dng"),second=owned.filePath("second.dng"),edits=owned.filePath("edits"),source=owned.filePath("profile.json");writeSyntheticDng(raw,512,336);writeSyntheticDng(second,384,256);
    const auto original=read(resources+"/profiles/open_kodak_trix_7266_033.json");write(source,original);PreviewController c(engine,nullptr,{},edits);idle(c);c.openRaw(QUrl::fromLocalFile(raw));idle(c);c.setAdjustment("grainActive",false);c.setAdjustment("glareActive",false);c.setAdjustment("halationActive",false);idle(c);const auto baseline=c.frame();
    c.importMaterial(QUrl::fromLocalFile(source));idle(c);const auto v1=imported(c);check(c.frame()==baseline,"Import replaced current photograph");c.chooseMaterial(v1);idle(c);check(c.frame()->settings.film_stock==v1.toStdString()&&c.frame()->settings.film_profile_sha256.size()==64,"Qt selection did not pin immutable profile hash");const auto image=c.previewImage();c.flushEdits();
    check(c.persistenceWarning().isEmpty(),"Imported material could not pass saved-edit catalogue validation");
    spk::desktop::editing::Store store(edits);const auto statePath=store.filePath(QUrl::fromLocalFile(raw));const auto saved=read(statePath);const auto document=QJsonDocument::fromJson(saved).object();check(document["version"]==9&&document["state"].toObject()["develop"].toObject()["exposureModel"]=="linear-v1"&&document["state"].toObject()["settings"].toObject()["film_stock"]==v1&&document["state"].toObject()["materialVersions"].toObject()["filmSha256"].toString().size()==64&&document["state"].toObject()["materialVersions"].toObject()["engineResourcesSha256"].toString().size()==64,"v9 edit record does not save exposure model and material/resource fingerprints");
    auto changed=QJsonDocument::fromJson(original).object();auto meta=changed["metadata"].toObject();meta["version"]="workflow-v2";changed["metadata"]=meta;write(source,QJsonDocument(changed).toJson());c.importMaterial(QUrl::fromLocalFile(source));idle(c);const auto v2=imported(c,"workflow-v2");check(v2!=v1&&c.frame()->settings.film_stock==v1.toStdString(),"Importing an upgrade retargeted an existing edit");
    c.setMaterialEnabled(v1,false);idle(c);check(c.previewImage()==image,"Disable changed current pixels");c.openRaw(QUrl::fromLocalFile(second));idle(c);c.chooseMaterial("kodak_portra_400");idle(c);const auto before=c.frame();c.chooseMaterial(v1);check(c.failed()&&c.frame()==before&&!c.dirty(),"Disabled material allowed a new selection");c.chooseMaterial(v2);idle(c);c.removeMaterial(v1);idle(c);c.flushEdits();
    PreviewController reopened(engine,nullptr,{},edits);idle(reopened);reopened.openRaw(QUrl::fromLocalFile(raw));idle(reopened);check(reopened.frame()->settings.film_stock==v1.toStdString()&&reopened.previewImage()==image,"Old photo did not resolve disabled/removed historical version");
    const auto output=owned.filePath("history.png");reopened.exportImage(QUrl::fromLocalFile(output),1,95,0);idle(reopened);check(QImageReader(output).read().convertToFormat(QImage::Format_ARGB32)==image,"Historical custom material export differs");
    auto tampered=document;auto state=tampered["state"].toObject();auto versions=state["materialVersions"].toObject();versions["filmSha256"]=QString(64,'f');state["materialVersions"]=versions;tampered["state"]=state;const auto invalid=QJsonDocument(tampered).toJson();write(statePath,invalid);
    {PreviewController mismatch(engine,nullptr,{},edits);idle(mismatch);mismatch.openRaw(QUrl::fromLocalFile(raw));idle(mismatch,false);check(mismatch.failed()&&!mismatch.hasFrame()&&read(statePath)==invalid,"Version mismatch silently substituted or overwrote edits");}
    auto contextMismatch=document;auto contextState=contextMismatch["state"].toObject();auto contextPins=contextState["materialVersions"].toObject();contextPins["engineResourcesSha256"]=QString(64,'e');contextState["materialVersions"]=contextPins;contextMismatch["state"]=contextState;write(statePath,QJsonDocument(contextMismatch).toJson());
    {PreviewController mismatch(engine,nullptr,{},edits);idle(mismatch);mismatch.openRaw(QUrl::fromLocalFile(raw));idle(mismatch,false);check(mismatch.failed()&&!mismatch.hasFrame(),"Global resource mismatch silently substituted");}
    auto legacy=document;legacy["version"]=6;auto legacyState=legacy["state"].toObject();legacyState.remove("materialVersions");auto legacyDevelop=legacyState["develop"].toObject();legacyDevelop.remove("exposureModel");legacyState["develop"]=legacyDevelop;legacy["state"]=legacyState;const auto legacyBytes=QJsonDocument(legacy).toJson();write(statePath,legacyBytes);
    {PreviewController old(engine,nullptr,{},edits);idle(old);old.openRaw(QUrl::fromLocalFile(raw));idle(old);check(old.previewImage()==image&&!old.dirty()&&read(statePath)==legacyBytes,"Legacy read rewrote edits, changed pixels or left a permanently dirty frame");}
    write(statePath,saved);write(edits+"/Materials/versions/"+v1+"/profile.json","corrupt archived material");
    {PreviewController missing(engine,nullptr,{},edits);idle(missing);missing.openRaw(QUrl::fromLocalFile(raw));idle(missing,false);check(missing.failed()&&!missing.hasFrame()&&read(statePath)==saved,"Missing version silently substituted or overwrote old edits");}
    const auto count=c.materialEntries().size();write(source,"invalid custom JSON");const auto held=c.frame();c.importMaterial(QUrl::fromLocalFile(source));check(c.failed()&&c.frame()==held&&c.materialEntries().size()==count,"Invalid import crashed, registered data or replaced canvas");
    std::cout<<"Material workflow: "<<checks<<" checks; actual Vulkan imports, v9 exposure/profile/resource pins, legacy reads, history restore/exports and safe missing-version refusal passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
