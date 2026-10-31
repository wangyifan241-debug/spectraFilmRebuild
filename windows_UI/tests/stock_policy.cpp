// The real controller and bundled catalog, with no RAW fixture or user store.
#include "PreviewController.hpp"
#include "SyntheticDng.hpp"
#include <QCoreApplication>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QTimer>
#include <QImageReader>
#include <QColorSpace>
#include <QDir>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>
#include <algorithm>
#include <memory>
#include <vector>
#include <stdexcept>
#include <cstring>
namespace output=spk::desktop::output;

namespace {
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
int stockIndex(const QVariantList& stocks, const QString& id) {
    for (int i=0;i<stocks.size();++i)
        if (stocks[i].toMap().value("id").toString()==id) return i;
    throw std::runtime_error("Required bundled stock is missing");
}
void renderSilverChart(spk_engine* engine, const std::string& film) {
    constexpr int width=128,height=32;
    std::vector<float> pixels(width*height*3);
    for(int y=0;y<height;++y)for(int x=0;x<width;++x) {
        const float value=0.005f+0.65f*float(x)/float(width-1);
        pixels[(y*width+x)*3]=value;
        pixels[(y*width+x)*3+1]=value*0.8f;
        pixels[(y*width+x)*3+2]=value*0.6f;
    }
    const std::string params="{\"film_stock\":\""+film+"\",\"print_stock\":\"ilford_multigrade_iv_rc\","
        "\"input_color_space\":\"ProPhoto RGB\",\"input_cctf_decoding\":false,"
        "\"output_color_space\":\"sRGB\",\"output_cctf_encoding\":true,\"auto_exposure\":false,"
        "\"grain_active\":false,\"grain_sublayers_active\":false,\"glare_active\":false,"
        "\"halation_active\":false,\"dir_couplers_active\":false,"
        "\"c_filter_neutral\":0,\"m_filter_neutral\":0,\"y_filter_neutral\":68}";
    spk_image image{pixels.data(),width,height,3};
    std::unique_ptr<spk_session,decltype(&spk_session_release)> session(
        spk_open(engine,&image,params.c_str(),nullptr),spk_session_release);
    if(!session)throw std::runtime_error(spk_last_error(engine));
    spk_result result{};
    if(spk_render(session.get(),"full",&result)!=SPK_OK)throw std::runtime_error(spk_last_error(engine));
    // Own the ABI result even when a pixel assertion throws.
    std::unique_ptr<spk_result,decltype(&spk_result_free)> held(&result,spk_result_free);
    check(result.width==width&&result.height==height,"Silver render changed chart size");
    int low=65535,high=0;
    for(unsigned y=0;y<result.height;++y)for(unsigned x=0;x<result.width;++x) {
        const auto* p=result.rgba16+(y*result.row_stride_px+x)*4;
        const auto range=std::minmax({p[0],p[1],p[2]});
        check(int(range.second)-int(range.first)<132,"Silver print has a colour cast");
        check(p[3]==65535,"Silver render is not opaque");
        low=std::min(low,int(p[1]));high=std::max(high,int(p[1]));
    }
    check(high-low>1024,"Silver render is flat or empty");
}
void renderCineStill(spk_engine* engine) {
    constexpr unsigned width=128,height=32;
    std::vector<float> pixels(width*height*3,0.05f);
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
        const float v=x<64?0.005f+float(x)*0.01f:2.0f+float(x-64)*0.1f;
        pixels[(y*width+x)*3]=v;
        pixels[(y*width+x)*3+1]=v*0.7f;
        pixels[(y*width+x)*3+2]=v*0.4f;
    }
    // Halation intentionally stays enabled. The silver tests disable it and
    // could never catch a missing kernel used only by no-backing films.
    spk_image image{pixels.data(),width,height,3};
    const char* params=R"({"film_stock":"kodak_portra_400","print_stock":"kodak_portra_endura",
        "input_color_space":"ProPhoto RGB","input_cctf_decoding":false,
        "output_color_space":"sRGB","grain_active":false,"glare_active":false,
        "auto_exposure":false,"striped":true})";
    std::unique_ptr<spk_session,decltype(&spk_session_release)> session(
        spk_open(engine,&image,params,nullptr),spk_session_release);
    if(!session)throw std::runtime_error(spk_last_error(engine));
    for(const char* id : {"cinestill_800t","kodak_portra_400","cinestill_800t"}) {
        const std::string delta="{\"film_stock\":\""+std::string(id)+"\"}";
        if(spk_set_params(session.get(),delta.c_str(),nullptr)!=SPK_OK)
            throw std::runtime_error(spk_last_error(engine));
        spk_result result{};
        std::unique_ptr<spk_result,decltype(&spk_result_free)> held(&result,spk_result_free);
        if(spk_reprint(session.get(),"full",&result)!=SPK_OK)
            throw std::runtime_error(std::string(id)+": "+spk_last_error(engine));
        check(result.width==width&&result.height==height,"CineStill render changed dimensions");
        unsigned low=65535,high=0;
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
            const auto* p=result.rgba16+(y*result.row_stride_px+x)*4;
            check(p[3]==65535,"CineStill render has invalid alpha");
            low=std::min(low,unsigned(p[1]));high=std::max(high,unsigned(p[1]));
        }
        check(high-low>1024,"CineStill render is blank or flat");
    }
}
void renderStripAgreement(spk_engine* engine) {
    constexpr unsigned width=193,height=389;
    std::vector<float> input(width*height*3);
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)for(unsigned c=0;c<3;++c)
        input[(y*width+x)*3+c]=0.001f+float((x*17+y*31+c*71)%1024)/173.0f;
    spk_image image{input.data(),width,height,3};
    const auto render=[&](const char* film,bool effects,int rows) {
        const std::string params=std::string("{\"film_stock\":\"")+film+"\",\"print_stock\":\"kodak_portra_endura\","
            "\"input_color_space\":\"ProPhoto RGB\",\"input_cctf_decoding\":false,"
            "\"output_color_space\":\"sRGB\",\"output_cctf_encoding\":true,\"auto_exposure\":false,"
            "\"grain_active\":"+(effects?"true":"false")+",\"glare_active\":"+(effects?"true":"false")+","
            "\"striped\":"+(rows?"true":"false")+",\"strip_rows\":"+std::to_string(rows)+"}";
        std::unique_ptr<spk_session,decltype(&spk_session_release)> session(
            spk_open(engine,&image,params.c_str(),nullptr),spk_session_release);
        if(!session)throw std::runtime_error(spk_last_error(engine));
        spk_result result{};
        std::unique_ptr<spk_result,decltype(&spk_result_free)> held(&result,spk_result_free);
        if(spk_render(session.get(),"full",&result)!=SPK_OK)throw std::runtime_error(spk_last_error(engine));
        check(result.width==width&&result.height==height,"Strip agreement changed the frame size");
        std::vector<std::uint16_t> output(width*height*4);
        for(unsigned y=0;y<height;++y)
            std::copy_n(result.rgba16+y*result.row_stride_px*4,width*4,output.data()+y*width*4);
        return output;
    };
    int worst=0;
    for(const auto* film: {"kodak_portra_400","cinestill_800t"})for(bool effects:{false,true}) {
        // Separate sessions begin with the same native RNG state; both use
        // their first render, so enabled grain AND glare are comparable.
        const auto reference=render(film,effects,0);
        for(int rows:{128,129}) {
            const auto striped=render(film,effects,rows);
            for(std::size_t i=0;i<reference.size();++i)
                worst=std::max(worst,std::abs(int(reference[i])-int(striped[i])));
            // Existing float32 render bound (3e-5), plus one integer
            // quantization step, permits at most two RGBA16 counts.
            check(worst<=2,"Strip rendering differs from monolithic render beyond the existing numerical bar");
        }
    }
    std::cout<<"Strip agreement: Portra/CineStill, effects off/on, rows 128/129; max RGBA16 error "<<worst<<'\n';
}
void waitUntilIdle(PreviewController& controller,int timeoutMs=30000) {
    QEventLoop loop;QTimer poll,deadline;poll.setInterval(10);deadline.setSingleShot(true);
    QObject::connect(&poll,&QTimer::timeout,&loop,[&]{if(!controller.busy()&&!controller.dirty())loop.quit();});
    QObject::connect(&deadline,&QTimer::timeout,&loop,&QEventLoop::quit);
    poll.start();deadline.start(timeoutMs);loop.exec();
    if(controller.busy()||controller.dirty()||controller.failed())
        throw std::runtime_error("Controller workflow: "+controller.status().toStdString());
}
void controllerDevelop(PreviewController& controller,const QString& folder,int cine,const std::filesystem::path& resources) {
    const QString input=folder+"/owned.dng";writeSyntheticDng(input);
    controller.setAdjustment("grainActive",false);controller.setAdjustment("glareActive",false);
    controller.openRaw(QUrl::fromLocalFile(input));waitUntilIdle(controller);
    check(controller.hasFrame(),"Synthetic DNG did not reach the canvas");
    controller.setFilmIndex(cine);waitUntilIdle(controller);
    check(controller.frame()->settings.film_stock=="cinestill_800t","CineStill selection rolled back");
    const auto frame=controller.frame();const auto before=controller.previewImage();
    check(frame->prophoto(),"Qt controller is not rendering in ProPhoto working space");
    const auto original=controller.originalImage();
    check(!original.isNull()&&original.size()==before.size()&&original!=before,"Original comparison is absent or is the processed film");
    controller.setPostAdjustment("exposure",0.7);
    check(controller.frame()==frame&&!controller.busy()&&!controller.dirty(),"Post-develop re-rendered film");
    const auto adjusted=controller.previewImage();
    check(adjusted!=before,"Post exposure did not change the canvas");
    controller.undo();check(controller.previewImage()==before,"Post exposure undo did not restore pixels");
    controller.redo();check(controller.previewImage()==adjusted,"Post exposure redo did not restore pixels");
    controller.setPostAdjustment("temperature",12);controller.setPostAdjustment("tint",-7);
    controller.setPostAdjustment("contrast",16);controller.setPostAdjustment("brightness",-5);
    controller.setPostAdjustment("saturation",-15);controller.setPostAdjustment("highlights",-22);
    controller.setPostAdjustment("shadows",17);controller.setPostAdjustment("blackPoint",2);
    controller.setPostAdjustment("whitePoint",3);
    controller.setPostAdjustment("zone0Hue",30);controller.setPostAdjustment("zone0Saturation",.17);
    controller.setPostAdjustment("zone1Luminance",.2);controller.setPostAdjustment("zone2Saturation",.12);
    controller.setPostAdjustment("zone3Hue",245);controller.setPostAdjustment("zone3Saturation",.21);
    controller.setPostAdjustment("curve0",QVariantList{QVariantMap{{"x",0},{"y",0}},QVariantMap{{"x",.5},{"y",.62}},QVariantMap{{"x",1},{"y",1}}});
    controller.setPostAdjustment("vignetteAmount",-40);controller.setPostAdjustment("vignetteMidpoint",35);
    check(controller.originalImage()==original,"Scan adjustments changed the original comparison pixels");
    const auto allAdjusted=controller.previewImage();
    controller.setPostAdjustment("enabled",false);
    check(controller.previewImage()==before,"Disabling post-develop did not bypass all adjustments");
    controller.setPostAdjustment("enabled",true);
    check(controller.previewImage()==allAdjusted,"Re-enabling post-develop lost parameters");
    const auto savedPost=controller.postAdjustments();
    controller.resetPostAdjustments();check(controller.previewImage()==before,"Post-develop reset did not restore simulation");
    controller.undo();check(controller.previewImage()==allAdjusted,"Post-develop reset was not undoable");
    controller.setCropRect({0.25,0.25,0.5,0.5});controller.rotateClockwise();
    const output::Geometry originalGeometry{{.25,.25,.5,.5},1};
    check(controller.originalImage()==output::displayImage(original,originalGeometry),"Original comparison crop and turn differ from processed geometry");
    const auto shown=controller.previewImage();
    const QString destination=folder+"/post-develop.png";
    controller.exportImage(QUrl::fromLocalFile(destination),1,95,0);waitUntilIdle(controller);
    QImageReader reader(destination);const auto exported=reader.read();
    check(!exported.isNull()&&exported.convertToFormat(QImage::Format_ARGB32)==shown,
        "Post-develop export disagrees with cropped and rotated canvas");
    controller.flushEdits();
    if(!controller.persistenceWarning().isEmpty())throw std::runtime_error(controller.persistenceWarning().toStdString());
    controller.selectAllPhotos(true);
    const QString batchFolder=folder+"/batch";check(QDir().mkpath(batchFolder),"Cannot create owned batch folder");
    controller.exportBatch(QUrl::fromLocalFile(batchFolder),1,95,0);waitUntilIdle(controller);
    QImageReader batchReader(batchFolder+"/owned.png");
    check(batchReader.read().convertToFormat(QImage::Format_ARGB32)==shown,"Batch export omitted post-develop");
    PreviewController reopened(resources,nullptr,{},folder+"/edits");waitUntilIdle(reopened);
    reopened.openRaw(QUrl::fromLocalFile(input));waitUntilIdle(reopened);
    check(reopened.postAdjustments()==savedPost&&reopened.quarterTurns()==1,
        "Reopening omitted persisted post-develop or geometry");
    check(reopened.previewImage()==shown,"Reopened canvas disagrees with the saved edit");
}
void controllerOriginalExport(PreviewController& controller,const QString& folder) {
    const QString input=folder+"/original-size.dng";
    writeSyntheticDng(input,3520,96);
    controller.openRaw(QUrl::fromLocalFile(input));waitUntilIdle(controller);
    check(controller.frame()->width()==3400,"Owned wide DNG did not exercise the preview limit");
    check(controller.metadata().value("sourceWidth").toUInt()==3520&&
          controller.metadata().value("sourceHeight").toUInt()==96,
          "Source metadata reports preview dimensions instead of decoded original dimensions");
    const auto held=controller.frame();
    const auto shown=controller.previewImage();
    const QString destination=folder+"/original-size.png";
    controller.exportImage(QUrl::fromLocalFile(destination),2,95,0);waitUntilIdle(controller);
    QImageReader reader(destination);
    check(reader.size()==QSize(3520,96),"Export saved the limited preview instead of original-size pixels");
    check(controller.frame()==held&&controller.previewImage()==shown,"Original export replaced the editing canvas");
    controller.setCropRect({0.25,0.25,0.5,0.5});controller.rotateClockwise();
    const QString cropped=folder+"/original-cropped.png";
    controller.exportImage(QUrl::fromLocalFile(cropped),2,95,0);waitUntilIdle(controller);
    QImageReader cropReader(cropped);
    check(cropReader.size()==QSize(48,1760),"Original export crop/turn uses preview-size geometry");
    controller.selectAllPhotos(false);controller.setItemSelected(controller.activeIndex(),true);
    const QString batch=folder+"/original-batch";
    check(QDir().mkpath(batch),"Cannot create original batch output");
    controller.exportBatch(QUrl::fromLocalFile(batch),2,95,0);waitUntilIdle(controller);
    QImageReader batchReader(batch+"/original-size.png");
    check(batchReader.size()==QSize(48,1760),"Batch still exports preview-size pixels");
    controller.setAdjustment("filmExposureEv",0.25);waitUntilIdle(controller);
    check(controller.frame()->settings.film_exposure_ev==0.25&&controller.frame()->width()==3400,
          "Preview session did not recover after original export");
}
void workingSnapshotOwnership(const std::filesystem::path& resources,const QString& folder) {
    const auto input=folder+"/colour-owned.dng";writeSyntheticDng(input);
    spk::desktop::FramePtr snapshot;QImage expected,original;
    output::Geometry geometry{{.25,.25,.5,.5},1};output::Develop develop;
    develop.curves[0]={{0,0},{.5,.6},{1,1}};develop.zones[2]={205,.2,0};develop.vignetteAmount=-20;
    {
        spk::desktop::PreviewHost host(resources,true);
        snapshot=host.open_raw(std::filesystem::path(input.toStdWString()),spk::desktop::DecodeMode::compatible16);
        expected=output::displayFrame(snapshot,geometry,develop);original=output::originalFrame(snapshot,geometry);
    }
    check(output::displayFrame(snapshot,geometry,develop)==expected&&output::originalFrame(snapshot,geometry)==original,
          "Working-space snapshot borrowed its transform/C_max/decode from a destroyed engine");
}
void originalSnapshotOwnership(const std::filesystem::path& resources,const QString& folder) {
    const QString source=folder+"/snapshot.dng";writeSyntheticDng(source,3520,96);
    spk::desktop::PreviewHost host(resources);
    spk::desktop::RenderSettings settings;settings.grain_active=false;settings.glare_active=false;
    const auto snapshot=host.open_raw(std::filesystem::path(source.toStdWString()),
                                     spk::desktop::DecodeMode::compatible16,settings);
    const auto before=host.render_original(snapshot);
    check(QFile::rename(source,folder+"/snapshot-moved.dng"),"Cannot move owned snapshot fixture");
    writeSyntheticDng(source,32,32);
    const auto newer=host.open_raw(std::filesystem::path(source.toStdWString()),
                                  spk::desktop::DecodeMode::compatible16,settings);
    const auto after=host.render_original(snapshot);
    check(host.current()==newer&&newer->width()==32,"Snapshot export replaced host's newer source");
    check(after->width()==3520&&after->height()==96,"Snapshot export re-read a replaced RAW file");
    for(unsigned y=0;y<after->height();++y)
        check(std::memcmp(before->pixels().rgba16+y*before->pixels().row_stride_px*4,
                          after->pixels().rgba16+y*after->pixels().row_stride_px*4,
                          std::size_t(after->width())*8)==0,
              "Original export did not retain exact decoded snapshot/settings");
}
void realOriginalGate(const std::filesystem::path& resources,const QString& input,const QString& folder) {
    QImageReader::setAllocationLimit(1024); // explicit large-fixture test only
    const bool synthetic=input=="--synthetic-24mp"||input=="--synthetic-45mp";
    check(synthetic||QFileInfo(input).isFile(),"Real RAW fixture is missing");
    check(!QFileInfo::exists(folder)&&QDir().mkdir(folder),"Real RAW output must be a NEW directory");
    QJsonObject report;QElapsedTimer timer;timer.start();
    const auto writeReport=[&] {
        QFile file(folder+"/original-export.json");
        check(file.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Cannot create original gate report");
        const auto bytes=QJsonDocument(report).toJson(QJsonDocument::Indented);
        check(file.write(bytes)==bytes.size()&&file.flush(),"Cannot finish original gate report");
    };
    try {
        const QString owned=folder+"/owned."+(synthetic?QStringLiteral("dng"):QFileInfo(input).suffix());
        if(synthetic)writeSyntheticDng(owned,input=="--synthetic-24mp"?6000:8256,
                                      input=="--synthetic-24mp"?4000:5504);
        else check(QFile::copy(input,owned),"Cannot copy real RAW into owned fixture directory");
        report.insert("synthetic",synthetic);
        QFile raw(owned);check(raw.open(QIODevice::ReadOnly),"Cannot hash owned RAW");
        QCryptographicHash hash(QCryptographicHash::Sha256);
        check(hash.addData(&raw),"Cannot finish owned RAW hash");raw.close();
        report.insert("source_sha256",QString::fromLatin1(hash.result().toHex()));
        PreviewController controller(resources,nullptr,{},folder+"/edits");waitUntilIdle(controller,180000);
        check(controller.ready(),"Real RAW controller initialization failed");
        controller.openRaw(QUrl::fromLocalFile(owned));waitUntilIdle(controller,180000);
        check(controller.hasFrame(),"Real RAW did not reach the controller");
        const auto held=controller.frame();const auto preview=controller.previewImage();
        const QSize original(int(held->source_width()),int(held->source_height()));
        check(std::max(original.width(),original.height())>3400,"Real RAW must exceed preview size");
        check(std::max(held->width(),held->height())==3400,"Real RAW preview is not limited");
        report.insert("camera",QString::fromStdString(held->metadata.make+" "+held->metadata.model));
        report.insert("synthetic",synthetic||held->metadata.make=="SpektraLab");
        report.insert("original_width",original.width());report.insert("original_height",original.height());
        report.insert("preview_width",int(held->width()));report.insert("preview_height",int(held->height()));
        report.insert("default_grain",held->settings.grain_active);
        report.insert("default_halation",held->settings.halation_active);
        report.insert("default_glare",held->settings.glare_active);
        controller.exportImage(QUrl::fromLocalFile(folder+"/native.png"),2,95,0);
        waitUntilIdle(controller,180000);
        QImageReader full(folder+"/native.png");
        check(full.size()==original,"Real RAW full export has preview dimensions");
        const auto fullPixels=full.read();check(!fullPixels.isNull(),"Cannot decode full exported PNG");
        check(fullPixels.format()==QImage::Format_RGBA64,"Original PNG lost 16-bit samples");
        check(fullPixels.colorSpace().isValid(),"Original PNG has no readable ICC profile");
        check(controller.frame()==held&&controller.previewImage()==preview,"Native export changed the canvas");
        report.insert("full_export",true);
        controller.setAdjustment("filmExposureEv",0.25);waitUntilIdle(controller,180000);
        check(controller.frame()->settings.film_exposure_ev==0.25,"Editing after native export failed");
        controller.setCropRect({0.25,0.25,0.5,0.5});controller.rotateClockwise();
        controller.setPostAdjustment("exposure",0.2);
        const auto crop=spk::desktop::output::cropPixels(original,{0.25,0.25,0.5,0.5});
        const QSize expected(crop.height(),crop.width());
        controller.selectAllPhotos(true);
        check(QDir().mkdir(folder+"/batch"),"Cannot create real RAW batch folder");
        controller.exportBatch(QUrl::fromLocalFile(folder+"/batch"),2,95,0);
        waitUntilIdle(controller,180000);
        QImageReader batch(folder+"/batch/owned.png");
        check(batch.size()==expected,"Real RAW batch crop/turn did not preserve original pixels");
        const auto batchPixels=batch.read();check(!batchPixels.isNull(),"Real RAW batch file cannot be decoded");
        report.insert("batch_crop_width",expected.width());report.insert("batch_crop_height",expected.height());
        report.insert("batch_export",true);report.insert("status","passed");
        report.insert("elapsed_ms",double(timer.elapsed()));
        report.insert("limits","Real RAW/LibRaw/Vulkan/controller/PNG gate; not live GUI, Mac parity, or measured GPU peak.");
        writeReport();
        std::cout<<(synthetic?"Synthetic RAW":"Real RAW")<<" original gate: "<<original.width()<<'x'<<original.height()
                 <<", full PNG16 + cropped/turned batch + post-export editing passed\n";
    }catch(const std::exception& error) {
        report.insert("status","failed");report.insert("error",QString::fromUtf8(error.what()));
        report.insert("elapsed_ms",double(timer.elapsed()));writeReport();throw;
    }
}
}
int main(int argc, char** argv) {
    QCoreApplication app(argc,argv);
    try {
        check(app.arguments().size()==2||app.arguments().size()==4,
              "Expected resources [real-RAW NEW-output-directory]");
        if(app.arguments().size()==4) {
            realOriginalGate(std::filesystem::path(app.arguments()[1].toStdWString()),app.arguments()[2],app.arguments()[3]);
            return 0;
        }
        QTemporaryDir store;
        check(store.isValid(),"Cannot create private edit store");
        const auto resourcePath=std::filesystem::path(app.arguments()[1].toStdWString());
        PreviewController controller(resourcePath,nullptr,{},store.path()+"/edits");
        QEventLoop loop;
        QTimer deadline;deadline.setSingleShot(true);
        QObject::connect(&controller,&PreviewController::initialized,&loop,&QEventLoop::quit);
        QObject::connect(&deadline,&QTimer::timeout,&loop,&QEventLoop::quit);
        deadline.start(30000);loop.exec();
        if(!controller.ready())throw std::runtime_error(controller.status().toStdString());
        const auto films=controller.films(), papers=controller.papers();
        const int cine=stockIndex(films,"cinestill_800t");
        check(films[cine].toMap().value("name").toString()=="CineStill 800T",
              "Derived film did not retain its own name");
        check(!films[cine].toMap().value("positive").toBool()&&
              !films[cine].toMap().value("monochrome").toBool(),"Derived film inherited wrong process");
        const int colour=stockIndex(films,"kodak_portra_400");
        const int colourPaper=stockIndex(papers,"kodak_portra_endura");
        const int silverPaper=stockIndex(papers,"ilford_multigrade_iv_rc");
        check(papers[silverPaper].toMap().value("monochrome").toBool(),"Silver paper metadata missing");
        for (const auto& id : {"kodak_tri_x_400","kodak_tmax_100","fujifilm_neopan_acros_100_ii","ilford_hp5_plus_400"}) {
            const int film=stockIndex(films,id);
            check(films[film].toMap().value("monochrome").toBool(),"Black-and-white film metadata missing");
            controller.setFilmIndex(film);
            check(controller.paperIndex()==silverPaper,"Black-and-white film did not select silver paper");
            controller.setPaperIndex(colourPaper);
            check(controller.paperIndex()==silverPaper,"Colour paper accepted for a silver negative");
            controller.setFilmIndex(colour);
            check(controller.paperIndex()==colourPaper,"Returning to colour did not select compatible paper");
            controller.setPaperIndex(silverPaper);
            check(controller.paperIndex()==colourPaper,"Silver paper accepted for colour film");
        }
        const auto resources=app.arguments()[1].toUtf8();
        std::unique_ptr<spk_engine,decltype(&spk_engine_destroy)> engine(
            spk_engine_create(resources.constData(),nullptr),spk_engine_destroy);
        check(bool(engine),"Synthetic-chart engine initialization failed");
        renderCineStill(engine.get());
        renderStripAgreement(engine.get());
        for (const auto& id : {"kodak_tri_x_400","kodak_tmax_100","fujifilm_neopan_acros_100_ii","ilford_hp5_plus_400"})
            renderSilverChart(engine.get(),id);
        controllerDevelop(controller,store.path(),cine,resourcePath);
        controllerOriginalExport(controller,store.path());
        originalSnapshotOwnership(resourcePath,store.path());
        workingSnapshotOwnership(resourcePath,store.path());
        std::cout << "Stock policy: four B&W films, compatible paper selection and cross-process refusal passed\n";
        std::cout << "Vulkan: four silver charts rendered with neutral RGB and non-flat tone response\n";
        std::cout << "Vulkan: CineStill -> Portra -> CineStill rendered with default halation enabled\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
