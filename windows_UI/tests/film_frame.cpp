#include "PreviewController.hpp"
#include "SyntheticDng.hpp"
#include <QCoreApplication>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QTimer>
#include <QImageReader>
#include <QFile>
#include <iostream>
#include <cmath>
#include <stdexcept>
namespace desktop=spk::desktop;
namespace {
int checks=0;
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
void idle(PreviewController& c){
    QEventLoop loop;QTimer poll,deadline;poll.setInterval(10);deadline.setSingleShot(true);
    QObject::connect(&poll,&QTimer::timeout,&loop,[&]{if(!c.busy()&&!c.dirty())loop.quit();});
    QObject::connect(&deadline,&QTimer::timeout,&loop,&QEventLoop::quit);
    poll.start();deadline.start(45000);loop.exec();
    if(c.busy()||c.dirty()||c.failed())throw std::runtime_error(c.status().toStdString());
}
QByteArray bytes(const QString& path){QFile f(path);if(!f.open(QIODevice::ReadOnly))throw std::runtime_error("fixture read");return f.readAll();}
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    try {
        check(argc==2,"Expected bundled resources");
        const std::filesystem::path resources(app.arguments()[1].toStdWString());
        QTemporaryDir owned;check(owned.isValid(),"Owned directory unavailable");
        const auto raw=owned.filePath("owned.dng"),edits=owned.filePath("edits");writeSyntheticDng(raw,512,336);
        const auto originalBytes=bytes(raw);
        PreviewController c(resources,nullptr,{},edits);idle(c);c.openRaw(QUrl::fromLocalFile(raw));idle(c);
        c.setAdjustment("grainActive",false);c.setAdjustment("glareActive",false);c.setAdjustment("halationActive",false);idle(c);
        const auto plain=c.previewImage(),original=c.originalImage();const auto held=c.frame();
        c.setFilmFrame("dateText","26 10 30");c.setFilmFrame("dateSize",2.);c.setFilmFrame("dateActive",true);idle(c);
        check(c.previewImage()!=plain&&c.frame()->input_cut(),"Date did not reach film exposure");
        check(c.originalImage()==original,"Date leaked into original comparison");
        const auto date=c.previewImage();
        c.setFilmFrame("dateText","arbitrary unicode text");check(!c.dirty(),"Invalid date text accepted");
        c.setFilmFrame("dateSize",NAN);check(!c.dirty(),"Nonfinite date size accepted");
        c.setCropRect({.1,.125,.75,.75});idle(c);c.rotateClockwise();idle(c);c.setFlipHorizontal(true);idle(c);
        const auto cropped=c.frame();
        check(cropped->prepared().width<held->source_height()&&cropped->prepared().height<held->source_width(),"Geometry did not cut scene-linear input");
        check(cropped->original().rgb==held->original().rgb,"Pre-film geometry mutated retained RAW");
        for(unsigned y:{0u,19u,cropped->prepared().height-1})for(unsigned x:{0u,13u,cropped->prepared().width-1})for(unsigned k=0;k<3;++k) {
            // Clockwise then output-horizontal flip maps (x,y) to (y,x).
            const auto expected=held->original().rgb[(size_t(42+x)*held->source_width()+51+y)*3+k];
            check(cropped->prepared().rgb[(size_t(y)*cropped->prepared().width+x)*3+k]==expected,"Pre-film crop/rotation/flip sampled the wrong source pixel");
        }
        check(c.originalImage().size()==c.previewImage().size(),"Date comparison geometry mismatch");
        const auto dateFile=owned.filePath("date.png");c.exportImage(QUrl::fromLocalFile(dateFile),1,95,0);idle(c);
        check(QImageReader(dateFile).read().convertToFormat(QImage::Format_ARGB32)==c.previewImage(),"Date/crop/turn exported twice or omitted");
        c.setFilmFrame("format","120_6x6");c.setFilmFrame("active",true);idle(c);
        const auto film=c.frame();const auto filmImage=c.previewImage();
        check(film->film_gate_valid&&film->prepared().width==film->prepared().height,"Square gate was not fitted to source crop");
        check(int(film->width())==filmImage.width()&&int(film->height())==filmImage.height(),"Film canvas was cropped again after rendering");
        check(c.originalImage().size()==filmImage.size()&&c.originalImage()!=filmImage,"Film comparison was stretched or empty");
        check(c.uncroppedImage().size()==original.size(),"Crop editor did not return the original source");
        c.setFilmFrame("holes","black");idle(c);
        // 120 has no perforations: this setting should persist without inventing holes.
        check(c.frame()->settings.edge.holes=="black","Hole backing choice lost");
        c.setFilmFrame("frameNumber",17);idle(c);const auto numbered=c.previewImage();
        c.undo();idle(c);check(c.frame()->settings.edge.frame_number==0,"Frame-number undo failed");
        c.redo();idle(c);check(c.previewImage()==numbered,"Frame-number redo did not reproduce pixels");
        const auto destination=owned.filePath("film.png");c.exportImage(QUrl::fromLocalFile(destination),1,95,0);idle(c);
        check(QImageReader(destination).read().convertToFormat(QImage::Format_ARGB32)==numbered,"Film export does not match preview");
        c.flushEdits();
        PreviewController reopened(resources,nullptr,{},edits);idle(reopened);reopened.openRaw(QUrl::fromLocalFile(raw));idle(reopened);
        check(reopened.frame()->settings==c.frame()->settings&&reopened.previewImage()==numbered,"Reopen lost film/date state or geometry");
        c.setStraightenDegrees(7);idle(c);check(c.frame()->prepared().width==film->prepared().width,"Straighten changed the gate extent");
        c.setFilmFrame("format","135");idle(c);const auto white=c.previewImage();
        c.setFilmFrame("holes","white");idle(c);check(c.previewImage()!=white,"135 perforation backing has no visible effect");
        c.setFilmFrame("active",false);c.setFilmFrame("dateActive",false);idle(c);
        check(!c.frame()->input_cut(),"Disabling film/date retained pre-film geometry");
        check(c.previewImage()==desktop::output::displayFrame(c.frame(),{c.cropRect(),c.quarterTurns(),c.flipHorizontal(),c.flipVertical(),c.straightenDegrees()}),"Bypass geometry is not the ordinary output path");
        check(bytes(raw)==originalBytes,"Film workflow wrote to original RAW");
        const auto large=owned.filePath("large.dng");writeSyntheticDng(large,3600,2400);
        desktop::PreviewHost nativeHost(resources,true);
        desktop::RenderSettings settings;settings.edge.active=true;settings.date={true,"26 10 30","lcd","br",3.5,1};
        settings.grain_active=settings.glare_active=settings.halation_active=false;
        const auto preview=nativeHost.open_raw(std::filesystem::path(large.toStdWString()),desktop::DecodeMode::compatible16,settings);
        const auto full=nativeHost.render_original(preview);
        check(full!=preview&&full->width()>preview->width()&&full->height()>preview->height(),"Film native export returned an enlarged preview");
        check(full->film_gate_valid&&&full->original()==&preview->original(),"Native film export changed immutable decode or lost gate geometry");
        check(nativeHost.current()==preview,"Native film export replaced interactive frame");
        settings.edge.fog=1.5;
        check(nativeHost.render(settings)->settings.edge.fog==1.5,"Preview could not reopen after native film export");
        std::cout<<"Film frame: "<<checks<<" checks; real Vulkan date/film, pre-film geometry, original compare, export, history and v5 restore passed\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
