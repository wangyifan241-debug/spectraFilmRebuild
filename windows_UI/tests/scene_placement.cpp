// Real controller, Vulkan meter and fit, with only owned RAWs and edit store.
#include "PreviewController.hpp"
#include "SyntheticDng.hpp"
#include <QCoreApplication>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QTimer>
#include <QImageReader>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <iostream>
#include <cmath>
#include <stdexcept>

namespace desktop=spk::desktop;
namespace {
int checks=0;
void check(bool value,const char* message) {++checks;if(!value)throw std::runtime_error(message);}
void idle(PreviewController& controller) {
    QEventLoop loop;QTimer poll,deadline;poll.setInterval(10);deadline.setSingleShot(true);
    QObject::connect(&poll,&QTimer::timeout,&loop,[&]{if(!controller.busy()&&!controller.dirty())loop.quit();});
    QObject::connect(&deadline,&QTimer::timeout,&loop,&QEventLoop::quit);
    poll.start();deadline.start(45000);loop.exec();
    if(controller.busy()||controller.dirty()||controller.failed())
        throw std::runtime_error("Scene workflow: "+controller.status().toStdString());
}
QByteArray bytes(const QString& path) {QFile file(path);if(!file.open(QIODevice::ReadOnly))throw std::runtime_error("Cannot read owned file: "+path.toStdString());return file.readAll();}
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    try {
        check(argc==2,"Expected bundled resource directory");
        const std::filesystem::path resources(app.arguments()[1].toStdWString());
        QTemporaryDir owned;check(owned.isValid(),"Cannot create owned fixture directory");
        const auto source=owned.filePath("owned.dng"),second=owned.filePath("second.dng"),edits=owned.filePath("edits");
        writeSyntheticDng(source);writeSyntheticDng(second,160,96);
        const auto rawBytes=bytes(source);
        PreviewController c(resources,nullptr,{},edits);idle(c);
        c.openRaw(QUrl::fromLocalFile(source));idle(c);
        c.setAdjustment("grainActive",false);c.setAdjustment("glareActive",false);idle(c);
        const auto initial=c.frame();const auto initialImage=c.previewImage();const auto original=c.originalImage();
        c.flushEdits();desktop::editing::Store store(edits);
        const auto storePath=store.filePath(QUrl::fromLocalFile(source));const auto initialEdits=bytes(storePath);
        const auto couldUndo=c.canUndo();
        c.measureScene();idle(c);
        const auto measured=c.sceneReport(),scene=measured.value("scene").toMap();
        check(measured.value("available").toBool(),"Measurement is not available for its own frame");
        check(scene.value("samples").toDouble()>0,"Scene meter returned no real samples");
        const auto hist=scene.value("histogram").toMap().value("fractions").toList();
        double sum=0;for(const auto& b:hist)sum+=b.toDouble();
        check(hist.size()>16&&std::abs(sum-1)<1e-5,"Measured histogram is empty or not normalized");
        const auto medium=measured.value("medium").toMap();
        check(medium.value("highlight_ev").toDouble()>medium.value("shadow_ev").toDouble(),"Measured medium range is invalid");
        check(std::abs(measured.value("below").toDouble()+measured.value("within").toDouble()+measured.value("above").toDouble()-sum)<1e-6,"Latitude shares do not add up to measured pixels");
        const auto separation=measured.value("separation").toList();double peak=0;
        for(const auto& sample:separation){const double v=sample.toMap().value("strength").toDouble();check(v>=0&&v<=1,"Invalid neutral response separation");peak=std::max(peak,v);}
        check(separation.size()>100&&peak==1,"Response band was not derived from measured ramp");
        check(c.frame()==initial&&c.previewImage()==initialImage&&c.canUndo()==couldUndo,"Measuring edited the canvas or history");
        c.flushEdits();check(bytes(storePath)==initialEdits,"Measuring persisted an edit");
        const double minimum=measured.value("fit").toMap().value("highlight").toMap().value("minimum_pull_back").toDouble();
        const double h=std::max(.75,minimum+.75);check(h<=8,"Owned scene outside supported fit range");
        c.fitScene(h,0);idle(c);
        if(!c.sceneReport().value("fit").toMap().value("valid").toBool())
            throw std::runtime_error(QJsonDocument::fromVariant(c.sceneReport()).toJson().toStdString());
        const auto fitted=c.frame();const auto fittedImage=c.previewImage();
        check(fitted->settings.placement.active&&std::abs(fitted->settings.placement.highlight_pull_back-h)<1e-9,"Resolved fit was not committed to rendered settings");
        check(fittedImage!=initialImage,"Active scene mapping did not change rendered pixels");
        check(c.originalImage()==original,"Scene mapping changed the original comparison");
        check(c.sceneReport().value("available").toBool(),"Applied fit lost its own measurement");
        c.flushEdits();const auto fittedEdits=bytes(storePath);const auto fittedSettings=fitted->settings;
        c.fitScene(0,8);idle(c);
        check(!c.sceneReport().value("fit").toMap().value("valid").toBool(),"Impossible lift was accepted");
        check(c.frame()==fitted&&c.previewImage()==fittedImage&&!c.dirty(),"Refused fit changed displayed or desired settings");
        c.flushEdits();check(bytes(storePath)==fittedEdits,"Refused fit changed stored edits");
        c.fitScene(NAN,0);check(!c.busy()&&c.frame()==fitted,"Non-finite fit request was submitted");
        c.undo();idle(c);check(c.frame()->settings==initial->settings&&c.previewImage()==initialImage,"Fit undo did not restore exact baseline");
        c.redo();idle(c);check(c.frame()->settings==fittedSettings&&c.previewImage()==fittedImage,"Fit redo did not restore exact result");
        c.resetScenePlacement();idle(c);check(c.frame()->settings.placement==desktop::ScenePlacement{}&&c.previewImage()==initialImage,"Reset did not bypass scene mapping");
        c.undo();idle(c);check(c.previewImage()==fittedImage,"Reset was not undoable");
        const auto destination=owned.filePath("fitted.png");
        c.exportImage(QUrl::fromLocalFile(destination),1,95,0);idle(c);
        check(QImageReader(destination).read().convertToFormat(QImage::Format_ARGB32)==fittedImage,"Export omitted or changed scene placement");
        c.flushEdits();
        PreviewController reopened(resources,nullptr,{},edits);idle(reopened);
        reopened.openRaw(QUrl::fromLocalFile(source));idle(reopened);
        check(reopened.frame()->settings==fittedSettings&&reopened.previewImage()==fittedImage,"Reopen lost fitted scene settings");
        check(!reopened.sceneReport().value("available").toBool(),"Reopen fabricated measured data");
        c.setAdjustment("filmExposureEv",.5);idle(c);
        check(!c.sceneReport().value("available").toBool(),"Old measurement survived a changed input exposure");
        c.measureScene();idle(c);check(c.sceneReport().value("available").toBool(),"Updated settings could not be remeasured");
        c.fitScene(0,0,true);idle(c);
        check(c.sceneReport().value("fit").toMap().value("valid").toBool(),"Automatic suggestion failed for owned scene");
        c.openRaw(QUrl::fromLocalFile(second));idle(c);
        check(!c.sceneReport().value("available").toBool(),"Measurement was retargeted to another image");
        check(bytes(source)==rawBytes,"Scene workflow changed original RAW bytes");

        // Snapshot identity must survive both a newer live session and changed
        // bytes at the same path. The meter must not reread or retarget it.
        desktop::PreviewHost host(resources,true);
        auto a=host.open_raw(std::filesystem::path(source.toStdWString()),desktop::DecodeMode::compatible16);
        const auto reportA=host.scene_latitude(a);
        auto b=host.open_raw(std::filesystem::path(second.toStdWString()),desktop::DecodeMode::compatible16);
        QFile changed(source);check(changed.open(QIODevice::WriteOnly|QIODevice::Truncate),"Cannot replace owned source fixture");changed.write("changed owned bytes");changed.close();
        check(host.scene_latitude(a)==reportA,"Measurement reread changed source or used newer live decode");
        check(host.current()==b,"Measuring retired snapshot replaced live frame");
        auto settings=b->settings;settings.film_exposure_ev=.1;
        check(host.render(settings)->source==b->source,"Editing after retired measurement changed live source");
        std::cout<<"Scene placement: "<<checks<<" checks; actual Vulkan measurement, fit/refusal, history, v4 persistence, export and immutable source passed\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
