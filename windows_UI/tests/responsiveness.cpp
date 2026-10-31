// Deterministic thread-ownership gate, not a machine-speed benchmark.
#include "PreviewController.hpp"
#include "SyntheticDng.hpp"
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QEventLoop>
#include <QTimer>
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
        QTimer heartbeat;heartbeat.setInterval(5);
        QObject::connect(&heartbeat,&QTimer::timeout,&app,[&]{++beats;});heartbeat.start();
        PreviewController c(std::filesystem::path(QString::fromLocal8Bit(argv[1]).toStdWString()),nullptr,
            [&](const auto& frame) {
                offThread=std::this_thread::get_id()!=ui;
                const auto before=beats.load();
                std::this_thread::sleep_for(std::chrono::milliseconds(120));
                during=beats.load()-before;
                return PreviewController::imageFromFrame(frame);
            },owned.filePath("edits"));
        QEventLoop loop;QTimer deadline;deadline.setSingleShot(true);
        QObject::connect(&deadline,&QTimer::timeout,&loop,&QEventLoop::quit);
        QObject::connect(&c,&PreviewController::initialized,&loop,[&](bool ok){if(ok)c.openRaw(QUrl::fromLocalFile(raw));else loop.quit();});
        QObject::connect(&c,&PreviewController::operationFinished,&loop,[&](const QString& operation,bool){if(operation=="open")loop.quit();});
        deadline.start(60000);loop.exec();
        if(!c.hasFrame()||c.failed())throw std::runtime_error("Owned RAW failed to open");
        if(!offThread)throw std::runtime_error("Presentation conversion still executes on the GUI thread");
        if(during<1)throw std::runtime_error("GUI event loop did not advance during presentation work");
        bool foundCine=false,stillCorrect=false;
        for(const auto& value:c.films()) {const auto row=value.toMap();if(row["id"]=="kodak_vision3_500t")foundCine=row["cine"].toBool();if(row["id"]=="kodak_portra_400")stillCorrect=!row["cine"].toBool();}
        if(!foundCine||!stillCorrect)throw std::runtime_error("CINE badge metadata lost or assigned to a still stock");
        std::cout<<"Presentation worker gate passed; GUI heartbeats during injected 120 ms work: "<<during<<"; CINE/still metadata correct\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
