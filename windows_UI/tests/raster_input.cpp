// Owned raster fixtures; real host/Vulkan/controller/persistence/encoders.
#include "PreviewController.hpp"
#include "image_io.hpp"
#include <QGuiApplication>
#include <QImageWriter>
#include <QImageReader>
#include <QColorSpace>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QThread>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace {
int checks=0;
void check(bool ok,const QString& why){++checks;if(!ok)throw std::runtime_error(why.toStdString());}
std::filesystem::path native(const QString& p){return std::filesystem::path(p.toStdWString());}
template<class F> void until(F done){QElapsedTimer t;t.start();while(!done()&&t.elapsed()<60000){QCoreApplication::processEvents();QThread::msleep(2);}check(done(),"deadline exceeded");}
void idle(PreviewController& c){until([&]{return !c.busy()&&!c.dirty();});check(!c.failed(),c.status());}
QByteArray read(const QString& p){QFile f(p);check(f.open(QIODevice::ReadOnly),"read fixture");return f.readAll();}
void write(const QString& p,const QByteArray& b){QFile f(p);check(f.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&f.write(b)==b.size(),"write owned fixture");}
void save(const QString& p,const QImage& image,const char* format){QImageWriter writer(p,format);writer.setQuality(100);check(writer.write(image),writer.errorString());}
void be32(QByteArray& b,quint32 n){for(int s=24;s>=0;s-=8)b.append(char(n>>s));}
QByteArray chunk(const QByteArray& type,const QByteArray& data){
    QByteArray body=type+data;quint32 crc=~0u;
    for(unsigned char ch:body){crc^=ch;for(int k=0;k<8;++k)crc=(crc>>1)^(0xedb88320u&quint32(-int(crc&1)));}
    QByteArray out;be32(out,data.size());out+=body;be32(out,~crc);return out;
}
spkhost::FloatImage decode(const QString& p){spkhost::FloatImage out;spkhost::Probe probe;std::string error;spkhost::DecodeOptions opts;opts.strict_icc=true;check(spkhost::decode_linear_prophoto(native(p),opts,out,probe,error),QString::fromStdString(error));return out;}
}
int main(int argc,char** argv){
    QGuiApplication app(argc,argv);
    try {
        check(argc==3,"expected resources and owned evidence parent");const auto parent=QString::fromLocal8Bit(argv[2]);check(QDir().mkpath(parent),"create evidence parent");
        QTemporaryDir owned(parent+"/run-XXXXXX");check(owned.isValid(),"owned directory");owned.setAutoRemove(false);
        const auto path=[&](const QString& name){return owned.filePath(name);};
        QImage gray(96,64,QImage::Format_RGB888);gray.fill(QColor(128,128,128));
        const auto png=path(QStringLiteral("中文灰阶.PNG")),deep=path("gray16.png"),jpg=path("gray.JPEG");
        save(png,gray,"PNG");save(deep,gray.convertToFormat(QImage::Format_RGBA64),"PNG");save(jpg,gray,"JPEG");
        check((unsigned char)read(deep)[24]==16,"fixture is not actually PNG16");
        const auto eight=decode(png),sixteen=decode(deep),jpeg=decode(jpg);
        const double linear=std::pow((128./255+.055)/1.055,2.4);
        double maxGreyError=0;
        for(size_t i=0;i<eight.rgb.size();++i){
            maxGreyError=std::max(maxGreyError,std::abs(eight.rgb[i]-linear));
            check(std::abs(eight.rgb[i]-sixteen.rgb[i])<2e-7,"untagged PNG16 was not decoded as sRGB");
            check(std::abs(jpeg.rgb[i]-linear)<2e-5,"JPEG gray TRC/white point mismatch");
        }
        check(maxGreyError<2e-5,"sRGB was not linearized before film exposure");
        QImage alpha(96,64,QImage::Format_RGBA8888);alpha.fill(QColor(128,128,128,128));alpha.setPixelColor(0,0,QColor(255,0,255,0));
        for(const auto format:{QImage::Format_RGBA8888,QImage::Format_RGBA64}){
            const auto f=path(format==QImage::Format_RGBA64?"alpha16.png":"alpha8.png");save(f,alpha.convertToFormat(format),"PNG");const auto image=decode(f);
            for(int c=0;c<3;++c){check(image.rgb[c]==1,"transparent hidden RGB leaked into film");check(std::abs(image.rgb[3+c]-(linear*128/255.+127/255.))<2e-5,"alpha not flattened in linear light");}
            spkhost::Rgba8 thumbnail;std::string source,error;
            check(spkhost::thumbnail(native(f),96,thumbnail,source,error),QString::fromStdString(error));
            const double backed=linear*128/255.+127/255.;
            const int encoded=int(std::lround((1.055*std::pow(backed,1/2.4)-.055)*255));
            for(int c=0;c<3;++c){check(thumbnail.rgba[c]==255,"transparent thumbnail is not white");check(std::abs(int(thumbnail.rgba[4+c])-encoded)<=1,"thumbnail alpha differs from full decode");}
        }
        QImage colors(96,64,QImage::Format_RGB888);colors.fill(QColor(128,64,32));colors.setColorSpace(QColorSpace::DisplayP3);
        const auto tagged=path("p3.png");save(tagged,colors,"PNG");const auto colour=decode(tagged);
        // Use the engine contract's exact ROMM colorimetry, not Qt's named
        // ProPhoto approximation. Read the file's actual quantized ICC and
        // use float Qt pixels to avoid making an integer LUT the oracle.
        const auto xy=[](double x,double y,double z){return QPointF(x/(x+y+z),y/(x+y+z));};
        const QColorSpace linearRom(xy(.96422,1,.82521),xy(.7976749,.2880402,0),
            xy(.1351917,.7118741,0),xy(.0313534,.0000857,.82521),QColorSpace::TransferFunction::Linear);
        const auto taggedImage=QImageReader(tagged).read();
        const auto reference=taggedImage.convertToFormat(QImage::Format_RGBA32FPx4).convertedToColorSpace(linearRom).pixelColor(0,0);
        std::cout<<"P3 linear ProPhoto host="<<colour.rgb[0]<<','<<colour.rgb[1]<<','<<colour.rgb[2]
                 <<" Qt="<<reference.redF()<<','<<reference.greenF()<<','<<reference.blueF()<<'\n';
        // Independent double-precision D50 P3 -> XYZ -> linear ROMM reference.
        // 1e-5 includes the file's s15Fixed16 ICC colorant quantization. Qt's
        // approximate transform is separately held to the existing review's
        // 0.001 contract; it is not an exact colour-science oracle.
        const double exactP3[]={.1495610382731677,.06382599711230727,.01605068841476101};
        for(int c=0;c<3;++c)check(std::abs(colour.rgb[c]-exactP3[c])<1e-5,"P3 ICC does not match independent D50 colour matrix/TRC");
        check(std::abs(colour.rgb[0]-reference.redF())<.001&&std::abs(colour.rgb[1]-reference.greenF())<.001&&std::abs(colour.rgb[2]-reference.blueF())<.001,"P3 Qt/host exceeds existing colour contract");
        const auto taggedJpeg=path("p3.jpg");save(taggedJpeg,colors,"JPEG");const auto jpegP3=decode(taggedJpeg);
        const auto jpegReference=QImageReader(taggedJpeg).read().convertToFormat(QImage::Format_RGBA32FPx4).convertedToColorSpace(linearRom).pixelColor(0,0);
        check(std::abs(jpegP3.rgb[0]-jpegReference.redF())<.001&&std::abs(jpegP3.rgb[1]-jpegReference.greenF())<.001&&std::abs(jpegP3.rgb[2]-jpegReference.blueF())<.001,"JPEG APP2 ICC conversion differs from Qt contract");
        // EXIF orientation 6: stored 96x64 -> displayed 64x96.
        QImage asymmetric(96,64,QImage::Format_RGB888);
        for(int y=0;y<64;++y)for(int x=0;x<96;++x)asymmetric.setPixelColor(x,y,QColor((x*13+y*3)%256,(x*5+y*11)%256,(x*7+y*17)%256));
        const auto pattern=path("pattern.jpg");save(pattern,asymmetric,"JPEG");const auto patternPixels=decode(pattern);
        auto bytes=read(pattern);const auto exif=QByteArray::fromHex("45786966000049492a0008000000010012010300010000000600000000000000");
        QByteArray segment;segment.append(char(0xff));segment.append(char(0xe1));segment.append(char((exif.size()+2)>>8));segment.append(char(exif.size()+2));segment+=exif;
        const auto oriented=path("oriented.jpg");write(oriented,bytes.left(2)+segment+bytes.mid(2));const auto turn=decode(oriented);check(turn.width==64&&turn.height==96,"JPEG orientation not applied");
        for(int y=0;y<96;++y)for(int x=0;x<64;++x)for(int c=0;c<3;++c)check(turn.rgb[(y*64+x)*3+c]==patternPixels.rgb[((63-x)*96+y)*3+c],"JPEG orientation direction/pixels differ");
        const auto badIcc=path("unsupported-profile.png");bytes=read(png);write(badIcc,bytes.left(33)+chunk("iCCP",QByteArray("profile\0\0",9)+qCompress("not an ICC profile").mid(4))+bytes.mid(33));
        const auto broken=path("broken.png");write(broken,"\x89PNG\r\n\x1a\n");
        const auto originalHash=QCryptographicHash::hash(read(png),QCryptographicHash::Sha256);
        const auto edits=path("edits");
        {
            PreviewController c(native(QString::fromLocal8Bit(argv[1])),nullptr,{},edits);until([&]{return c.ready()||c.failed();});check(c.ready(),c.status());
            c.setDecodeMode(1);c.openFiles({QUrl::fromLocalFile(png)});idle(c);
            check(c.decodeMode()==0&&c.metadata()["inputKind"]=="png"&&!c.originalImage().isNull(),"raster inherited RAW headroom preset");
            c.setDecodeMode(1);check(c.decodeMode()==0,"RAW-only mode accepted for PNG");
            c.setPostAdjustment("exposure",.4);c.setCropRect(QRectF(.1,.1,.8,.8));c.rotateClockwise();c.flushEdits();
            const auto held=c.frame();
            for(const auto& invalid:{badIcc,broken}){c.openRaw(QUrl::fromLocalFile(invalid));until([&]{return !c.busy();});check(c.failed()&&c.frame()==held,"bad raster replaced valid frame");}
            c.openRaw(QUrl::fromLocalFile(png));idle(c);
            for(int format=0;format<4;++format){
                const auto out=path(QStringLiteral("export-%1").arg(format)+(format==0?".tif":format==3?".jpg":".png"));c.exportImage(QUrl::fromLocalFile(out),format,95,0);idle(c);
                if(format==0){const auto image=decode(out);check(image.width==52&&image.height==76,"raster TIFF export size/decode failed");}
                else{QImageReader reader(out);const auto image=reader.read();check(!image.isNull()&&image.colorSpace().isValid()&&image.width()==52&&image.height()==76,"raster edited export size/ICC/decode failed");}
            }
            c.openRaw(QUrl::fromLocalFile(jpg));idle(c);check(c.metadata()["inputKind"]=="jpeg","JPEG did not enter controller");c.setPostAdjustment("exposure",.2);c.flushEdits();
            c.selectAllPhotos(false);c.setItemSelected(0,true);c.setItemSelected(3,true);c.createSelectedPair();idle(c);
            check(c.frame()->settings.pair.active&&!c.previewImage().isNull(),"JPEG/PNG pair did not render");
            check(c.prepareToClose(),"edit flush failed");
        }
        {
            PreviewController c(native(QString::fromLocal8Bit(argv[1])),nullptr,{},edits);until([&]{return c.ready()||c.failed();});check(c.ready(),c.status());c.openRaw(QUrl::fromLocalFile(png));idle(c);
            check(std::abs(c.postAdjustments()["exposure"].toDouble()-.4)<1e-9&&c.previewImage().size()==QSize(52,76),"PNG edits did not restore after reopening");
            c.openRaw(QUrl::fromLocalFile(jpg));idle(c);check(std::abs(c.postAdjustments()["exposure"].toDouble()-.2)<1e-9,"JPEG edits did not restore");
        }
        check(QCryptographicHash::hash(read(png),QCryptographicHash::Sha256)==originalHash,"source PNG changed");
        std::cout<<"Raster input: "<<checks<<" checks; JPEG/PNG8/PNG16, sRGB/P3 ICC, EXIF, alpha, rejection, Vulkan, edits, reopen and TIFF/PNG/JPEG passed; max gray linear error="<<maxGreyError<<"; owned evidence="<<owned.path().toStdString()<<'\n';return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
