// A real QML scene with an owned, asymmetric Bayer DNG. No external fixture
// skips and no writes beside the user's photographs. GUI display is offscreen;
// film simulation still runs through Vulkan. Monitor calibration is not tested.
#include "SyntheticDng.hpp"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QProcess>
#include <QProcessEnvironment>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QImage>
#include <iostream>
#include <stdexcept>

int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    try {
        if(argc!=2)throw std::runtime_error("Expected Qt application path");
        QTemporaryDir owned;if(!owned.isValid())throw std::runtime_error("Cannot create owned QML fixture folder");
        const auto raw=owned.filePath("owned.dng"),bad=owned.filePath("bad.NEF"),out=owned.filePath("report");
        // Both axes exceed the canvas so every 100% interior probe lands on
        // photograph pixels, not the letterbox of a very wide strip fixture.
        writeSyntheticDng(raw,1920,1280);
        QFile corrupt(bad);
        if(!corrupt.open(QIODevice::WriteOnly|QIODevice::NewOnly)||corrupt.write("Owned corrupt RAW fixture")<0)
            throw std::runtime_error("Cannot write owned rejection fixture");
        corrupt.close();
        QProcess child;auto environment=QProcessEnvironment::systemEnvironment();
        environment.insert("QT_QPA_PLATFORM","offscreen");environment.insert("QT_QUICK_BACKEND","software");
        child.setProcessEnvironment(environment);
        child.start(QString::fromLocal8Bit(argv[1]),{"--open",raw,"--reject",bad,"--self-test",out});
        if(!child.waitForStarted(10000)||!child.waitForFinished(180000)) {
            child.kill();child.waitForFinished();throw std::runtime_error("QML child did not complete");
        }
        const auto errors=child.readAllStandardError();
        if(!errors.isEmpty())std::cerr<<errors.constData();
        QFile file(out+"/report.json");
        if(!file.open(QIODevice::ReadOnly))throw std::runtime_error("No QML report (possible crash)");
        const auto report=QJsonDocument::fromJson(file.readAll()).object();
        if(child.exitStatus()!=QProcess::NormalExit||child.exitCode()!=0||!report["passed"].toBool()||!errors.isEmpty())
            throw std::runtime_error("QML workflow failed: "+report["error"].toString().toStdString());
        const QImage original(out+"/original.png"),split(out+"/compare-split.png"),post(out+"/post-develop.png");
        if(original.isNull()||split.isNull()||post.isNull()||original.size()!=split.size()||original==split)
            throw std::runtime_error("Original/split scene captures missing or identical");
        std::cout<<"QML workflow: "<<report.size()<<" report fields; actual scene captures, original/split/Space, curves, geometry, native export and isolated persistence passed\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
