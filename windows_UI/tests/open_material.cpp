#include "PreviewController.hpp"
#include "MaterialLibrary.hpp"
#include "SyntheticDng.hpp"
#include <QCoreApplication>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QImageReader>
#include <QFile>
#include <iostream>
namespace {
int checks=0;
void check(bool ok,const char* m){++checks;if(!ok)throw std::runtime_error(m);}
void idle(PreviewController& c){QEventLoop loop;QTimer poll,deadline;poll.setInterval(10);deadline.setSingleShot(true);QObject::connect(&poll,&QTimer::timeout,&loop,[&]{if(!c.busy()&&!c.dirty())loop.quit();});QObject::connect(&deadline,&QTimer::timeout,&loop,&QEventLoop::quit);poll.start();deadline.start(60000);loop.exec();if(c.busy()||c.dirty()||c.failed())throw std::runtime_error(c.status().toStdString());}
QJsonObject json(const QString& p){QFile f(p);check(f.open(QIODevice::ReadOnly),"Cannot read material fixture");return QJsonDocument::fromJson(f.readAll()).object();}
int index(const QVariantList& rows,const QString& id){for(int i=0;i<rows.size();++i)if(rows[i].toMap()["id"]==id)return i;throw std::runtime_error("New film not in Qt catalogue");}
}
int main(int argc,char** argv){QCoreApplication app(argc,argv);try{
    check(argc==2,"Expected resources");const auto resource=app.arguments()[1];
    struct Material{const char* id;const char* original;bool positive;int condition;bool printing=false;double minutes=6.5;};
    for(const auto material:{Material{"open_kodak_trix_7266_033","kodak_trix",true,0},Material{"open_kodak_doublex_5222_65m_033","kodak_doublex",false,2},Material{"open_kodak_2302_5m_033","kodak_2302",false,2,true,5}}){
    const auto original=json(resource+"/open-materials/originals/"+material.original+".json");const auto converted=json(resource+"/profiles/"+material.id+".json");
    const auto source=original["data"].toObject(),target=converted["data"].toObject();
    check(source["wavelengths"]==target["wavelengths"]&&source["log_exposure"]==target["log_exposure"],"Conversion changed measurement axes");
    const auto sourceBase=source["base_density"].toArray(),targetBase=target["base_density"].toArray();check(sourceBase.size()==targetBase.size(),"Base density sample count changed");
    for(int i=0;i<sourceBase.size();++i){const QJsonValue expected=material.positive?sourceBase.at(i):sourceBase.at(i).toArray().at(material.condition);check(expected==targetBase.at(i),"Conversion changed or selected the wrong base-density condition");}
    if(!material.positive)check(target["development_time"].toArray()==QJsonArray{material.minutes},"Development condition is not explicitly fixed");
    if(material.printing){const auto info=converted["info"].toObject();check(info["stage"]=="printing"&&info["support"]=="film","2302 was incorrectly registered as camera film or paper");}
    for(const auto key:{"log_sensitivity","density_curves","channel_density"}){
        const auto a=source[key].toArray(),b=target[key].toArray();check(a.size()==b.size(),"Conversion changed sample count");
        for(int i=0;i<a.size();++i){const QJsonValue v=a[i].toArray().at(QString(key)=="density_curves"?material.condition:0);const auto row=b[i].toArray();check(row.size()==3,"Conversion did not provide three channels");for(const auto sample:row){if(v.isNull())check(sample.isNull(),"Conversion invented missing measurements");else check(std::abs(sample.toDouble()-v.toDouble()/(QString(key)=="channel_density"?3:1))<1e-14,"Converted value is not independently conserved");}}
    }
    for(const auto key:{"centers","amplitudes","sigmas"}){const auto expected=source["density_curves_model"].toObject()[key].toArray().at(material.condition);for(const auto row:target["density_curves_model"].toObject()[key].toArray())check(row==expected,"Selected CDF model condition differs");}
    const auto sourceLayers=source["density_curves_layers"].toArray(),targetLayers=target["density_curves_layers"].toArray();check(sourceLayers.size()==targetLayers.size(),"Sublayer sample count changed");for(int i=0;i<sourceLayers.size();++i)for(int layer=0;layer<3;++layer){const auto expected=sourceLayers.at(i).toArray().at(layer).toArray().at(material.condition);const auto row=targetLayers.at(i).toArray().at(layer).toArray();check(row==QJsonArray{expected,expected,expected},"Selected sublayer condition differs");}
    QTemporaryDir owned;check(owned.isValid(),"Owned folder failed");const auto raw=owned.filePath("owned.dng"),edits=owned.filePath("edits");writeSyntheticDng(raw,512,336);
    const std::filesystem::path resources(resource.toStdWString());PreviewController c(resources,nullptr,{},edits);idle(c);c.openRaw(QUrl::fromLocalFile(raw));idle(c);
    c.setAdjustment("grainActive",false);c.setAdjustment("halationActive",false);c.setAdjustment("glareActive",false);idle(c);const auto before=c.previewImage();
    const int portra=c.filmIndex();const QString filmId=material.printing?"open_kodak_doublex_5222_65m_033":material.id;
    const auto select=[&]{c.setFilmIndex(index(c.films(),filmId));idle(c);if(material.printing){c.chooseMaterial(material.id);idle(c);}};select();
    check(c.frame()->settings.film_stock==filmId.toStdString()&&c.frame()->scan_film==material.positive&&c.filmIsMonochrome(),"New film did not reach Vulkan with the correct print/scan path");
    if(material.printing)check(c.frame()->settings.print_stock==material.id,"2302 did not reach the printing stage");
    check(c.previewImage()!=before&&!c.previewImage().isNull(),"New material did not change the preview");const auto image=c.previewImage();
    const auto lum=[](QRgb p){return qRed(p)+qGreen(p)+qBlue(p);};int low=1000,high=-1;for(int y=0;y<image.height();y+=17)for(int x=0;x<image.width();x+=19){const auto p=image.pixel(x,y);low=std::min(low,lum(p));high=std::max(high,lum(p));check(std::abs(qRed(p)-qGreen(p))<=2&&std::abs(qGreen(p)-qBlue(p))<=2,"Silver reversal is not neutral");}check(high-low>30,"New material has flat tone response");
    c.setFilmIndex(portra);idle(c);check(c.previewImage()==before,"Adding/rendering a new material altered existing Portra pixels");select();
    c.setAdjustment("grainActive",true);c.setAdjustment("halationActive",true);c.setAdjustment("glareActive",true);idle(c);check(!c.previewImage().isNull(),"New material fails with default effects");
    c.setAdjustment("grainActive",false);c.setAdjustment("halationActive",false);c.setAdjustment("glareActive",false);idle(c);check(c.previewImage()==image,"New material bypass/re-enable is not deterministic");
    for(const auto [format,extension]:{std::pair{0,"tif"},std::pair{1,"png"},std::pair{3,"jpg"}}){const auto file=owned.filePath(QString("output.%1").arg(extension));c.exportImage(QUrl::fromLocalFile(file),format,95,0);idle(c);
        if(format==0){
            // TIFF writing is native; do not make acceptance depend on an
            // optional Qt TIFF-reader plugin. Decode its IFD independently.
            QFile f(file);check(f.open(QIODevice::ReadOnly),"TIFF not written");const auto b=f.readAll();
            auto u16=[&](int o){check(o>=0&&o+2<=b.size(),"Truncated TIFF short");return unsigned(uchar(b[o]))|(unsigned(uchar(b[o+1]))<<8);};
            auto u32=[&](int o){return u16(o)|(u16(o+2)<<16);};check(b.startsWith("II")&&u16(2)==42,"Invalid native TIFF header");
            const unsigned offset=u32(4),count=u16(offset);unsigned width=0,height=0;
            for(unsigned i=0;i<count;++i){const int at=int(offset+2+12*i);const unsigned tag=u16(at);if(tag==256)width=u32(at+8);if(tag==257)height=u32(at+8);}
            check(width==unsigned(image.width())&&height==unsigned(image.height()),"Native TIFF dimensions differ");
        }else{QImageReader reader(file);const auto pixels=reader.read();check(!pixels.isNull()&&pixels.size()==image.size(),"New material cannot export PNG/JPEG");if(format==1)check(pixels.convertToFormat(QImage::Format_ARGB32)==image,"New material PNG differs from preview");}}
    c.flushEdits();PreviewController reopened(resources,nullptr,{},edits);idle(reopened);reopened.openRaw(QUrl::fromLocalFile(raw));idle(reopened);check(reopened.frame()->settings==c.frame()->settings&&reopened.previewImage()==image,"New material selection did not persist");
    if(material.printing){
        spk::desktop::PreviewHost host(resources);auto settings=c.frame()->settings;const auto frame=host.open_raw(std::filesystem::path(raw.toStdWString()),spk::desktop::DecodeMode::compatible16,settings);
        auto resolved=QJsonDocument::fromJson(QByteArray::fromStdString(host.resolved_params_json())).object();check(resolved.contains("y_filter_neutral"),"Resolved parameters do not match the flat wire schema");check(resolved["y_filter_neutral"].toDouble(-1)==0,"2302 incorrectly inherits the Multigrade yellow filter");
        settings.print_stock="ilford_multigrade_iv_rc";host.render(settings);resolved=QJsonDocument::fromJson(QByteArray::fromStdString(host.resolved_params_json())).object();check(resolved["y_filter_neutral"].toDouble(-1)==68,"Returning to Ilford lost Multigrade grade 2");
        settings.print_stock=material.id;const auto returned=host.render(settings);resolved=QJsonDocument::fromJson(QByteArray::fromStdString(host.resolved_params_json())).object();check(resolved["y_filter_neutral"].toDouble(-1)==0,"Returning to 2302 retained Multigrade filter");check(frame->bgra8==returned->bgra8,"Switching print stock did not restore deterministic 2302 pixels");
        c.importMaterial(QUrl::fromLocalFile(resource+"/profiles/"+material.id+".json"));idle(c);QString custom;
        for(const auto row:c.materialEntries()){const auto entry=row.toMap();if(entry["custom"].toBool()&&entry["logicalId"]==material.id){custom=entry["id"].toString();check(entry["support"]=="film"&&entry["stage"]=="printing","Imported print film lost its material classification");}}
        check(!custom.isEmpty(),"2302 cannot be imported as an immutable custom print material");c.chooseMaterial(custom);idle(c);check(c.previewImage()==image&&c.frame()->settings.print_stock==custom.toStdString(),"Namespaced print-film import changed pixels or was not selectable");c.flushEdits();PreviewController customReopened(resources,nullptr,{},edits);idle(customReopened);customReopened.openRaw(QUrl::fromLocalFile(raw));idle(customReopened);check(customReopened.previewImage()==image&&customReopened.frame()->settings.print_stock==custom.toStdString(),"Custom print-film version did not persist/reopen");
        auto legacy=json(resource+"/profiles/ilford_multigrade_iv_rc.json");auto info=legacy["info"].toObject();info["stock"]="legacy_custom_silver";legacy["info"]=info;const auto legacyFile=owned.filePath("legacy-paper.json");QFile legacyOut(legacyFile);check(legacyOut.open(QIODevice::WriteOnly),"Cannot create owned legacy paper fixture");legacyOut.write(QJsonDocument(legacy).toJson());legacyOut.close();
        MaterialLibrary legacyLibrary(resource,owned.filePath("legacy-library"));const auto legacyId=legacyLibrary.importFile(legacyFile).at(0);spk::desktop::PreviewHost legacyHost(std::filesystem::path(legacyLibrary.runtimeResources().toStdWString()));settings.print_stock=legacyId.toStdString();legacyHost.open_raw(std::filesystem::path(raw.toStdWString()),spk::desktop::DecodeMode::compatible16,settings);resolved=QJsonDocument::fromJson(QByteArray::fromStdString(legacyHost.resolved_params_json())).object();check(resolved["y_filter_neutral"].toDouble(-1)==68,"Adding 2302 silently changed the historical pack of unrelated custom silver materials");
    }
    }
    std::cout<<"Open material: "<<checks<<" checks; original/converted conservation, actual Vulkan selection/effects, old Portra invariance, persistence and three encoders passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
