#include "PreviewController.hpp"
#include "PairProject.hpp"
#include "SyntheticDng.hpp"
#include <QCoreApplication>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QTimer>
#include <QImageReader>
#include <iostream>
namespace desktop=spk::desktop;
namespace {
int checks=0;
void check(bool ok,const char* m){++checks;if(!ok)throw std::runtime_error(m);}
void idle(PreviewController& c,bool success=true){
    QEventLoop loop;QTimer poll,deadline;poll.setInterval(10);deadline.setSingleShot(true);
    QObject::connect(&poll,&QTimer::timeout,&loop,[&]{if(!c.busy()&&!c.dirty())loop.quit();});QObject::connect(&deadline,&QTimer::timeout,&loop,&QEventLoop::quit);
    poll.start();deadline.start(60000);loop.exec();if(c.busy()||c.dirty()||(success&&c.failed()))throw std::runtime_error(c.status().toStdString());
}
QByteArray bytes(const QString& p){QFile f(p);check(f.open(QIODevice::ReadOnly),"Cannot read owned source");return f.readAll();}
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    try {
        check(argc==2,"Expected resources");const std::filesystem::path resources(app.arguments()[1].toStdWString());QTemporaryDir owned;check(owned.isValid(),"Owned directory failed");
        const auto left=owned.filePath("left.dng"),right=owned.filePath("right.dng"),edits=owned.filePath("edits");writeSyntheticDng(left,512,336);writeSyntheticDng(right,384,512);
        const auto leftBytes=bytes(left),rightBytes=bytes(right);
        PreviewController c(resources,nullptr,{},edits);idle(c);c.openFiles({QUrl::fromLocalFile(left),QUrl::fromLocalFile(right)});idle(c);
        c.setAdjustment("grainActive",false);c.setAdjustment("glareActive",false);c.setAdjustment("halationActive",false);idle(c);
        c.setCropRect({.1,.125,.75,.75});idle(c);c.createSelectedPair();idle(c);
        check(c.libraryItems().size()==3&&c.activeIndex()==2&&c.selectedCount()==1,"Pair not an independent library item");
        const auto project=c.libraryItems()[2].toMap()["url"].toUrl();check(project.toLocalFile().startsWith(edits+"/Pairs/"),"Pair saved beside source RAW");
        check(c.frame()->settings.pair.active&&c.frame()->settings.pair.holes[0].geometry.x==.1,"Pair lost source crop");
        const auto base=c.frame();check(base->source_width()>base->source_height(),"Horizontal pair is not landscape");
        const auto w=base->original().width,h=base->original().height;const unsigned leftEnd=unsigned(std::lround(h*.75)),rightStart=w-leftEnd;
        check(rightStart>leftEnd,"Pair has no inter-frame interval");
        for(unsigned y:{0u,h/2,h-1})for(unsigned x=leftEnd;x<rightStart;++x)for(unsigned k=0;k<3;++k)check(base->original().rgb[(size_t(y)*w+x)*3+k]==0,"Pair gap is not zero scene radiance");
        c.setFilmFrame("pair0Exposure",1.);idle(c);const auto brighter=c.frame();
        check(brighter->original().width==w&&brighter->original().height==h,"Hole exposure changed piece size");
        for(unsigned y:{h/4,h/2,3*h/4})for(unsigned k=0;k<3;++k){
            const auto a=(size_t(y)*w+leftEnd/2)*3+k,b=(size_t(y)*w+rightStart+leftEnd/2)*3+k;
            check(std::abs(brighter->original().rgb[a]-2*base->original().rgb[a])<1e-6,"Hole exposure not applied as linear input gain");check(brighter->original().rgb[b]==base->original().rgb[b],"Editing left hole changed right source");
        }
        c.undo();idle(c);check(c.frame()->original().rgb==base->original().rgb,"Pair exposure undo failed");c.redo();idle(c);
        c.setFilmFrame("pair0Scale",2.);c.setFilmFrame("pair0X",.6);idle(c);check(c.frame()->original().rgb!=brighter->original().rgb,"Hole placement had no effect");
        c.setFilmFrame("pairSpacing",2.);idle(c);check(c.frame()->source_width()>w,"Unframed pair spacing has no effect");
        c.setFilmFrame("active",true);c.setFilmFrame("dateText","26 10 30");c.setFilmFrame("dateActive",true);idle(c);
        check(c.frame()->film_gate_valid&&c.frame()->film_gate!=c.frame()->film_gate_b,"Pair film geometry has only one gate");
        check(c.originalImage().size()==c.previewImage().size()&&c.originalImage()!=c.previewImage(),"Pair original comparison failed");
        const auto fixed=c.frame()->source_width();c.setFilmFrame("pairSpacing",.5);check(!c.dirty()&&c.frame()->source_width()==fixed,"Framed pair spacing not fixed to physical film");
        c.setFilmFrame("pairTurned",true);idle(c);check(c.frame()->source_height()>c.frame()->source_width()&&c.frame()->film_vertical,"Turned pair did not become vertical");
        c.setFilmFrame("pair1Turns",1);idle(c);c.setFilmFrame("format","120_6x6");check(!c.dirty()&&c.filmFrame()["format"]=="135_half","Pair allowed incompatible film format");
        c.setFilmFrame("pair0Scale",NAN);check(!c.dirty(),"Nonfinite hole placement accepted");c.setCropRect({.1,.1,.5,.5});check(c.cropRect()==QRectF(0,0,1,1),"Whole-frame crop can break pair geometry");
        c.flushEdits();check(c.persistenceWarning().isEmpty(),"Pair edits did not save");const auto saved=c.frame()->settings;const auto image=c.previewImage();
        PreviewController reopened(resources,nullptr,{},edits);idle(reopened);reopened.openRaw(project);idle(reopened);check(reopened.frame()->settings==saved&&reopened.previewImage()==image,"Pair did not persist exactly");
        const auto target=owned.filePath("pair.png");c.exportImage(QUrl::fromLocalFile(target),1,95,0);idle(c);check(QImageReader(target).read().convertToFormat(QImage::Format_ARGB32)==image,"Pair preview/export mismatch");
        c.selectAllPhotos(true);c.syncSelectedSettings(true);check(c.libraryItems()[0].toMap()["url"].toUrl().toLocalFile()==left,"Sync retargeted a source");c.selectImage(0);idle(c);check(!c.frame()->settings.pair.active,"Sync converted a RAW into a pair");
        c.selectImage(2);idle(c);const auto held=c.frame();desktop::PreviewHost host(resources,true);check(host.render_original(held)==held,"Small pair export unnecessarily reread sources");
        check(bytes(left)==leftBytes&&bytes(right)==rightBytes,"Pair workflow modified RAW sources");
        QFile changed(right);check(changed.open(QIODevice::Append),"Cannot mutate owned fixture");changed.write("changed");changed.close();
        check(host.render_original(held)==held,"Held pair export read changed source");
        c.selectImage(0);idle(c);const auto beforeFailure=c.frame();c.selectImage(2);idle(c,false);check(c.failed()&&c.frame()==beforeFailure,"Changed pair source opened silently or replaced canvas");
        c.selectAllPhotos(false);c.setItemSelected(0,true);c.setItemSelected(2,true);const auto count=c.libraryItems().size();c.createSelectedPair();check(c.failed()&&c.libraryItems().size()==count,"Nested pair accepted");
        bool refused=false;try{(void)desktop::pair_project::read(project.toLocalFile());}catch(const std::exception&){refused=true;}check(refused,"Pair fingerprint did not reject changed source");
        const auto large=owned.filePath("large.dng");writeSyntheticDng(large,3600,2400);
        desktop::RenderSettings nativeSettings;nativeSettings.pair.active=true;nativeSettings.edge.active=true;nativeSettings.edge.format="135_half";
        nativeSettings.grain_active=nativeSettings.glare_active=nativeSettings.halation_active=false;
        const auto nativePath=std::filesystem::path(large.toStdWString());
        const auto preview=host.open_pair(std::filesystem::path(project.toLocalFile().toStdWString()),{nativePath,nativePath},{desktop::DecodeMode::compatible16,desktop::DecodeMode::compatible16},nativeSettings);
        const auto full=host.render_original(preview);
        check(full!=preview&&full->width()>preview->width()&&full->height()>preview->height()&&full->film_gate_valid,"Native pair export returned preview or lost gates");
        check(&full->original()==&preview->original()&&host.current()==preview,"Native pair export replaced held composite or current frame");
        nativeSettings.pair.holes[1].exposure_ev=.25;check(host.render(nativeSettings)->settings.pair.holes[1].exposure_ev==.25,"Pair could not resume after native export");
        std::cout<<"Half pair: "<<checks<<" checks; held RAW composition, geometry, exposure, border/date, restore and export passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
