// Optional large-image probe. This is the Qt export subchain, NOT a camera RAW
// decode or Vulkan VRAM benchmark. Run each size in a separate native process.
#include "FrameOutput.hpp"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QColorSpace>
#include <QFileInfo>
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    try {
        if(argc!=4)throw std::runtime_error("width height resources required");
        const int w=QString::fromLocal8Bit(argv[1]).toInt(),h=QString::fromLocal8Bit(argv[2]).toInt();
        if(w<=0||h<=0||int64_t(w)*h>65000000)throw std::runtime_error("invalid probe size");
        QImage source(w,h,QImage::Format_RGBA64);if(source.isNull())throw std::runtime_error("allocation failed");
        source.setColorSpace(QColorSpace::SRgb);source.fill(QColor::fromRgbF(.46,.38,.28));
        spk::desktop::output::ExportOptions options;options.develop.exposure=.5;
        spk::desktop::output::Geometry geometry;geometry.crop={.05,.05,.9,.9};geometry.quarterTurns=1;geometry.flipHorizontal=true;geometry.straightenDegrees=2;
        QTemporaryDir owned;if(!owned.isValid())throw std::runtime_error("probe directory failed");
        const auto path=owned.filePath("probe.tif");
        spk::desktop::output::exportImageFile(source,std::filesystem::path(path.toStdWString()),geometry,options,std::filesystem::path(QString::fromLocal8Bit(argv[3]).toStdWString()));
        if(QFileInfo(path).size()<int64_t(w)*h*3)throw std::runtime_error("TIFF unexpectedly small");
#ifdef _WIN32
        PROCESS_MEMORY_COUNTERS_EX counters{};counters.cb=sizeof(counters);
        if(!GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),sizeof(counters)))throw std::runtime_error("peak unavailable");
        std::cout<<"{\"width\":"<<w<<",\"height\":"<<h<<",\"peak_working_set_bytes\":"<<counters.PeakWorkingSetSize<<",\"qt_export_only\":true,\"camera_raw\":false}\n";
#endif
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
