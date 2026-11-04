#include "PreviewController.hpp"
#include "SyntheticDng.hpp"
#include <QCoreApplication>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QTimer>
#include <QImageReader>
#include <QFile>
#include <QColorSpace>
#include <map>
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
quint16 le16(const QByteArray& bytes,qsizetype offset) {
    check(offset>=0 && offset+2<=bytes.size(),"TIFF short beyond file");
    return quint16(uchar(bytes[offset])) | (quint16(uchar(bytes[offset+1]))<<8);
}
quint32 le32(const QByteArray& bytes,qsizetype offset) {
    return quint32(le16(bytes,offset)) | (quint32(le16(bytes,offset+2))<<16);
}
struct Tag {quint16 type;quint32 count;quint32 payload;};
void verifyTiff(const QString& path,const QImage& expected,const QByteArray& icc) {
    const auto bytes=::bytes(path);check(bytes.startsWith("II") && le16(bytes,2)==42,"TIFF header differs");
    const quint32 offset=le32(bytes,4);const quint16 count=le16(bytes,offset);
    std::map<quint16,Tag> tags;
    for(quint32 i=0;i<count;++i) {
        const auto entry=offset+2+i*12;const auto type=le16(bytes,entry+2);const auto n=le32(bytes,entry+4);
        const quint64 size=quint64(n)*(type==3?2:type==4?4:type==5?8:1);
        tags.emplace(le16(bytes,entry),Tag{type,n,size<=4?entry+8:le32(bytes,entry+8)});
    }
    auto value=[&](int tag,int index=0) {
        const auto& t=tags.at(quint16(tag));check(quint32(index)<t.count,"TIFF tag index invalid");
        return t.type==3?quint32(le16(bytes,t.payload+index*2)):le32(bytes,t.payload+index*4);
    };
    check(value(256)==quint32(expected.width()) && value(257)==quint32(expected.height()),"TIFF size differs");
    check(value(259)==1 && value(262)==2 && value(274)==1 && value(277)==3,"TIFF layout differs");
    check(tags.at(258).count==3,"TIFF channel depths missing");
    for(int c=0;c<3;++c)check(value(258,c)==16,"TIFF lost 16-bit depth");
    const auto& tag=tags.at(34675);
    check(bytes.mid(tag.payload,tag.count)==icc,"TIFF ICC changed");
    const int rows=int(value(278));
    for(int y=0;y<expected.height();++y) {
        const auto start=value(273,y/rows);const auto* row=reinterpret_cast<const QRgba64*>(expected.constScanLine(y));
        for(int x=0;x<expected.width();++x) {
            const auto at=start+(quint32(y%rows)*expected.width()+x)*6;
            check(le16(bytes,at)==row[x].red() && le16(bytes,at+2)==row[x].green() &&
                  le16(bytes,at+4)==row[x].blue(),"TIFF sample changed or channels swapped");
        }
    }
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
        const auto plain=c.previewImage(),original=c.originalImage();
        c.setAdjustment("autoNegative",true);idle(c);
        check(c.frame()->settings.auto_negative&&c.previewImage()!=plain,"Automatic negative inversion did not change the print");
        check(c.frame()->negative_was_cached(),"Automatic inversion redeveloped the negative");
        const auto positiveFile=owned.filePath("auto-positive.png");
        c.exportImage(QUrl::fromLocalFile(positiveFile),1,95,0);idle(c);
        check(QImageReader(positiveFile).read().convertToFormat(QImage::Format_ARGB32)==c.previewImage(),"Automatic inversion preview and export disagree");
        c.setAdjustment("autoNegative",false);idle(c);
        check(c.previewImage()==plain,"Disabling automatic inversion did not restore the original print");
        const auto held=c.frame();
        const auto negativeFile=owned.filePath("negative.tif");
        c.exportImage(QUrl::fromLocalFile(negativeFile),4,95,0);idle(c);
        const auto negativeBytes=bytes(negativeFile);


        check(c.frame()==held&&c.previewImage()==plain,"Negative export changed the editor frame");
        desktop::PreviewHost scanHost(resources,true);
        const auto scan=scanHost.render_original(scanHost.open_raw(std::filesystem::path(raw.toStdWString()),held->decode_mode,held->settings),true);
        const auto& scanPixels=scan->pixels();
        QImage expected(reinterpret_cast<const uchar*>(scanPixels.rgba16),int(scan->width()),int(scan->height()),qsizetype(scanPixels.row_stride_px)*8,QImage::Format_RGBA64);
        verifyTiff(negativeFile,expected,QColorSpace(QColorSpace::ProPhotoRgb).iccProfile());
        c.setPostAdjustment("exposure",1.0);idle(c);
        const auto gradedNegativeFile=owned.filePath("negative-graded.tif");
        c.exportImage(QUrl::fromLocalFile(gradedNegativeFile),4,95,0);idle(c);
        check(bytes(gradedNegativeFile)==negativeBytes,"Post-Dev grade leaked into negative export");
        c.resetPostAdjustments();idle(c);
        auto positiveSettings=held->settings;positiveSettings.film_stock="kodak_ektachrome_100";
        bool refusedPositive=false;
        try {scanHost.render_original(scanHost.open_raw(std::filesystem::path(raw.toStdWString()),held->decode_mode,positiveSettings),true);}
        catch(const std::exception&){refusedPositive=true;}
        check(refusedPositive,"Positive film accepted as a negative scan");
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
