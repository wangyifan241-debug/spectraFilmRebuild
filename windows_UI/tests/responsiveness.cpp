// Deterministic thread-ownership gate, not a machine-speed benchmark.
#include "PreviewController.hpp"
#include "SyntheticDng.hpp"
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QEventLoop>
#include <QTimer>
#include <QElapsedTimer>
#include <QThread>
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

int main(int argc,char** argv) {
    QGuiApplication app(argc,argv);
    try {
        if(argc!=2)throw std::runtime_error("Expected resources directory");
        QTemporaryDir owned;
        if(!owned.isValid())throw std::runtime_error("Cannot own responsiveness fixtures");
        const auto raw=owned.filePath("owned.dng");writeSyntheticDng(raw,256,192);
        const auto ui=std::this_thread::get_id();
        std::atomic<int> beats{0},during{0};std::atomic<bool> offThread{false};
        std::atomic<bool> holdView{false},viewStarted{false},releaseView{false},failView{false};
        std::atomic<bool> viewOffThread{false};
        QTimer heartbeat;heartbeat.setInterval(5);
        QObject::connect(&heartbeat,&QTimer::timeout,&app,[&]{++beats;});heartbeat.start();
        PreviewController c(std::filesystem::path(QString::fromLocal8Bit(argv[1]).toStdWString()),nullptr,
            [&](const auto& frame) {
                offThread=std::this_thread::get_id()!=ui;
                const auto before=beats.load();
                std::this_thread::sleep_for(std::chrono::milliseconds(120));
                during=beats.load()-before;
                return PreviewController::imageFromFrame(frame);
            },owned.filePath("edits"),[&](const auto& frame,const QImage&,const auto& geometry,const auto& develop){
                viewOffThread=std::this_thread::get_id()!=ui;
                if(holdView){viewStarted=true;while(!releaseView)std::this_thread::sleep_for(std::chrono::milliseconds(1));}
                if(failView.exchange(false))throw std::runtime_error("injected view conversion failure");
                return spk::desktop::output::displayFrame(frame,geometry,develop);
            });
        struct ReleaseOnExit{std::atomic<bool>& release;~ReleaseOnExit(){release=true;}} unblock{releaseView};
        QEventLoop loop;QTimer deadline;deadline.setSingleShot(true);
        QObject::connect(&deadline,&QTimer::timeout,&loop,&QEventLoop::quit);
        QObject::connect(&c,&PreviewController::initialized,&loop,[&](bool ok){if(ok)c.openRaw(QUrl::fromLocalFile(raw));else loop.quit();});
        QObject::connect(&c,&PreviewController::operationFinished,&loop,[&](const QString& operation,bool){if(operation=="open")loop.quit();});
        deadline.start(60000);loop.exec();
        if(!c.hasFrame()||c.failed())throw std::runtime_error("Owned RAW failed to open");
        if(!offThread)throw std::runtime_error("Presentation conversion still executes on the GUI thread");
        if(during<1)throw std::runtime_error("GUI event loop did not advance during presentation work");
        auto wait=[&](auto predicate){QElapsedTimer time;time.start();while(!predicate()&&time.elapsed()<30000){QCoreApplication::processEvents();QThread::msleep(1);}if(!predicate())throw std::runtime_error("view deadline exceeded");};
        const auto held=c.frame();holdView=true;
        c.setPostAdjustment("exposure",.2);wait([&]{return viewStarted.load();});
        if(!c.busy()||!c.postEditable()||!viewOffThread)throw std::runtime_error("post conversion blocked GUI or disabled coalesced edits");
        bool eventDelivered=false;QTimer::singleShot(0,&app,[&]{eventDelivered=true;});wait([&]{return eventDelivered;});
        c.setPostAdjustment("exposure",.4);c.setPostAdjustment("exposure",.7);c.rotateClockwise();
        c.exportTiff(QUrl::fromLocalFile(owned.filePath("interleaved.tif")));
        holdView=false;releaseView=true;wait([&]{return !c.busy();});
        if(c.failed()||c.frame()!=held||c.previewImage().size()!=QSize(192,256))throw std::runtime_error("view/export/coalesced geometry damaged state");
        spk::desktop::output::Develop expected;expected.exposure=.7;
        spk::desktop::output::Geometry geometry;geometry.quarterTurns=1;
        if(c.previewImage()!=spk::desktop::output::displayFrame(held,geometry,expected))throw std::runtime_error("obsolete view replaced latest pixels");
        const auto beforeFailure=c.previewImage();failView=true;c.setPostAdjustment("exposure",.9);wait([&]{return !c.busy();});
        if(!c.failed()||c.previewImage()!=beforeFailure||!c.status().contains("injected view"))throw std::runtime_error("view failure lost prior image or diagnostic");
        c.setPostAdjustment("exposure",.7);wait([&]{return !c.busy();});
        if(c.failed()||c.originalImage().isNull()||c.uncroppedImage().isNull())throw std::runtime_error("view worker did not recover with cached comparison surfaces");
        c.setPostAdjustment("exposure",.8);c.exportImage(QUrl::fromLocalFile(owned.filePath("immediate.png")),2,95,0);
        wait([&]{return !c.busy();});expected.exposure=.8;
        if(c.failed()||c.previewImage()!=spk::desktop::output::displayFrame(held,geometry,expected))throw std::runtime_error("immediate export retired the pending view");
        bool foundCine=false,stillCorrect=false;
        for(const auto& value:c.films()) {const auto row=value.toMap();if(row["id"]=="kodak_vision3_500t")foundCine=row["cine"].toBool();if(row["id"]=="kodak_portra_400")stillCorrect=!row["cine"].toBool();}
        if(!foundCine||!stillCorrect)throw std::runtime_error("CINE badge metadata lost or assigned to a still stock");
        std::cout<<"Presentation/view worker gates passed; GUI event delivered while post work was held; coalesced edits, geometry, export, failure/recovery and cached surfaces correct. Engine presentation heartbeats during 120 ms work: "<<during<<"\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
