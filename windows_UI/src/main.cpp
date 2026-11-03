#include "PreviewController.hpp"
#include "FrameCanvas.hpp"
#include <QCommandLineParser>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlExpression>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QStringList>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#endif

namespace {
QQuickItem* visualNamed(QQuickItem* root,const QString& name) {
    if(root->objectName()==name)return root;
    // ListView delegates are parented to its visual contentItem, not always
    // in the window's QObject ownership tree.
    for(auto* child:root->childItems())if(auto* found=visualNamed(child,name))return found;
    return nullptr;
}
QString fingerprint(const spk::desktop::FramePtr& frame) {
    if(!frame)return {};
    // Working-space frames deliberately have no unconverted BGRA8 copy.
    const auto image=PreviewController::imageFromFrame(frame);
    return QString::fromLatin1(QCryptographicHash::hash(
        QByteArrayView(reinterpret_cast<const char*>(image.constBits()),image.sizeInBytes()),
        QCryptographicHash::Sha256).toHex());
}

// This is an actual QML/controller/engine test with saved window captures. It
// deliberately makes no claim about physical monitor colour management or
// native file-dialog interaction; those need a separate interactive gate.
class SelfTest final:public QObject {
public:
    SelfTest(PreviewController& controller,QQuickWindow& window,QString input,QString reject,
             QString output,std::shared_ptr<bool> failDisplay)
        :controller_(controller),window_(window),input_(std::move(input)),reject_(std::move(reject)),
         output_(std::move(output)),fail_display_(std::move(failDisplay)) {
        canvas_=window_.findChild<FrameCanvas*>("frameCanvas");
        QObject::connect(&controller_,&PreviewController::initialized,this,[this](bool ok){
            if(!require(ok,"engine_init"))return;
            if(!require(canvas_!=nullptr,"canvas_found"))return;
            controller_.openRaw(QUrl::fromLocalFile(input_));
        });
        QObject::connect(&controller_,&PreviewController::operationFinished,this,
            [this](const QString& operation,bool ok){++operations_finished_;step(operation,ok);});
        QObject::connect(&controller_,&PreviewController::frameChanged,this,[this]{++frame_publications_;});
        // The native gate also exercises multi-megapixel camera RAW, several
        // full-size exports and paired frames. This is a correctness deadline,
        // not an interaction-speed threshold; the owned 2 MP CI gate stays 180s.
        QTimer::singleShot(QGuiApplication::platformName()=="windows"?360000:180000,this,[this]{if(!done_)finish(false,"timeout");});
    }
private:
    bool require(bool value,const QString& name) {
        checks_[name]=value;if(!value){finish(false,name+": "+controller_.status());return false;}return true;
    }
    void later(std::function<void()> action,int delayMs=180){QTimer::singleShot(delayMs,this,[this,action=std::move(action)]{if(!done_)action();});}
    void whenIdle(std::function<void()> action,unsigned attempts=0){
        if(controller_.busy()||controller_.dirty()){
            if(attempts>=1500){require(false,"presentation_settle_timeout");return;}
            later([this,action=std::move(action),attempts]{whenIdle(action,attempts+1);},20);
        }else later(std::move(action),20); // allow the scene graph to draw the publication
    }
    int filmIndex(const QString& id) const {
        const auto films=controller_.films();
        for(int i=0;i<films.size();++i)if(films[i].toMap().value("id").toString()==id)return i;
        return -1;
    }
    bool exposureIs(double ev) const {
        return std::abs(controller_.exposureEv()-ev)<1e-12&&
            std::abs(controller_.frame()->settings.print_exposure-std::exp2(-ev))<1e-12;
    }
    void checkpoint() {held_=controller_.frame();saved_publications_=frame_publications_;saved_operations_=operations_finished_;}
    void startSlides() {
        remembered_paper_=controller_.paperIndex();remembered_ev_=controller_.exposureEv();
        negative_film_=controller_.filmIndex();slide_index_=0;stage_=15;
        selectSlide();
    }
    void selectSlide() {
        const int index=filmIndex(slide_ids_[slide_index_]);
        if(!require(index>=0,"slide_in_catalog_"+slide_ids_[slide_index_]))return;
        controller_.setFilmIndex(index);
    }
    bool capture(const QString& name) {
        capture_=window_.grabWindow();
        return require(!capture_.isNull()&&capture_.save(output_+"/"+name+".png"),"capture_"+name);
    }
    void step(const QString& operation,bool ok) {
        if(done_)return;
        if(stage_==0) {
            if(!require(operation=="open"&&ok&&controller_.hasFrame(),"open_raw"))return;
            original_=controller_.frame();original_hash_=fingerprint(original_);
            checks_["width"]=int(original_->width());checks_["height"]=int(original_->height());
            checks_["film_count"]=controller_.films().size();checks_["paper_count"]=controller_.papers().size();
            stage_=1;
            later([this]{
                bool cine=false,still=false;
                for(const auto& value:controller_.films()) {const auto row=value.toMap();if(row["id"]=="kodak_vision3_500t")cine=row["cine"].toBool();if(row["id"]=="kodak_portra_400")still=!row["cine"].toBool();}
                if(!require(cine&&still,"stock_cine_metadata_correct"))return;
                if(!require(!QGuiApplication::windowIcon().isNull()&&!window_.icon().isNull(),"film_window_icon_loaded"))return;
                auto* quick=window_.findChild<QObject*>("filmQuickSelect");
                if(!require(quick&&quick->property("currentIndex").toInt()==controller_.filmIndex()&&quick->property("displayText").toString()==controller_.films()[controller_.filmIndex()].toMap()["name"].toString(),"quick_picker_matches_selected_stock"))return;
                if(QGuiApplication::platformName()=="windows") {
                    checks_["native_caption_diagnostics"]=QJsonObject::fromVariantMap(window_.property("nativeCaptionDiagnostics").toMap());
                    if(!require(window_.property("nativeDarkCaptionApplied").toBool(),"native_dark_caption_applied"))return;
                }
                if(!require(visualNamed(window_.contentItem(),"cineBadge")!=nullptr,"stock_cine_badge_instantiated"))return;
                if(!capture("fit"))return;
                if(!require(canvas_->fit()&&canvas_->image().cacheKey()==controller_.previewImage().cacheKey(),"canvas_matches_published_frame"))return;
                canvas_->resetView(false);
                later([this]{
                    const auto rect=canvas_->imageRect();
                    const auto ratio=window_.effectiveDevicePixelRatio();
                    if(!require(std::abs(rect.width()*ratio-original_->width())<0.01&&std::abs(canvas_->scalePercent()-100)<1e-9,"physical_100_percent"))return;
                    if(!capture("100-percent"))return;
                    // Interior canvas samples prove the texture is displaying
                    // this frame at one physical pixel per source pixel.
                    const auto origin=canvas_->mapToScene(QPointF());
                    bool exact=true;int samples=0;
                    for(double fx:{0.3,0.5,0.7})for(double fy:{0.3,0.5,0.7}) {
                        const int x=int(std::floor((origin.x()+canvas_->width()*fx)*ratio));
                        const int y=int(std::floor((origin.y()+canvas_->height()*fy)*ratio));
                        const int sx=int(std::floor(x+0.5-(origin.x()+rect.x())*ratio));
                        const int sy=int(std::floor(y+0.5-(origin.y()+rect.y())*ratio));
                        if(sx<0||sy<0||sx>=canvas_->image().width()||sy>=canvas_->image().height())continue;
                        exact=exact&&capture_.pixel(x,y)==canvas_->image().pixel(sx,sy);++samples;
                    }
                    if(!require(exact&&samples==9,"100_percent_source_pixels_exact"))return;
                    canvas_->panBy(1e9,1e9);const auto edge=canvas_->imageRect();
                    // Probe one PHYSICAL pixel. At native Windows DPI a
                    // logical pixel need not be an integer physical pixel;
                    // the 100% origin deliberately snaps to the latter grid.
                    const double step=1/ratio;checks_["device_pixel_ratio"]=ratio;
                    checks_["pan_probe_step_logical"]=step;
                    canvas_->panBy(-step,-step);const auto back=canvas_->imageRect();
                    if(!require((edge.width()<=canvas_->width()||std::abs(back.x()-edge.x()+step)<0.000001)&&
                                (edge.height()<=canvas_->height()||std::abs(back.y()-edge.y()+step)<0.000001),"pan_clamp_no_dead_zone"))return;
                    canvas_->resetView(true);
                    controller_.exportTiff(QUrl::fromLocalFile(output_+"/displayed-frame.tif"));
                    // While export is queued, commands that could replace the
                    // selected frame or mutate its settings must be rejected.
                    controller_.setExposureEv(1);controller_.openRaw(QUrl::fromLocalFile(reject_));
                    require(controller_.busy()&&controller_.frame()==original_&&controller_.exposureEv()==0,"export_single_flight");
                });
            });
        } else if(stage_==1) {
            if(!require(operation=="export"&&ok&&QFileInfo(output_+"/displayed-frame.tif").size()>0,"export_displayed_frame"))return;
            stage_=2;controller_.setExposureEv(0.5);
            controller_.setDialogOpen(true);
            later([this]{
                if(!require(!controller_.busy()&&!controller_.settingsEditable()&&controller_.dirty()&&
                            controller_.frame()==original_,"file_dialog_suspends_pending_preview"))return;
                controller_.setExposureEv(1);
                if(!require(controller_.exposureEv()==0.5,"file_dialog_holds_selected_settings"))return;
                controller_.setDialogOpen(false);
            },400);
            QTimer::singleShot(5000,this,[this]{if(!done_&&stage_==2)finish(false,"automatic brightness preview did not run");});
        } else if(stage_==2) {
            // A native export releases the preview GPU session to avoid
            // keeping two full negatives on a 4 GB device. The first edit
            // rebuilds it; subsequent print edits exercise cached reprint.
            const bool nativeExport=original_->width()!=original_->source_width()||
                                    original_->height()!=original_->source_height();
            if(!require(operation=="render"&&ok&&controller_.frame()!=original_&&
                        controller_.frame()->reprint()==!nativeExport,"brightness_after_original_export"))return;
            if(!require(fingerprint(original_)==original_hash_,"old_frame_immutable"))return;
            held_=controller_.frame();stage_=3;controller_.openRaw(QUrl::fromLocalFile(reject_));
        } else if(stage_==3) {
            if(!require(operation=="open"&&!ok&&controller_.frame()==held_,"failed_raw_retains_frame"))return;
            checks_["rejected_message"]=controller_.status();
            stage_=4;controller_.setExposureEv(1);controller_.exportTiff(QUrl::fromLocalFile(output_+"/displayed-frame.tif"));
        } else if(stage_==4) {
            if(!require(operation=="export"&&!ok&&controller_.frame()==held_&&controller_.dirty()&&controller_.exposureEv()==1,"export_refuses_overwrite_and_retains_pending_settings"))return;
            // Fault injection at the publication boundary: host opens a
            // different decode mode successfully, QImage publication fails.
            stage_=5;*fail_display_=true;controller_.setDecodeMode(1);controller_.openRaw(QUrl::fromLocalFile(input_));
        } else if(stage_==5) {
            if(!require(operation=="open"&&!ok&&controller_.frame()==held_&&controller_.decodeMode()==0,"publication_failure_retains_frame_and_controls"))return;
            stage_=6;controller_.apply();
        } else if(stage_==6) {
            if(!require(operation=="render"&&ok&&controller_.frame()->source==held_->source&&
                        controller_.frame()->decode_mode==held_->decode_mode&&!controller_.frame()->reprint(),"publication_failure_reconciles_worker_session"))return;
            if(!require(fingerprint(original_)==original_hash_,"held_frame_still_immutable"))return;
            const int negative=filmIndex("kodak_ektar_100");
            if(!require(negative>=0,"negative_test_stock_in_catalog"))return;
            stage_=7;controller_.setFilmIndex(negative);
        } else if(stage_==7) {
            if(!require(operation=="render"&&ok&&!controller_.frame()->reprint()&&
                        controller_.frame()->settings.film_stock!=held_->settings.film_stock,"film_change_renders_selected_stock"))return;
            stage_=8;controller_.setPaperIndex((controller_.paperIndex()+1)%controller_.papers().size());
        } else if(stage_==8) {
            if(!require(operation=="render"&&ok&&controller_.frame()->reprint()&&
                        controller_.frame()->settings.print_stock!=held_->settings.print_stock,"paper_change_reprints_selected_stock"))return;
            checkpoint();stage_=9;
            controller_.setExposureEv(-0.2);controller_.setExposureEv(0.3);controller_.setExposureEv(0.8);
            if(!require(!controller_.busy()&&controller_.frame()==held_,"rapid_idle_edits_are_debounced"))return;
        } else if(stage_==9) {
            if(!require(operation=="render"&&ok&&exposureIs(0.8)&&!controller_.dirty()&&
                        frame_publications_==saved_publications_+1&&operations_finished_==saved_operations_+1,
                        "rapid_idle_edits_publish_only_final_settings"))return;
            stage_=10;
            later([this]{
                if(!require(!controller_.busy()&&!controller_.dirty()&&
                            frame_publications_==saved_publications_+1&&operations_finished_==saved_operations_+1,
                            "rapid_idle_edits_do_not_queue_extra_renders"))return;
                checkpoint();stage_=11;
                // Start synchronously, then edit before the event loop can
                // accept its reply. This exercises a real in-flight request
                // without timing guesses or a test-only worker delay.
                controller_.setExposureEv(1.1);controller_.apply();
                if(!require(controller_.busy()&&controller_.settingsEditable(),"render_allows_new_settings"))return;
                const int negative=filmIndex("kodak_portra_400");
                if(!require(negative>=0,"supersede_stock_in_catalog"))return;
                controller_.setFilmIndex(negative);controller_.setExposureEv(-0.4);
                if(!require(controller_.filmIndex()==negative&&std::abs(controller_.exposureEv()+0.4)<1e-12,
                            "in_flight_edits_record_latest_settings"))return;
            },650);
        } else if(stage_==11) {
            if(!require(operation=="render"&&ok&&controller_.frame()==held_&&
                        controller_.filmIndex()==filmIndex("kodak_portra_400")&&
                        std::abs(controller_.exposureEv()+0.4)<1e-12&&controller_.dirty()&&
                        frame_publications_==saved_publications_&&operations_finished_==saved_operations_+1,
                        "superseded_render_never_publishes_or_restores_controls"))return;
            stage_=12;
        } else if(stage_==12) {
            if(!require(operation=="render"&&ok&&controller_.frame()->settings.film_stock=="kodak_portra_400"&&
                        exposureIs(-0.4)&&!controller_.dirty()&&!controller_.busy()&&
                        frame_publications_==saved_publications_+1&&operations_finished_==saved_operations_+2,
                        "in_flight_edits_eventually_publish_latest_settings"))return;
            checkpoint();stage_=13;
            controller_.setExposureEv(0.4);controller_.apply();controller_.setExposureEv(-0.4);
            if(!require(controller_.busy()&&!controller_.dirty(),"return_to_displayed_settings_is_recorded"))return;
        } else if(stage_==13) {
            if(!require(operation=="render"&&ok&&controller_.frame()==held_&&exposureIs(-0.4)&&
                        !controller_.busy()&&!controller_.dirty()&&frame_publications_==saved_publications_&&
                        operations_finished_==saved_operations_+1,"return_to_displayed_settings_drops_old_result"))return;
            stage_=14;
            later([this]{
                if(!require(!controller_.busy()&&!controller_.dirty()&&controller_.frame()==held_&&
                            frame_publications_==saved_publications_&&operations_finished_==saved_operations_+1,
                            "return_to_displayed_settings_settles_without_render"))return;
                startSlides();
            },650);
        } else if(stage_==15) {
            const auto id=slide_ids_[slide_index_];
            const auto caseName=id+(headroom_slides_?QStringLiteral("-headroom"):QString());
            if(!require(operation=="render"&&ok&&controller_.frame()->scan_film&&controller_.filmIsPositive()&&
                        controller_.frame()->settings.film_stock==id.toStdString()&&!controller_.dirty(),
                        "slide_auto_scan_"+caseName))return;
            controller_.setPaperIndex((remembered_paper_+1)%controller_.papers().size());
            controller_.setExposureEv(remembered_ev_+0.5);
            if(!require(controller_.paperIndex()==remembered_paper_&&exposureIs(remembered_ev_)&&
                        !controller_.dirty()&&!controller_.busy(),"slide_ignores_print_controls_"+caseName))return;
            later([this,id,caseName]{
                const auto* brightness=window_.findChild<QObject*>("exposureSlider");
                if(!require(brightness&&!brightness->property("enabled").toBool(),"slide_brightness_control_disabled_"+caseName))return;
                if(!require(canvas_->image().cacheKey()==controller_.previewImage().cacheKey()&&
                            canvas_->image().width()==int(controller_.frame()->width())&&
                            canvas_->image().height()==int(controller_.frame()->height()),"slide_canvas_matches_frame_"+caseName))return;
                if(!capture("slide-"+caseName))return;
                slide_results_.append(QJsonObject{{"film",id},{"scan_film",controller_.frame()->scan_film},
                    {"decode_mode",headroom_slides_?"headroom":"compatible16"},
                    {"width",int(controller_.frame()->width())},{"height",int(controller_.frame()->height())},
                    {"display_sha256",fingerprint(controller_.frame())},{"render_ms",controller_.frame()->timings.render_ms},
                    {"screenshot","slide-"+caseName+".png"}});
                if(++slide_index_<slide_ids_.size())selectSlide();
                else {stage_=16;controller_.setFilmIndex(negative_film_);}
            });
        } else if(stage_==16) {
            if(!require(operation=="render"&&ok&&!controller_.frame()->scan_film&&!controller_.filmIsPositive()&&
                        controller_.filmIndex()==negative_film_&&controller_.paperIndex()==remembered_paper_&&
                        exposureIs(remembered_ev_)&&!controller_.dirty(),headroom_slides_?
                        "negative_slide_negative_restores_print_controls_headroom":"negative_slide_negative_restores_print_controls"))return;
            if(headroom_slides_){stage_=18;controller_.setDecodeMode(0);}
            else {stage_=17;controller_.setDecodeMode(1);}
        } else if(stage_==17) {
            if(!require(operation=="render"&&ok&&controller_.decodeMode()==1&&
                        controller_.frame()->decode_mode==spk::desktop::DecodeMode::headroom&&
                        controller_.frame()->timings.decode_ms>0&&!controller_.dirty(),"decode_mode_automatically_reopens_headroom"))return;
            headroom_slides_=true;startSlides();
        } else if(stage_==18) {
            if(!require(operation=="render"&&ok&&controller_.decodeMode()==0&&
                        controller_.frame()->decode_mode==spk::desktop::DecodeMode::compatible16&&
                        controller_.frame()->timings.decode_ms>0&&!controller_.dirty(),"decode_mode_automatically_returns_compatible"))return;
            stage_=19;controller_.resetSettings();
        } else if(stage_==19) {
            if(!require(operation=="render"&&ok&&controller_.frame()->settings.film_stock=="kodak_portra_400"&&
                        controller_.frame()->settings.print_stock=="kodak_portra_endura"&&exposureIs(0)&&
                        !controller_.frame()->scan_film&&controller_.decodeMode()==0&&!controller_.dirty(),
                        "reset_settings_automatically_updates_preview"))return;
            if(!require(fingerprint(original_)==original_hash_,"original_frame_immutable_after_all_auto_edits"))return;
            stage_=20;
            later([this]{
                if(!capture("final"))return;
                const QString copy=output_+"/second-frame."+QFileInfo(input_).suffix();
                if(!require(QFile::copy(input_,copy),"isolated_second_photo_fixture"))return;
                controller_.openFiles({QUrl::fromLocalFile(input_),QUrl::fromLocalFile(copy)});
            });
        } else if(stage_==20) {
            if(!require(operation=="open"&&ok&&controller_.libraryItems().size()>=3,
                        "qml_library_contains_two_photos_and_rejected_item"))return;
            stage_=21;later([this]{
                if(!capture("library"))return;
                const auto unchanged=controller_.frame();
                controller_.setCropAspect(1);
                if(!require(controller_.frame()==unchanged,"crop_does_not_queue_film_redevelopment"))return;
                whenIdle([this,unchanged]{
                    if(!require(controller_.frame()==unchanged&&controller_.previewImage().width()==controller_.previewImage().height(),
                                "crop_changes_display_without_redeveloping"))return;
                    if(!require(canvas_->image().cacheKey()==controller_.previewImage().cacheKey(),"cropped_canvas_matches_output")||!capture("cropped"))return;
                    if(!require(QMetaObject::invokeMethod(&window_,"startCrop"),"crop_editor_opens"))return;
                    later([this]{
                        if(!require(window_.property("cropMode").toBool()&&canvas_->image().cacheKey()==controller_.uncroppedImage().cacheKey(),
                                    "crop_editor_displays_uncropped_source")||!capture("crop-editor"))return;
                        // Deliver real pointer events through the QML overlay,
                        // including its dialog/debounce lock and normalized
                        // source mapping. No native file dialog is automated.
                        const auto bounds=canvas_->imageRect();
                        const auto start=canvas_->mapToScene(bounds.topLeft()+QPointF(bounds.width()*0.2,bounds.height()*0.3));
                        const auto end=canvas_->mapToScene(bounds.topLeft()+QPointF(bounds.width()*0.7,bounds.height()*0.7));
                        auto pointer=[this](QEvent::Type type,QPointF point,Qt::MouseButton button,Qt::MouseButtons buttons){
                            QMouseEvent event(type,point,point,window_.mapToGlobal(point.toPoint()),button,buttons,Qt::NoModifier);
                            QCoreApplication::sendEvent(&window_,&event);
                        };
                        pointer(QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::LeftButton);
                        pointer(QEvent::MouseMove,end,Qt::NoButton,Qt::LeftButton);
                        pointer(QEvent::MouseButtonRelease,end,Qt::LeftButton,Qt::NoButton);
                        const auto crop=controller_.cropRect();
                        if(!require(!window_.property("cropMode").toBool()&&std::abs(crop.x()-0.2)<1e-5&&
                                    std::abs(crop.y()-0.3)<1e-5&&std::abs(crop.width()-0.5)<1e-5&&
                                    std::abs(crop.height()-0.4)<1e-5,"qml_crop_pointer_gesture_applies_geometry"))return;
                        whenIdle([this]{
                        if(!require(QMetaObject::invokeMethod(&window_,"openExport"),"export_options_open"))return;
                        later([this]{
                            if(!require(!controller_.settingsEditable(),"export_options_suspend_edits")||!capture("export-options"))return;
                            auto* popup=window_.findChild<QObject*>("exportPopup");
                            if(!require(popup&&QMetaObject::invokeMethod(popup,"close"),"export_options_close"))return;
                            later([this]{
                                if(!require(controller_.settingsEditable(),"export_options_release_edits"))return;
                                const auto frame=controller_.frame();
                                controller_.rotateClockwise();controller_.setFlipHorizontal(true);controller_.setStraightenDegrees(3.5);
                                if(!require(controller_.frame()==frame&&controller_.quarterTurns()==1&&controller_.flipHorizontal()&&
                                            controller_.straightenDegrees()==3.5,"orientation_preserves_engine_frame"))return;
                                controller_.undo();
                                if(!require(controller_.straightenDegrees()==0&&controller_.canRedo(),"geometry_undo"))return;
                                controller_.redo();controller_.flushEdits();
                                if(!require(controller_.straightenDegrees()==3.5&&controller_.persistenceWarning().isEmpty(),"geometry_redo_and_persist"))return;
                                whenIdle([this]{
                                    if(!require(canvas_->image().cacheKey()==controller_.previewImage().cacheKey(),"transformed_canvas_matches_output")||!capture("geometry"))return;
                                    auto* undoButton=window_.findChild<QObject*>("undoButton");
                                    if(!require(undoButton&&undoButton->property("enabled").toBool()&&QMetaObject::invokeMethod(undoButton,"clicked"),"qml_undo_button"))return;
                                    if(!require(controller_.straightenDegrees()==0,"qml_undo_reaches_geometry"))return;
                                    auto* redoButton=window_.findChild<QObject*>("redoButton");
                                    if(!require(redoButton&&QMetaObject::invokeMethod(redoButton,"clicked")&&controller_.straightenDegrees()==3.5,"qml_redo_button"))return;
                                    whenIdle([this]{testComparisons();});
                                });
                            });
                        });
                        });
                    });
                });
            });
        } else if(stage_==22) {
            if(!require(operation=="scene-measure"&&ok&&controller_.sceneReport()["available"].toBool(),"qml_scene_measurement_available"))return;
            if(!require(controller_.frame()==held_&&controller_.previewImage()==scene_before_,"qml_measure_does_not_change_picture"))return;
            stage_=23;
            later([this]{
                // Native windows may defer layout while occluded. Capture
                // forces the actual render/polish before inspecting its rail.
                if(!capture("scene-measured"))return;
                auto* graph=window_.findChild<QObject*>("sceneLatitudeGraph");
                checks_["graph_visible"]=graph&&graph->property("visible").toBool();checks_["graph_width"]=graph?graph->property("width").toDouble():0;
                checks_["graph_controller_available"]=controller_.sceneReport()["available"].toBool();checks_["graph_post_mode"]=window_.property("postMode").toBool();checks_["window_exposed"]=window_.isExposed();
                if(!require(graph&&graph->property("visible").toBool()&&graph->property("width").toDouble()>0,"qml_real_latitude_graph_visible"))return;
                const double minimum=controller_.sceneReport()["fit"].toMap()["highlight"].toMap()["minimum_pull_back"].toDouble();
                auto* high=window_.findChild<QObject*>("sceneHighlightSlider");auto* low=window_.findChild<QObject*>("sceneShadowSlider");
                auto* apply=window_.findChild<QObject*>("applySceneFitButton");
                if(!require(high&&low&&apply&&high->setProperty("value",std::max(.75,minimum+.75))&&low->setProperty("value",0),"qml_scene_fit_controls"))return;
                require(QMetaObject::invokeMethod(apply,"clicked"),"qml_scene_apply_button");
            });
        } else if(stage_==23) {
            if(!require(operation=="scene-fit"&&ok&&controller_.dirty(),"qml_scene_fit_commits_curve"))return;
            stage_=24;
        } else if(stage_==24) {
            if(!require(operation=="render"&&ok&&controller_.frame()->settings.placement.active&&controller_.previewImage()!=scene_before_,"qml_scene_curve_reaches_canvas"))return;
            later([this]{
                if(!capture("scene-fitted"))return;
                if(!require(QMetaObject::invokeMethod(&window_,"showFilmFrame"),"qml_film_section_scrolls_into_view"))return;
                stage_=25;
                auto* toggle=window_.findChild<QObject*>("dateBackToggle");
                controller_.setFilmFrame("dateText","26 10 30");
                controller_.setFilmFrame("dateSize",2.);
                if(!require(toggle&&toggle->setProperty("checked",true)&&QMetaObject::invokeMethod(toggle,"toggled"),"qml_date_toggle_reaches_controller"))return;
            });
        } else if(stage_==25) {
            if(!require(operation=="render"&&ok&&controller_.frame()->settings.date.active&&controller_.frame()->input_cut(),"qml_date_exposure_reaches_render"))return;
            later([this]{
                if(!capture("date-back"))return;
                stage_=26;
                auto* toggle=window_.findChild<QObject*>("filmEdgeToggle");
                if(!require(toggle&&toggle->setProperty("checked",true)&&QMetaObject::invokeMethod(toggle,"toggled"),"qml_film_toggle_reaches_controller"))return;
            });
        } else if(stage_==26) {
            if(!require(operation=="render"&&ok&&controller_.frame()->film_gate_valid&&controller_.frame()->settings.edge.active&&
                controller_.previewImage().size()==QSize(int(controller_.frame()->width()),int(controller_.frame()->height())),"qml_film_canvas_not_recropped"))return;
            later([this]{
                const auto original=controller_.originalImage();
                if(!require(original.size()==controller_.previewImage().size(),"qml_original_aligned_to_film_gate")||!capture("film-edge-date"))return;
                controller_.selectAllPhotos(false);int selected=0;
                for(int i=0;i<controller_.libraryItems().size()&&selected<2;++i){const auto item=controller_.libraryItems()[i].toMap();if(item["error"].toString().isEmpty()){controller_.setItemSelected(i,true);++selected;}}
                auto* button=window_.findChild<QObject*>("createPairButton");stage_=27;
                if(!require(selected==2&&button&&button->property("enabled").toBool()&&QMetaObject::invokeMethod(button,"clicked"),"qml_create_pair_button"))return;
            });
        } else if(stage_==27) {
            if(!require(operation=="open"&&ok&&controller_.frame()->settings.pair.active&&controller_.frame()->film_gate_valid&&controller_.frame()->film_gate!=controller_.frame()->film_gate_b,"qml_pair_two_real_sources_and_gates"))return;
            later([this]{
                if(!require(controller_.originalImage().size()==controller_.previewImage().size(),"qml_pair_original_geometry")||!capture("half-pair"))return;
                stage_=28;auto* toggle=window_.findChild<QObject*>("pairTurnedToggle");
                if(!require(toggle&&toggle->setProperty("checked",true)&&QMetaObject::invokeMethod(toggle,"toggled"),"qml_pair_direction_control"))return;
            });
        } else if(stage_==28) {
            if(!require(operation=="render"&&ok&&controller_.frame()->settings.pair.turned&&controller_.frame()->film_vertical,"qml_pair_direction_reaches_canvas"))return;
            later([this]{
                if(!capture("half-pair-vertical"))return;auto* button=window_.findChild<QObject*>("openMaterialManagerButton");
                if(!require(button&&QMetaObject::invokeMethod(button,"clicked"),"qml_material_manager_opens"))return;
                later([this]{
                    auto* manager=window_.findChild<QObject*>("materialManager");auto* use=window_.findChild<QObject*>("useMaterialButton");
                    if(!require(manager&&manager->property("visible").toBool()&&manager->setProperty("selectedId","open_kodak_trix_7266_033")&&use,"qml_material_source_details")||!capture("material-library"))return;
                    stage_=29;require(QMetaObject::invokeMethod(use,"clicked"),"qml_use_new_open_material");
                });
            });
        } else if(stage_==29) {
            if(!require(operation=="render"&&ok&&controller_.frame()->settings.film_stock=="open_kodak_trix_7266_033"&&controller_.frame()->scan_film,"qml_7266_real_vulkan_preview"))return;
            later([this]{
                auto* manager=window_.findChild<QObject*>("materialManager");if(!require(manager&&QMetaObject::invokeMethod(manager,"close"),"qml_manager_closes_for_preview")||!capture("material-7266"))return;
                const auto profile=output_+"/owned-profile.json";if(!require(QFile::copy(QCoreApplication::applicationDirPath()+"/resources/profiles/open_kodak_trix_7266_033.json",profile),"qml_owned_material_import_fixture"))return;
                held_=controller_.frame();stage_=30;
                require(QMetaObject::invokeMethod(manager,"importUrl",Q_ARG(QVariant,QVariant(QUrl::fromLocalFile(profile)))),"qml_profile_import_handler");
            });
        } else if(stage_==30) {
            if(!require(operation=="materials"&&ok&&controller_.frame()==held_,"qml_import_preserves_current_frame"))return;
            later([this]{
                QString id;for(const auto value:controller_.materialEntries()){const auto entry=value.toMap();if(entry["custom"].toBool()){id=entry["id"].toString();break;}}
                auto* manager=window_.findChild<QObject*>("materialManager");auto* use=window_.findChild<QObject*>("useMaterialButton");
                if(!require(!id.isEmpty()&&manager&&manager->setProperty("selectedId",id)&&QMetaObject::invokeMethod(manager,"open")&&use,"qml_custom_material_registered"))return;
                stage_=31;require(QMetaObject::invokeMethod(use,"clicked"),"qml_use_custom_material_button");
            });
        } else if(stage_==31) {
            if(!require(operation=="render"&&ok&&controller_.frame()->settings.film_stock.starts_with("usr_")&&controller_.frame()->settings.film_profile_sha256.size()==64,"qml_custom_material_version_reaches_vulkan"))return;
            later([this]{
                if(!capture("material-custom"))return;auto* manager=window_.findChild<QObject*>("materialManager");auto* use=window_.findChild<QObject*>("useMaterialButton");
                if(!require(manager&&manager->setProperty("selectedId","open_kodak_doublex_5222_65m_033")&&use,"qml_doublex_material_available"))return;
                stage_=32;require(QMetaObject::invokeMethod(use,"clicked"),"qml_use_doublex_button");
            });
        } else if(stage_==32) {
            if(!require(operation=="render"&&ok&&controller_.frame()->settings.film_stock=="open_kodak_doublex_5222_65m_033"&&!controller_.frame()->scan_film&&controller_.frame()->settings.print_stock=="ilford_multigrade_iv_rc","qml_doublex_real_vulkan_silver_print"))return;
            later([this]{
                auto* manager=window_.findChild<QObject*>("materialManager");if(!require(manager&&QMetaObject::invokeMethod(manager,"close"),"qml_doublex_manager_closes")||!capture("material-doublex"))return;
                auto* use=window_.findChild<QObject*>("useMaterialButton");if(!require(manager->setProperty("selectedId","open_kodak_2302_5m_033")&&QMetaObject::invokeMethod(manager,"open")&&use,"qml_2302_print_material_available"))return;
                stage_=33;require(QMetaObject::invokeMethod(use,"clicked"),"qml_use_2302_button");
            });
        } else if(stage_==33) {
            if(!require(operation=="render"&&ok&&controller_.frame()->settings.print_stock=="open_kodak_2302_5m_033"&&!controller_.frame()->scan_film,"qml_2302_real_vulkan_print"))return;
            later([this]{auto* manager=window_.findChild<QObject*>("materialManager");if(!require(manager&&QMetaObject::invokeMethod(manager,"close"),"qml_2302_manager_closes"))return;
                auto* note=window_.findChild<QObject*>("printMaterialPolicyNote");if(require(note&&note->property("text").toString().contains(QStringLiteral("电影印片")),"qml_2302_not_mislabeled_as_multigrade_paper")&&capture("material-2302"))testBatchDialog(false);});
        } else if(stage_==34||stage_==35) {
            const auto files=QDir(output_+"/qml-selected-export").entryList({"*.png"},QDir::Files);
            if(!require(operation=="batch-export"&&ok&&files.size()==1,"qml_folder_export_writes_selected_photo"))return;
            QFile file(output_+"/qml-selected-export/"+files.first());
            if(!require(file.open(QIODevice::ReadOnly),"qml_folder_export_readback"))return;
            const auto hash=QCryptographicHash::hash(file.readAll(),QCryptographicHash::Sha256);
            if(stage_==34){
                batch_export_hash_=hash;
                if(!require(controller_.status().contains(QStringLiteral("导出 1 张")),"qml_batch_result_reports_written_file"))return;
                later([this]{testBatchDialog(true);});
            }else{
                if(!require(hash==batch_export_hash_&&controller_.status().contains(QStringLiteral("跳过已有 1 张"))&&
                            controller_.status().contains(QStringLiteral("空文件夹")),"qml_batch_skip_preserves_file_and_explains_result"))return;
                later([this]{
                    if(require(window_.property("exportResultMessage").toString().contains(QStringLiteral("跳过已有 1 张")),"qml_batch_result_stays_visible")&&capture("batch-result"))finish(true,{});
                });
            }
        } else {
            require(false,"unexpected_operation_during_settle");
        }
    }
    void testBatchDialog(bool repeat) {
        const auto destination=output_+"/qml-selected-export";
        if(!require(repeat||QDir().mkdir(destination),"qml_batch_owned_destination"))return;
        controller_.selectAllPhotos(false);controller_.setItemSelected(controller_.activeIndex(),true);
        window_.setProperty("exportFormat",1);window_.setProperty("exportLongEdge",1080);
        QQmlExpression options(QQmlEngine::contextForObject(&window_),&window_,QStringLiteral("batchDialog.options = FolderDialog.DontUseNativeDialog"));options.evaluate();
        if(!require(!options.hasError()&&QMetaObject::invokeMethod(&window_,"openExport"),"qml_batch_export_options_open"))return;
        later([this,destination,repeat]{
            auto* button=window_.findChild<QObject*>("exportSelectedButton");
            if(!require(button&&button->property("enabled").toBool()&&QMetaObject::invokeMethod(button,"clicked"),"qml_batch_export_button"))return;
            later([this,destination,repeat]{
                auto* dialog=window_.findChild<QObject*>("batchExportDialog");
                auto* popup=window_.findChild<QObject*>("exportPopup");
                if(!require(dialog&&dialog->property("visible").toBool()&&popup&&!popup->property("visible").toBool(),"qml_folder_dialog_opens_after_export_popup_closes"))return;
                if(!require(dialog->setProperty("selectedFolder",QUrl::fromLocalFile(destination)),"qml_folder_destination_selected"))return;
                stage_=repeat?35:34;
                require(QMetaObject::invokeMethod(dialog,"accept"),"qml_folder_accept_starts_batch");
            });
        });
    }
    void testComparisonViewport(int view,std::function<void()> complete) {
        if(view==6) {
            canvas_->resetView(true);canvas_->setSplitPosition(.37);
            window_.setProperty("compareMode",false);
            later(std::move(complete));return;
        }
        // Exercise the actual QML overlay as well as the clipped textures.
        // Native-size images extend beyond the viewport; their divider and
        // its hit area must never escape into the rails or toolbar.
        canvas_->resetView(view==0);
        if(view==2)canvas_->panBy(1e9,1e9);
        if(view==3)canvas_->panBy(-1e9,-1e9);
        canvas_->setSplitPosition(view==2?.15:view==3?.85:view==4?0:view==5?1:.5);
        canvas_->setCompare(false);
        later([this,view,complete=std::move(complete)]{
            if(!capture(QStringLiteral("compare-viewport-%1-base").arg(view)))return;
            const auto baseline=capture_;
            canvas_->setCompare(true);
            later([this,view,baseline,complete]{
                if(!capture(QStringLiteral("compare-viewport-%1-split").arg(view)))return;
                const double ratio=window_.effectiveDevicePixelRatio();
                const auto origin=canvas_->mapToScene(QPointF());
                const QRectF viewport(origin,QSizeF(canvas_->width(),canvas_->height()));
                bool contained=baseline.size()==capture_.size();
                for(int y=0;contained&&y<capture_.height();++y)
                    for(int x=0;x<capture_.width();++x)
                        if(!viewport.adjusted(-1,-1,1,1).contains(QPointF((x+.5)/ratio,(y+.5)/ratio))&&
                           baseline.pixel(x,y)!=capture_.pixel(x,y)){contained=false;break;}
                if(!require(contained,QStringLiteral("compare_viewport_%1_no_pixels_outside_canvas").arg(view)))return;
                auto* divider=visualNamed(window_.contentItem(),"compareDivider");
                if(view==5&&!require(divider&&divider->parentItem()==canvas_&&canvas_->clip()&&divider->width()==1,
                            QStringLiteral("compare_viewport_%1_line_and_hit_area_clipped_at_constant_width").arg(view)))return;
                if(view<4) {
                    const auto imageBox=canvas_->imageRect();const double split=canvas_->splitPosition();
                    const auto start=canvas_->mapToScene(QPointF(imageBox.x()+imageBox.width()*split+.5,canvas_->height()*.5));
                    const auto end=start+QPointF(12,0);
                    auto pointer=[this](QEvent::Type type,QPointF point,Qt::MouseButton button,Qt::MouseButtons buttons){
                        QMouseEvent event(type,point,point,window_.mapToGlobal(point.toPoint()),button,buttons,Qt::NoModifier);
                        QCoreApplication::sendEvent(&window_,&event);
                    };
                    pointer(QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::LeftButton);
                    pointer(QEvent::MouseMove,end,Qt::NoButton,Qt::LeftButton);
                    pointer(QEvent::MouseButtonRelease,end,Qt::LeftButton,Qt::NoButton);
                    const double expected=(end.x()-origin.x()-imageBox.x())/imageBox.width();
                    if(!require(std::abs(canvas_->splitPosition()-expected)<1e-6&&canvas_->imageRect()==imageBox,
                                QStringLiteral("compare_viewport_%1_drag_moves_split_without_panning").arg(view)))return;
                }
                testComparisonViewport(view+1,complete);
            });
        });
    }
    void testComparisons() {
        window_.setProperty("originalMode",false);window_.setProperty("compareMode",true);
        testComparisonViewport(0,[this]{testComparisonModes();});
    }
    void testComparisonModes() {
        const auto held=controller_.frame();const auto rendered=controller_.previewImage();
        const auto original=controller_.originalImage();
        if(!require(!original.isNull()&&original.size()==rendered.size()&&original!=rendered,"comparison_has_real_decoded_original"))return;
        if(!capture("comparison-base"))return;comparison_base_=capture_;
        window_.setProperty("originalMode",true);
        later([this,held,rendered,original]{
            if(!require(canvas_->showOriginal()&&canvas_->beforeImage()==original&&controller_.previewImage()==rendered&&controller_.frame()==held,"original_view_does_not_replace_export_or_frame")||!capture("original"))return;
            const auto offset=canvas_->mapToScene(QPointF());const auto box=canvas_->imageRect().translated(offset);
            const auto ratio=window_.effectiveDevicePixelRatio();bool contained=true;
            for(double fx:{.05,.15,.85,.95})for(double fy:{.25,.5,.75}) {
                const auto p=offset+QPointF(canvas_->width()*fx,canvas_->height()*fy);
                if(!box.adjusted(-2,-2,2,2).contains(p)) {
                    const int x=int(p.x()*ratio),y=int(p.y()*ratio);
                    contained=contained&&capture_.pixel(x,y)==comparison_base_.pixel(x,y);
                }
            }
            if(!require(contained,"original_texture_stays_in_transformed_canvas_rectangle"))return;
            window_.setProperty("originalMode",false);window_.setProperty("compareMode",true);canvas_->setSplitPosition(.37);
            later([this,held,rendered]{
                if(!require(canvas_->compare()&&!canvas_->showOriginal()&&std::abs(canvas_->splitPosition()-.37)<1e-9&&controller_.previewImage()==rendered,"split_compare_has_shared_geometry_and_immutable_output")||!capture("compare-split"))return;
                canvas_->forceActiveFocus();
                QKeyEvent press(QEvent::KeyPress,Qt::Key_Space,Qt::NoModifier);
                QCoreApplication::sendEvent(&window_,&press);
                later([this]{
                    if(!require(canvas_->showOriginal(),"space_holds_original_over_split"))return;
                    QKeyEvent release(QEvent::KeyRelease,Qt::Key_Space,Qt::NoModifier);QCoreApplication::sendEvent(&window_,&release);
                    later([this]{
                        if(!require(!canvas_->showOriginal()&&canvas_->compare(),"space_release_restores_split"))return;
                        window_.setProperty("compareMode",false);window_.setProperty("postMode",true);
                        QQmlExpression edit(QQmlEngine::contextForObject(&window_),&window_,QStringLiteral("preview.setCurve(0, [{x:0,y:0},{x:0.5,y:0.62},{x:1,y:1}])"));
                        edit.evaluate();
                        if(!require(!edit.hasError()&&controller_.postAdjustments()["curves"].toList()[0].toList().size()==3,"qml_curve_array_reaches_edit_state"))return;
                        controller_.setPostAdjustment("zone2Hue",215);controller_.setPostAdjustment("zone2Saturation",.2);
                        controller_.setPostAdjustment("vignetteAmount",-25);controller_.flushEdits();
                        whenIdle([this]{
                            auto* slider=window_.findChild<QObject*>("postAdjustmentSlider_exposure");
                            if(!require(slider&&!slider->property("live").toBool(),"post_slider_defers_full_precision_until_release"))return;
                            const auto before=controller_.postAdjustments()["exposure"].toDouble();
                            const auto visible=controller_.previewImage().cacheKey();
                            slider->setProperty("pressed",true);slider->setProperty("value",.4);QMetaObject::invokeMethod(slider,"moved");
                            if(!require(controller_.postAdjustments()["exposure"].toDouble()==before&&controller_.previewImage().cacheKey()==visible,"held_post_slider_does_not_process_full_frame"))return;
                            slider->setProperty("pressed",false);
                            if(!require(std::abs(controller_.postAdjustments()["exposure"].toDouble()-.4)<1e-9,"released_post_slider_commits_value"))return;
                            controller_.undo();
                            if(!require(std::abs(controller_.postAdjustments()["exposure"].toDouble()-before)<1e-9,"post_slider_release_is_one_undo_step"))return;
                            whenIdle([this]{
                            if(!capture("post-develop"))return;
                            if(!require(QMetaObject::invokeMethod(&window_,"showSceneFit"),"qml_scene_section_scrolls_into_view"))return;
                            held_=controller_.frame();scene_before_=controller_.previewImage();stage_=22;
                            auto* button=window_.findChild<QObject*>("measureSceneButton");
                            require(button&&QMetaObject::invokeMethod(button,"clicked"),"qml_scene_measure_button");
                            });
                        });
                    });
                });
            });
        });
    }
    void finish(bool success,const QString& error) {
        if(done_)return;done_=true;
        if(!success){const auto scene=window_.grabWindow();QFile screenshot(output_+"/failure.png");if(!scene.isNull()&&screenshot.open(QIODevice::WriteOnly|QIODevice::NewOnly))scene.save(&screenshot,"PNG");}
        checks_["passed"]=success;checks_["error"]=error;
        checks_["qt_version"]=QString(qVersion());checks_["platform"]=QGuiApplication::platformName();
        checks_["quick_backend_requested"]=qEnvironmentVariable("QT_QUICK_BACKEND");
        checks_["slide_results"]=slide_results_;
        checks_["frame_publications"]=frame_publications_;checks_["operations_finished"]=operations_finished_;
        checks_["scope"]="QML scene, controller, native Vulkan renderer and file writer; not physical display calibration or native dialog automation";
        QFile report(output_+"/report.json");if(report.open(QIODevice::WriteOnly))report.write(QJsonDocument(checks_).toJson());
        QCoreApplication::exit(success?0:1);
    }
    PreviewController& controller_;QQuickWindow& window_;FrameCanvas* canvas_=nullptr;
    QString input_,reject_,output_,original_hash_;std::shared_ptr<bool> fail_display_;
    spk::desktop::FramePtr original_,held_;QJsonObject checks_;int stage_=0;bool done_=false;
    int frame_publications_=0,operations_finished_=0,saved_publications_=0,saved_operations_=0;
    int remembered_paper_=0,negative_film_=0,slide_index_=0;double remembered_ev_=0;
    const QStringList slide_ids_{"fujifilm_provia_100f","fujifilm_velvia_100","kodak_ektachrome_100","kodak_kodachrome_64"};
    QJsonArray slide_results_;
    bool headroom_slides_=false;
    QImage capture_;
    QImage comparison_base_;
    QByteArray batch_export_hash_;
    QImage scene_before_;
};
}

int main(int argc,char** argv) {
    // The Windows offscreen plugin uses a file font database instead of the
    // native DirectWrite one. Reuse installed system fonts for the explicit
    // test platform; no fonts are copied or bundled with the application.
    if(qEnvironmentVariable("QT_QPA_PLATFORM")=="offscreen"&&qEnvironmentVariableIsEmpty("QT_QPA_FONTDIR"))
        qputenv("QT_QPA_FONTDIR",(qEnvironmentVariable("WINDIR")+"/Fonts").toUtf8());
    // Preserve Qt diagnostics when launched by a validation runner with
    // redirected stderr, even though the production executable has no console.
    qInstallMessageHandler([](QtMsgType type,const QMessageLogContext& context,const QString& message){
        const auto bytes=qFormatLogMessage(type,context,message).toUtf8();
        std::fwrite(bytes.data(),1,std::size_t(bytes.size()),stderr);std::fputc('\n',stderr);std::fflush(stderr);
    });
    QGuiApplication app(argc,argv);
    app.setOrganizationName("SpektraLab");app.setApplicationName("SpektraLab Windows");
    app.setWindowIcon(QIcon(QStringLiteral(":/assets/film.ico")));
    QQuickStyle::setStyle("Basic");
    QCommandLineParser parser;parser.setApplicationDescription("SpektraLab Windows Qt Quick preview");parser.addHelpOption();
    parser.addOptions({{"resources","Engine resource directory","directory"},
                       {"open","Open a RAW file","file"},
                       {"snapshot","Save one rendered window capture to a new PNG and exit (requires --open)","file"},
                       {"ui-smoke","Capture the initialized empty editor to a NEW PNG and exit (no RAW required)","file"},
                       {"ui-smoke-post","Show the post-develop tab in the UI smoke capture"},
                       {"ui-smoke-popup","Show the material picker popup in a UI smoke capture"},
                       {"edit-store","Override the per-photo edit directory (tests use an isolated directory)","directory"},
                       {"self-test","Run real RAW frontend checks and save a report in a NEW directory","directory"},
                       {"reject","Unsupported RAW for the failed-open check","file"}});
    #ifdef SPK_PHOENIX_PREVIEW
    parser.addOption({"fixed-gain","Open an isolated Research Only fixed internal gain window"});
    parser.addOption({"phoenix-stock","Initial experimental stock ID","id"});
#endif
    parser.process(app);
#ifdef SPK_PHOENIX_PREVIEW
    qputenv("SPEKTRALAB_PHOENIX_FIXED_GAIN",parser.isSet("fixed-gain")?"1":"0");
    app.setApplicationName(parser.isSet("fixed-gain")?"SpektraLab Phoenix Fixed Gain Research 0.1":"SpektraLab Phoenix Experimental Preview 0.1");
#endif
    const auto resources=parser.isSet("resources")?parser.value("resources"):QCoreApplication::applicationDirPath()+"/resources";
    const bool testing=parser.isSet("self-test");
    const bool snapshot=parser.isSet("snapshot");
    const bool uiSmoke=parser.isSet("ui-smoke");
    if(uiSmoke&&(snapshot||testing||parser.isSet("open")||QFileInfo::exists(parser.value("ui-smoke"))))return 2;
    if(snapshot&&(!parser.isSet("open")||testing||QFileInfo::exists(parser.value("snapshot"))))return 2;
    const auto testOutput=parser.value("self-test");
    if(testing&&(!parser.isSet("open")||!parser.isSet("reject")||QFileInfo::exists(testOutput)||!QDir().mkpath(testOutput)))return 2;
    QString testInput=parser.value("open"),testReject=parser.value("reject");
    if(testing){
        testInput=QDir(testOutput).absoluteFilePath("input."+QFileInfo(testInput).suffix());
        testReject=QDir(testOutput).absoluteFilePath("rejected."+QFileInfo(testReject).suffix());
        if(!QFile::copy(parser.value("open"),testInput)||!QFile::copy(parser.value("reject"),testReject))return 2;
    }
    const QString editDirectory=testing?QDir(testOutput).absoluteFilePath("edits"):
        parser.isSet("edit-store")?QFileInfo(parser.value("edit-store")).absoluteFilePath():
        snapshot?QFileInfo(parser.value("snapshot")).absoluteFilePath()+"-edits":
        uiSmoke?QFileInfo(parser.value("ui-smoke")).absoluteFilePath()+"-edits":QString{};
    auto failDisplay=std::make_shared<bool>(false);
    PreviewController controller(std::filesystem::path(resources.toStdWString()),nullptr,
        [failDisplay](const auto& frame){if(*failDisplay){*failDisplay=false;throw std::runtime_error("Injected display publication failure");}return PreviewController::imageFromFrame(frame);},editDirectory);
    #ifdef SPK_PHOENIX_PREVIEW
    if(parser.isSet("phoenix-stock"))QObject::connect(&controller,&PreviewController::sourceOpened,&app,[&controller,&parser]{controller.chooseMaterial(parser.value("phoenix-stock"));});
#endif
    qmlRegisterType<FrameCanvas>("SpektraLab.Native",1,0,"FrameCanvas");
    QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("preview",&controller);
    QObject::connect(&engine,&QQmlApplicationEngine::objectCreationFailed,&app,[]{QCoreApplication::exit(2);},Qt::QueuedConnection);
    engine.loadFromModule("SpektraLab","Main");
    if(engine.rootObjects().isEmpty())return 2;
#ifdef _WIN32
    if(QGuiApplication::platformName()=="windows") {
        // Keep native resizing, snap layouts, caption buttons and accessibility.
        // Style only this window, never global Windows theme/registry settings.
        if(auto* native=qobject_cast<QQuickWindow*>(engine.rootObjects().first())) {
            const HWND handle=reinterpret_cast<HWND>(native->winId());
            const BOOL dark=TRUE;
            const COLORREF caption=RGB(41,43,41),ink=RGB(250,248,244);
            const auto modeResult=DwmSetWindowAttribute(handle,20,&dark,sizeof(dark));
            const auto captionResult=DwmSetWindowAttribute(handle,35,&caption,sizeof(caption));
            const auto textResult=DwmSetWindowAttribute(handle,36,&ink,sizeof(ink));
            COLORREF observed=0;
            const auto readback=DwmGetWindowAttribute(handle,35,&observed,sizeof(observed));
            // CAPTION_COLOR is documented for Set, not Get. Some Windows
            // builds accept Set and reject Get with E_INVALIDARG; do not
            // mistake an unsupported diagnostic read for a failed setting.
            native->setProperty("nativeCaptionDiagnostics",QVariantMap{{"dark_mode_set_hresult",qint64(modeResult)},{"caption_set_hresult",qint64(captionResult)},{"text_set_hresult",qint64(textResult)},{"optional_get_hresult",qint64(readback)},{"optional_observed_color",quint64(observed)},{"requested_color",quint64(caption)}});
            native->setProperty("nativeDarkCaptionApplied",SUCCEEDED(modeResult)&&SUCCEEDED(captionResult)&&SUCCEEDED(textResult));
        }
    }
#endif
    std::unique_ptr<SelfTest> test;
    if(uiSmoke) {
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());if(!window)return 2;
        if(parser.isSet("ui-smoke-post"))window->setProperty("postMode",true);
        const auto destination=parser.value("ui-smoke");
        QObject::connect(&controller,&PreviewController::initialized,&app,[window,destination](bool ok){
            if(!ok){QCoreApplication::exit(1);return;}
            QTimer::singleShot(300,window,[window,destination]{
                const bool controls=!QGuiApplication::windowIcon().isNull()&&!window->icon().isNull()&&
                                    window->findChild<QObject*>("monochromeFilmList")&&
                                    window->findChild<QObject*>("displayHistogram");
                QFile file(destination);const auto scene=window->grabWindow();
                const bool saved=controls&&!scene.isNull()&&file.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&
                    scene.save(&file,"PNG")&&file.flush();
                QCoreApplication::exit(saved?0:1);
            });
        });
        if(parser.isSet("ui-smoke-popup"))QObject::connect(&controller,&PreviewController::initialized,&app,[window](bool ok){if(ok)QTimer::singleShot(100,window,[window]{if(auto* picker=window->findChild<QObject*>("filmQuickSelect")){auto* popup=picker->property("popup").value<QObject*>();if(popup)QMetaObject::invokeMethod(popup,"open");}});});
        QTimer::singleShot(30000,&app,[]{QCoreApplication::exit(1);});
    } else if(testing) {
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());if(!window)return 2;
        test=std::make_unique<SelfTest>(controller,*window,testInput,testReject,testOutput,failDisplay);
    } else if(parser.isSet("open")) {
        if(snapshot) {
            auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());if(!window)return 2;
            const auto destination=parser.value("snapshot");
            QObject::connect(&controller,&PreviewController::operationFinished,&app,[window,destination](const QString& operation,bool ok){
                if(operation!="open")return;
                if(!ok){QCoreApplication::exit(1);return;}
                QTimer::singleShot(250,window,[window,destination]{
                    QFile file(destination);const auto scene=window->grabWindow();
                    const bool saved=!scene.isNull()&&file.open(QIODevice::WriteOnly|QIODevice::NewOnly)&&scene.save(&file,"PNG")&&file.flush();
                    QCoreApplication::exit(saved?0:1);
                });
            });
            QTimer::singleShot(60000,&app,[]{QCoreApplication::exit(1);});
        }
        QObject::connect(&controller,&PreviewController::initialized,&app,[&](bool ok){if(ok)controller.openRaw(QUrl::fromLocalFile(parser.value("open")));});
    }
    return app.exec();
}
