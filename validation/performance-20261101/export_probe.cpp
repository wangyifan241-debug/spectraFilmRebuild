#include "desktop/preview_host.hpp"
#include "EditStore.hpp"
#include "FrameOutput.hpp"
#include "image_io.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImageWriter>
#include <QColorSpace>
#include <QJsonDocument>
#include <iostream>
int main(int argc,char** argv){QCoreApplication app(argc,argv);try{
    if(argc!=6)throw std::runtime_error("resources raw tif edit-store new-output required");
    const auto path=[](const QString& p){return std::filesystem::path(p.toStdWString());};
    const auto args=app.arguments();const auto out=args[5];if(QFileInfo::exists(out)||!QDir().mkpath(out))throw std::runtime_error("output must be new");
    const auto owned=out+"/owned.ARW";if(!QFile::copy(args[2],owned))throw std::runtime_error("copy failed");
    spk::desktop::PreviewHost host(path(args[1]),true);spk::desktop::editing::Store original(args[4]);
    const auto saved=original.load(QUrl::fromLocalFile(args[2]),host.catalog());
    if(!saved.state||saved.blocked)throw std::runtime_error(saved.warning.toStdString());
    const auto& state=*saved.state;const auto& g=state.geometry;
    const auto frame=host.open_raw(path(owned),state.mode,state.settings,{g.crop.x(),g.crop.y(),g.crop.width(),g.crop.height(),g.straightenDegrees,g.quarterTurns,g.flipHorizontal,g.flipVertical});
    const auto preview=spk::desktop::output::displayFrame(frame,g,state.develop);
    const auto resolved=host.resolved_params_json();
    if(!preview.save(out+"/preview.png"))throw std::runtime_error("preview write failed");
    spkhost::Rgba8 small;std::string source,error;
    if(!spkhost::thumbnail(path(args[3]),1400,small,source,error))throw std::runtime_error(error);
    QImage image(small.rgba.data(),small.width,small.height,small.width*4,QImage::Format_RGBA8888);image.setColorSpace(QColorSpace::SRgb);
    if(!image.save(out+"/existing-tiff.png"))throw std::runtime_error("tiff readback write failed");
    if(qEnvironmentVariableIsSet("SPEKTRALAB_LIVE_ONLY")){std::cout<<"Rendered live diagnostics\n";return 0;}
    const auto full=host.render_original(frame);
    if(qEnvironmentVariableIsSet("SPEKTRALAB_DIAG_ONLY")){std::cout<<"Rendered live and full headroom diagnostics\n";return 0;}
    {const auto display=spk::desktop::output::displayFrame(full,g,state.develop);
     if(!display.scaled(1400,1400,Qt::KeepAspectRatio,Qt::SmoothTransformation).save(out+"/native-display.png"))throw std::runtime_error("native display write failed");}
    spk::desktop::output::ExportOptions options;options.develop=state.develop;
    spk::desktop::output::exportFrame(full,path(out+"/fresh.tif"),g,options,path(args[1]));
    if(!spkhost::thumbnail(path(out+"/fresh.tif"),1400,small,source,error))throw std::runtime_error(error);
    QImage fresh(small.rgba.data(),small.width,small.height,small.width*4,QImage::Format_RGBA8888);fresh.setColorSpace(QColorSpace::SRgb);
    if(!fresh.save(out+"/fresh-tiff.png"))throw std::runtime_error("fresh TIFF readback write failed");
    QFile file(out+"/resolved.json");if(!file.open(QIODevice::WriteOnly|QIODevice::NewOnly))throw std::runtime_error("report failed");file.write(resolved.c_str());
    std::cout<<"Saved owned preview "<<frame->width()<<'x'<<frame->height()<<" and existing TIFF decode "<<small.width<<'x'<<small.height<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
