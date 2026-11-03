#include "PreviewController.hpp"
#include "PairProject.hpp"
#include <QColorSpace>
#include <QMetaObject>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDate>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>

namespace {
namespace desktop=spk::desktop;
namespace output=desktop::output;
namespace editing=desktop::editing;
QString text(const std::string& s) { return QString::fromUtf8(s.data(),qsizetype(s.size())); }
std::filesystem::path path(const QUrl& u) { return std::filesystem::path(u.toLocalFile().toStdWString()); }
QVariantList list(const std::vector<desktop::Stock>& stocks,const MaterialLibrary* materials=nullptr) {
    QVariantList result;
    for(const auto& s:stocks) {const auto entry=materials?materials->entry(text(s.id)):QVariantMap{};const bool enabled=entry.isEmpty()||(entry.value("available").toBool()&&entry.value("enabled").toBool()&&!entry.value("removed").toBool());result.push_back(QVariantMap{{"id",text(s.id)},{"name",text(s.label)+(enabled?QString():QStringLiteral("（已禁用/存档）"))},{"positive",s.is_positive},{"monochrome",s.is_monochrome},{"printFilm",entry.value("stage")=="printing"&&entry.value("support")=="film"},{"unfiltered2302",entry.value("logicalId")=="open_kodak_2302_5m_033"},{"multigrade",entry.value("logicalId")=="ilford_multigrade_iv_rc"},{"cine",entry.value("use")=="cine"},{"custom",entry.value("custom",false)},{"enabled",enabled}});}
    return result;
}
bool sameSettings(desktop::RenderSettings a, desktop::RenderSettings b) {
    const bool exposure=std::abs(a.print_exposure-b.print_exposure)<1e-12;
    a.print_exposure=b.print_exposure;
    return exposure&&a==b;
}
bool sameState(const editing::State& a,const editing::State& b) {
    return sameSettings(a.settings,b.settings)&&a.mode==b.mode&&a.geometry==b.geometry&&a.develop==b.develop;
}
QImage visibleImage(const desktop::FramePtr& frame,const QImage& full,const output::Geometry& geometry,const output::Develop& develop={}) {
    if(frame->input_cut())return develop.neutral()?full:output::displayFrame(frame,{},develop);
    if(!develop.neutral())return output::displayFrame(frame,geometry,develop);
    if(geometry.quarterTurns==0&&!geometry.flipHorizontal&&!geometry.flipVertical&&geometry.straightenDegrees==0)
        return output::displayImage(full,geometry.crop);
    return output::displayFrame(frame,geometry);
}
desktop::InputGeometry filmGeometry(const output::Geometry& g) {
    return {g.crop.x(),g.crop.y(),g.crop.width(),g.crop.height(),g.straightenDegrees,g.quarterTurns,g.flipHorizontal,g.flipVertical};
}
output::ExportOptions exportOptions(int format,int quality,int maxEdge) {
    if(format<0||format>3||quality<1||quality>100||maxEdge<0||maxEdge>100000)
        throw std::runtime_error("Invalid export options");
    return {static_cast<output::Format>(format),quality,maxEdge};
}
}
QImage PreviewController::imageFromFrame(const desktop::FramePtr& frame) {
    if(frame->prophoto())return output::displayFrame(frame,{});
    // QImage is implicitly shared; the cleanup holder keeps the independently
    // owned engine result and its display bytes alive on the scene-graph side.
    auto held=std::make_unique<desktop::FramePtr>(frame);
    QImage image(frame->bgra8.data(),int(frame->width()),int(frame->height()),
                 qsizetype(frame->width())*4,QImage::Format_ARGB32,
                 [](void* p){delete static_cast<desktop::FramePtr*>(p);},held.get());
    if(image.isNull()) throw std::runtime_error("Cannot create the display image");
    held.release();
    image.setColorSpace(QColorSpace::SRgb);
    return image;
}

PreviewController::PreviewController(std::filesystem::path resources,QObject* parent,ImageFactory factory,QString editDirectory,ViewFactory viewFactory)
    :QObject(parent),resources_(resources),
     edit_store_(editDirectory.isEmpty()?QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/Edits":editDirectory),
     image_factory_(factory?std::move(factory):ImageFactory(imageFromFrame)) {
    view_factory_=std::move(viewFactory);
    auto_apply_.setSingleShot(true);auto_apply_.setInterval(180);
    connect(&auto_apply_,&QTimer::timeout,this,[this]{if(dirty())apply();});
    save_timer_.setSingleShot(true);save_timer_.setInterval(350);
    connect(&save_timer_,&QTimer::timeout,this,&PreviewController::flushEdits);
    try {
        const auto materialDirectory=editDirectory.isEmpty()?QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/Materials":QDir(editDirectory).absoluteFilePath("Materials");
        materials_=std::make_unique<MaterialLibrary>(QString::fromStdWString(resources.wstring()),materialDirectory);
        resources_=std::filesystem::path(materials_->runtimeResources().toStdWString());
    }catch(const std::exception& e){busy_=false;failed_=true;status_=QStringLiteral("材料库无法启动，现有文件未覆盖：")+text(e.what());QTimer::singleShot(0,this,[this]{emit stateChanged();emit initialized(false);});return;}
    worker_=std::thread([this,resources=resources_]{workerLoop(resources);});
}
PreviewController::~PreviewController() {
    auto_apply_.stop();
    endEditGesture();flushEdits();
    if(cancel_batch_)cancel_batch_->store(true);
    ++generation_;
    {std::lock_guard lock(mutex_);stopping_=true;jobs_.clear();}
    wake_.notify_one();
    if(worker_.joinable()) worker_.join();
}
void PreviewController::workerLoop(std::filesystem::path resources) {
    std::unique_ptr<desktop::PreviewHost> host;
    Reply init;init.operation="init";
    try {host=std::make_unique<desktop::PreviewHost>(resources,true);init.catalog=host->catalog();}
    catch(const std::exception& e){init.error=text(e.what());}
    catch(...){init.error=QStringLiteral("Unknown exception while initializing render engine");}
    deliver(std::move(init));
    if(!host)return;
    for(;;) {
        Task task;
        {std::unique_lock lock(mutex_);wake_.wait(lock,[&]{return stopping_||!jobs_.empty();});
         if(stopping_)break;task=std::move(jobs_.front());jobs_.pop_front();}
        Reply reply;reply.generation=task.generation;reply.settings_revision=task.revision;reply.operation=task.operation;
        try { reply=task.job(*host); }
        catch(const std::exception& e){reply.error=task.operation+": "+text(e.what());}
        catch(...){reply.error=task.operation+QStringLiteral(": Unknown background exception");}
        reply.generation=task.generation;reply.settings_revision=task.revision;reply.operation=task.operation;
        deliver(std::move(reply));
    }
}
void PreviewController::deliver(Reply reply) {
    // Tone conversion of a large working-space image can outlast a display
    // frame. Prepare immutable QImages here on the worker, not in accept().
    // Geometry/develop are captured with the job: never read mutable UI state.
    if(reply.frame&&reply.error.isEmpty()) {
        try {
            reply.full_image=image_factory_(reply.frame);
            if(reply.full_image.isNull())throw std::runtime_error("Cannot create the display image");
            prepareView(reply,reply.frame,reply.full_image);
        }catch(const std::exception& e){reply.error=text(e.what());}
        catch(...){reply.error=reply.operation+QStringLiteral(": Unknown presentation exception");}
    }
    QMetaObject::invokeMethod(this,[this,reply=std::move(reply)]() mutable {accept(std::move(reply));},Qt::QueuedConnection);
}
void PreviewController::submit(Job job,QString status,QString operation) {
    // Direct signal handlers may edit controls synchronously. The envelope
    // must describe the captured job, not settings after stateChanged returns.
    Task task{std::move(job),generation_,settings_revision_,operation};
    auto_apply_.stop();active_operation_=std::move(operation);
    busy_=true;failed_=false;status_=std::move(status);emit stateChanged();emit historyChanged();
    {std::lock_guard lock(mutex_);jobs_.push_back(std::move(task));}
    wake_.notify_one();
}
void PreviewController::accept(Reply reply) {
    if(reply.operation=="view"){acceptView(std::move(reply));return;}
    if(reply.generation!=generation_)return; // a retired generation never replaces the canvas
    const bool superseded=reply.operation=="render"&&reply.settings_revision!=settings_revision_;
    busy_=false;active_operation_.clear();failed_=!superseded&&!reply.error.isEmpty();
    if(superseded) {
        // The worker may finish an earlier choice after the next click. Keep
        // both the displayed frame and the newest desired controls; never flash
        // the retired result or restore its settings over the user's last edit.
        status_=dirty()?QStringLiteral("正在更新预览…"):QStringLiteral("预览已是当前选择");
    } else if(failed_) {
        status_=reply.error;
        if(frame_&&reply.operation!="export"&&reply.operation!="batch-export")restoreSettings();
    } else if(reply.operation=="init") {
        ready_=true;catalog_=std::move(reply.catalog);decorateCatalog(catalog_);films_=list(catalog_.films,materials_.get());papers_=list(catalog_.papers,materials_.get());
        restoreSettings();emit catalogChanged();status_=QStringLiteral("就绪 · 打开 RAW、JPG 或 PNG 开始");
        emit materialsChanged();
    } else if(reply.operation=="materials") {
        const auto desired=settings();const auto mode=decode_mode_?desktop::DecodeMode::headroom:desktop::DecodeMode::compatible16;
        const auto retainMissing=[&](auto& next,const auto& previous,const std::string& id){if(std::none_of(next.begin(),next.end(),[&](const auto& s){return s.id==id;})){const auto old=std::find_if(previous.begin(),previous.end(),[&](const auto& s){return s.id==id;});if(old!=previous.end())next.push_back(*old);}};
        retainMissing(reply.catalog.films,catalog_.films,desired.film_stock);retainMissing(reply.catalog.papers,catalog_.papers,desired.print_stock);
        catalog_=std::move(reply.catalog);decorateCatalog(catalog_);films_=list(catalog_.films,materials_.get());papers_=list(catalog_.papers,materials_.get());
        edit_store_.updateCatalog(catalog_);
        for(auto& item:library_)if(item.material_blocked){const auto restored=edit_store_.load(item.url,catalog_);item.material_blocked=restored.material_unavailable;item.save_blocked=restored.blocked;item.save_warning=restored.warning;if(restored.state){item.settings=restored.state->settings;item.mode=restored.state->mode;item.geometry=restored.state->geometry;item.develop=restored.state->develop;item.has_stored=true;}}
        loadSettings(desired,mode);emit catalogChanged();emit materialsChanged();status_=QStringLiteral("材料目录已更新；历史版本保留，未替换当前照片");
    } else if(reply.operation=="scene-measure"||reply.operation=="scene-fit") {
        scene_report_=reply.scene;scene_reference_=frame_;measured_settings_=settings();
        const auto fit=scene_report_.value("fit").toMap();
        if(reply.apply_scene&&fit.value("valid").toBool()&&fit.contains("params_delta")) {
            const auto delta=fit.value("params_delta").toMap();
            auto p=extra_settings_.placement;
            p.active=delta.value("scene_latitude_active").toBool();p.norm=delta.value("scene_latitude_norm").toString().toStdString();
            p.highlight_knee=delta.value("scene_latitude_highlight_knee").toDouble();p.highlight_room=delta.value("scene_latitude_highlight_room").toDouble();
            p.shadow_knee=delta.value("scene_latitude_shadow_knee").toDouble();p.shadow_room=delta.value("scene_latitude_shadow_room").toDouble();
            p.rolloff=delta.value("scene_latitude_rolloff").toDouble();p.max_lift=delta.value("scene_latitude_max_lift").toDouble();
            p.highlight_pull_back=fit.value("highlight").toMap().value("pull_back").toDouble();
            p.shadow_pull_back=fit.value("shadow").toMap().value("pull_back").toDouble();
            if(!std::isfinite(p.highlight_pull_back)||!std::isfinite(p.shadow_pull_back)||p.highlight_pull_back<0||p.highlight_pull_back>8||p.shadow_pull_back<0||p.shadow_pull_back>8) {
                status_=QStringLiteral("自动建议超出面板的 0–8 档范围；原有曲线保持不变");
            }else if(p!=extra_settings_.placement) {
                extra_settings_.placement=std::move(p);changed("scenePlacement");measured_settings_=settings();
            }else status_=QStringLiteral("拟合与当前设置相同，无需重新渲染");
        }else status_=fit.value("valid").toBool()?QStringLiteral("场景测量完成；建议尚未应用"):QStringLiteral("拟合被拒绝，原有曲线保持不变");
    } else if(reply.frame) {
        // Publish the worker-prepared images atomically with frame/settings.
        try {
            auto next=std::move(reply.full_image);
            if(next.isNull()||reply.visible_image.isNull())throw std::runtime_error("Cannot create the display image");
            const auto geometry=reply.operation=="open"?reply.geometry:geometry_;
            const auto develop=reply.operation=="open"?reply.develop:develop_;
            auto visible=std::move(reply.visible_image);
            frame_=std::move(reply.frame);full_image_=std::move(next);image_=std::move(visible);
            original_image_=std::move(reply.original_image);uncropped_image_=std::move(reply.uncropped_image);
            view_pending_=false;
            geometry_=geometry;
            develop_=develop;
            if(reply.operation=="open")active_index_=reply.library_index;
        }
        catch(const std::exception& e){failed_=true;status_=text(e.what());if(frame_)restoreSettings();}
        if(!failed_) {
            if(reply.operation=="process") {
                loadSettings(frame_->settings,frame_->decode_mode);
                saveActiveSettings(true);save_timer_.start();
            } else restoreSettings();
            updateThumbnail();
            status_=QStringLiteral("渲染完成 %1 秒%2")
                .arg(frame_->timings.render_ms/1000,0,'f',2)
                .arg(frame_->reprint()?QStringLiteral(" · 已复用胶片"):QString());
            emit frameChanged();emit geometryChanged();if(reply.operation=="open")emit sourceOpened();
        }
    } else if(reply.operation=="export") status_=reply.message.isEmpty()?QStringLiteral("已导出当前照片"):reply.message;
    else if(reply.operation=="batch-export")status_=reply.message;
    if(failed_&&reply.operation=="open"&&reply.library_index>=0&&reply.library_index<int(library_.size()))
        library_[reply.library_index].error=status_;
    if(failed_&&reply.operation!="export"&&reply.operation!="batch-export"&&active_index_>=0){
        auto& history=library_[active_index_].undo;
        while(!history.empty()&&sameState(history.back(),currentState()))history.pop_back();
    }
    if(reply.operation=="batch-export")cancel_batch_.reset();
    emit libraryChanged();
    emit historyChanged();
    emit stateChanged();
    if(reply.operation=="init")emit initialized(!failed_);
    else emit operationFinished(reply.operation,!failed_);
    // Export holds an immutable displayed frame. An edit made just before it
    // began still needs applying, including after an overwrite refusal.
    if((superseded||reply.operation=="export"||reply.operation=="batch-export"||reply.operation=="materials")&&!busy_)schedulePreview();
    if(view_pending_&&!busy_&&!view_busy_)requestView();
}

void PreviewController::prepareView(Reply& reply,const desktop::FramePtr& frame,const QImage& full) {
    const auto before=worker_uncropped_reference_.lock();
    const bool sourceUnchanged=before&&&before->original()==&frame->original();
    const bool reuse=frame->input_cut()?(sourceUnchanged&&before->input_cut()):
        before==frame&&worker_uncropped_develop_==reply.develop;
    if(!reuse||worker_uncropped_image_.isNull()){
        if(frame->input_cut())worker_uncropped_image_=output::originalFrame(frame,{},true);
        else if(reply.develop.neutral())worker_uncropped_image_=full;
        else worker_uncropped_image_=output::displayFrame(frame,{},reply.develop);
    }
    worker_uncropped_reference_=frame;worker_uncropped_develop_=reply.develop;
    reply.uncropped_image=worker_uncropped_image_;
    if(view_factory_)reply.visible_image=view_factory_(frame,full,reply.geometry,reply.develop);
    else if(!frame->input_cut()&&reply.geometry.straightenDegrees==0)
        // Crop/orthogonal permutations commute exactly with a pointwise
        // colour transform and quantization. Bilinear straighten does not.
        reply.visible_image=output::displayImage(reply.uncropped_image,reply.geometry);
    else reply.visible_image=visibleImage(frame,full,reply.geometry,reply.develop);
    const auto previous=worker_original_reference_.lock();
    const bool newSource=!previous||&previous->original()!=&frame->original()||
        previous->input_cut()!=frame->input_cut()||(frame->input_cut()&&previous!=frame);
    if(newSource){worker_original_working_=output::originalWorkingFrame(frame);worker_original_neutral_={};}
    if(newSource||worker_original_geometry_!=reply.geometry||worker_original_image_.isNull()) {
        if(!frame->input_cut()&&reply.geometry.straightenDegrees==0){
            if(worker_original_neutral_.isNull())worker_original_neutral_=output::displayWorkingImage(worker_original_working_,frame,{});
            worker_original_image_=output::displayImage(worker_original_neutral_,reply.geometry);
        }else worker_original_image_=output::displayWorkingImage(worker_original_working_,frame,frame->input_cut()?output::Geometry{}:reply.geometry);
        worker_original_geometry_=reply.geometry;
    }
    worker_original_reference_=frame;reply.original_image=worker_original_image_;
}

void PreviewController::requestView() {
    view_pending_=true;++view_revision_;
    if(view_busy_||busy_||!frame_)return;
    const auto held=frame_;const auto full=full_image_;const auto geometry=geometry_;const auto develop=develop_;
    const auto generation=generation_,revision=view_revision_;
    Task task{[this,held,full,geometry,develop,revision](auto&){
        Reply reply;reply.view_frame=held;reply.geometry=geometry;reply.develop=develop;reply.view_revision=revision;
        try{prepareView(reply,held,full);}
        catch(const std::exception& e){reply.error=text(e.what());}
        catch(...){reply.error=QStringLiteral("Unknown background view exception");}
        return reply;
    },generation,settings_revision_,"view"};
    {std::lock_guard lock(mutex_);jobs_.push_back(std::move(task));}
    view_busy_=true;view_pending_=false;wake_.notify_one();
    emit stateChanged();
}
void PreviewController::acceptView(Reply reply) {
    view_busy_=false;
    // Export changes the engine generation without replacing the displayed
    // frame. The immutable frame identity and view revision own this lane.
    const bool current=reply.view_frame==frame_&&reply.view_revision==view_revision_;
    if(current) {
        if(reply.error.isEmpty()) {
            image_=std::move(reply.visible_image);original_image_=std::move(reply.original_image);uncropped_image_=std::move(reply.uncropped_image);
            if(!busy_&&!failed_)status_=QStringLiteral("预览已更新");
            updateThumbnail();emit frameChanged();
        }else{failed_=true;status_=reply.error;}
    }
    // A newer desired view replaces the obsolete one; it never flashes an
    // intermediate image, clears an engine job, or starts a second engine render.
    if(view_pending_&&!busy_&&!view_busy_)requestView();
    emit stateChanged();
}
desktop::RenderSettings PreviewController::settings() const {
    desktop::RenderSettings s=extra_settings_;
    s.engine_resources_sha256=catalog_.engine_resources_sha256;
    if(film_index_>=0&&std::size_t(film_index_)<catalog_.films.size()){s.film_stock=catalog_.films[film_index_].id;s.film_profile_sha256=catalog_.films[film_index_].profile_sha256;}
    if(paper_index_>=0&&std::size_t(paper_index_)<catalog_.papers.size()){s.print_stock=catalog_.papers[paper_index_].id;s.print_profile_sha256=catalog_.papers[paper_index_].profile_sha256;}
    s.print_exposure=std::exp2(-exposure_ev_);return s;
}
void PreviewController::restoreSettings() {
    const auto s=frame_?frame_->settings:desktop::RenderSettings{};
    loadSettings(s,frame_?frame_->decode_mode:desktop::DecodeMode::compatible16);
    saveActiveSettings();
}
void PreviewController::loadSettings(const desktop::RenderSettings& s,desktop::DecodeMode mode) {
    extra_settings_=s;
    for(std::size_t i=0;i<catalog_.films.size();++i)if(catalog_.films[i].id==s.film_stock)film_index_=int(i);
    for(std::size_t i=0;i<catalog_.papers.size();++i)if(catalog_.papers[i].id==s.print_stock)paper_index_=int(i);
    exposure_ev_=-std::log2(s.print_exposure);
    decode_mode_=mode==desktop::DecodeMode::headroom?1:0;
    emit settingsChanged();
}
bool PreviewController::dirty() const {
    if(!frame_)return false;
    return !sameSettings(settings(),frame_->settings)||
        ((settings().edge.active||settings().date.active)&&filmGeometry(geometry_)!=frame_->input_geometry())||
        decode_mode_!=(frame_->decode_mode==desktop::DecodeMode::headroom?1:0);
}
bool PreviewController::filmIsPositive() const {
    return film_index_>=0&&std::size_t(film_index_)<catalog_.films.size()&&catalog_.films[film_index_].is_positive;
}
bool PreviewController::filmIsMonochrome() const {
    return film_index_>=0&&std::size_t(film_index_)<catalog_.films.size()&&catalog_.films[film_index_].is_monochrome;
}
QString PreviewController::fileName() const {return frame_?QString::fromStdWString(frame_->source.filename().wstring()):QString();}
QUrl PreviewController::sourceFolder() const {return frame_?QUrl::fromLocalFile(QString::fromStdWString(frame_->source.parent_path().wstring())):QUrl();}
QString PreviewController::processDiagnostics() const {
    if(!frame_)return {};
    QJsonObject report;
    if(!frame_->process_report.empty())report=QJsonDocument::fromJson(QByteArray::fromStdString(frame_->process_report)).object();
    else {
        report["resolved"]=QJsonDocument::fromJson(QByteArray::fromStdString(frame_->resolved_params)).object();
        report["render_progress"]=QJsonDocument::fromJson(QByteArray::fromStdString(frame_->render_progress)).object();
        report["reason"]="Current rendered frame; derived exposure is informational, never an extra compensation";
    }
    report["source"]=QString::fromStdWString(frame_->source.wstring());
    return QString::fromUtf8(QJsonDocument(report).toJson(QJsonDocument::Indented));
}
QVariantMap PreviewController::metadata() const {
    if(!frame_)return {};
    return {{"width",image_.width()},{"height",image_.height()},
        {"sourceWidth",frame_->source_width()},{"sourceHeight",frame_->source_height()},
        {"previewWidth",frame_->width()},{"previewHeight",frame_->height()},
        {"camera",frame_->metadata.input_kind=="raw"?text(frame_->metadata.make+" "+frame_->metadata.model):text(frame_->metadata.input_kind).toUpper()+QStringLiteral(" 图片")},
        {"inputKind",text(frame_->metadata.input_kind)},{"inputProfile",text(frame_->metadata.input_profile)},
        {"decodeMs",frame_->timings.decode_ms},{"renderMs",frame_->timings.render_ms},
        {"totalMs",frame_->timings.total_ms},{"cached",frame_->reprint()},{"scanFilm",frame_->scan_film},
        {"mode",frame_->decode_mode==desktop::DecodeMode::headroom?"headroom":"compatible16"}};
}
QVariantMap PreviewController::histogram() const {
    if(histogram_key_==image_.cacheKey()&&!histogram_cache_.isEmpty())return histogram_cache_;
    const auto counts=output::displayHistogram(image_);
    QVariantList channels;
    for(const auto& row:counts.bins) {
        QVariantList bins;bins.reserve(256);
        for(auto count:row)bins.push_back(QVariant::fromValue<qulonglong>(count));
        channels.push_back(QVariant(bins));
    }
    histogram_key_=image_.cacheKey();
    histogram_cache_={{"channels",channels},{"samples",QVariant::fromValue<qulonglong>(counts.samples)},{"stride",counts.stride}};
    return histogram_cache_;
}
void PreviewController::schedulePreview(){if(!dialog_open_&&gesture_key_!="straightenDegrees"&&frame_&&dirty()&&!auto_apply_.isActive())auto_apply_.start();}
void PreviewController::setDialogOpen(bool open){
    if(dialog_open_==open)return;
    dialog_open_=open;
    if(open){endEditGesture();auto_apply_.stop();}else schedulePreview();
    emit stateChanged();emit historyChanged();
}
void PreviewController::changed(const QString& editKey){
    if(gesture_index_>=0&&gesture_key_!=editKey)endEditGesture();
    saveActiveSettings(true);
    ++settings_revision_;auto_apply_.stop();schedulePreview();
    if(frame_)status_=QStringLiteral("正在更新预览…");
    failed_=false;emit settingsChanged();emit stateChanged();
}
void PreviewController::setFilmIndex(int v){
    if(!settingsEditable()||v<0||v>=films_.size()||v==film_index_)return;
    // Change film and its compatible paper as one history entry. Never let an
    // intermediate orange silver-on-colour render reach the canvas or store.
    const auto& film=catalog_.films[v];
    if(!films_[v].toMap().value("enabled",true).toBool())return;
    int nextPaper=paper_index_;
    if(!film.is_positive&&(nextPaper<0||nextPaper>=int(catalog_.papers.size())||
        catalog_.papers[nextPaper].is_monochrome!=film.is_monochrome)) {
        const std::string preferred=film.is_monochrome?"ilford_multigrade_iv_rc":"kodak_portra_endura";
        nextPaper=-1;
        for(int i=0;i<int(catalog_.papers.size());++i)
            if(catalog_.papers[i].is_monochrome==film.is_monochrome) {
                if(nextPaper<0)nextPaper=i;
                if(catalog_.papers[i].id==preferred){nextPaper=i;break;}
            }
        if(nextPaper<0)return; // incomplete bundle: do not publish an invalid pair
    }
    film_index_=v;paper_index_=nextPaper;changed("filmIndex");
}
void PreviewController::setPaperIndex(int v){
    if(!settingsEditable()||filmIsPositive()||v<0||v>=papers_.size()||v==paper_index_||
        catalog_.papers[v].is_monochrome!=filmIsMonochrome()||!papers_[v].toMap().value("enabled",true).toBool())return;
    paper_index_=v;changed("paperIndex");
}
void PreviewController::setDecodeMode(int v){if(!settingsEditable()||extra_settings_.pair.active||(frame_&&frame_->metadata.input_kind!="raw")||v<0||v>1||v==decode_mode_)return;decode_mode_=v;changed("decodeMode");}
void PreviewController::setExposureEv(double v){if(!settingsEditable()||filmIsPositive()||!std::isfinite(v)||v< -2||v>2||v==exposure_ev_)return;exposure_ev_=v;changed("exposureEv");}
void PreviewController::resetSettings(){if(!settingsEditable())return;resetPostAdjustments();desktop::RenderSettings reset;reset.pair=extra_settings_.pair;if(reset.pair.active)reset.edge.format="135_half";const auto mode=reset.pair.active&&decode_mode_?desktop::DecodeMode::headroom:desktop::DecodeMode::compatible16;loadSettings(reset,mode);changed();}
void PreviewController::openRaw(const QUrl& url) {
    // Changing sources and exporting remain single flight. Parameter edits
    // can be coalesced during a render, but cannot retarget an in-flight open.
    if(!ready_||busy_||!url.isLocalFile())return;
    saveActiveSettings();
    const int index=addFile(url);if(index<0)return;
    // A known photo restores its own saved settings. A genuinely new photo
    // still starts with the current preset; opening never overwrites a sidecar.
    openItem(index);
}
void PreviewController::openItem(int index) {
    endEditGesture();
    const auto item=library_.at(index);const auto p=path(item.url);
    const auto gen=++generation_;
    submit([p,item,gen,index](auto& host){Reply r;r.generation=gen;r.operation="open";r.library_index=index;r.geometry=item.geometry;r.develop=item.develop;
        try{if(item.material_blocked)throw std::runtime_error((QStringLiteral("材料缺失或版本不匹配，不会替换成其他胶片：")+item.save_warning).toStdString());r.frame=desktop::pair_project::open(host,p,item.mode,item.settings,filmGeometry(item.geometry));}catch(const std::exception& e){r.error=text(e.what());}return r;},QStringLiteral("正在解码并渲染照片…"),"open");
}
void PreviewController::apply() {
    if(!frame_||busy_||dialog_open_)return;
    const auto s=settings();const auto shown=frame_;const auto gen=++generation_;const auto revision=settings_revision_;
    const auto mode=decode_mode_?desktop::DecodeMode::headroom:desktop::DecodeMode::compatible16;
    const auto cut=filmGeometry(geometry_);const auto geometry=geometry_;const auto develop=develop_;
    submit([s,shown,gen,revision,mode,cut,geometry,develop](auto& host){Reply r;r.generation=gen;r.settings_revision=revision;r.operation="render";r.geometry=geometry;r.develop=develop;
        try {
            // Publication can fail after a successful worker open (for example,
            // a display allocation failure). Reconcile against the image the
            // user is actually editing before reusing the worker's session.
            const auto active=host.current();
            r.frame=active&&active->source==shown->source&&active->decode_mode==mode
                ?host.render(s,cut):desktop::pair_project::open(host,shown->source,mode,s,cut);
        }catch(const std::exception& e){r.error=text(e.what());}return r;},QStringLiteral("正在更新胶片效果…"),"render");
}
void PreviewController::exportTiff(const QUrl& url) {
    exportImage(url,0,95,0);
}
void PreviewController::exportImage(const QUrl& url,int format,int quality,int maxLongEdge) {
    if(!frame_||busy_||!url.isLocalFile())return;
    const auto p=path(url);const auto held=frame_;const auto gen=++generation_;const auto geometry=geometry_;const auto resources=resources_;const auto develop=develop_;
    submit([p,held,gen,geometry,develop,resources,format,quality,maxLongEdge](auto& host){Reply r;r.generation=gen;r.operation="export";
        try{auto options=exportOptions(format,quality,maxLongEdge);options.develop=develop;
            if(std::filesystem::exists(p))throw std::runtime_error("The export file already exists");
            output::exportFrame(host.render_original(held),p,geometry,options,resources);
            r.message=QStringLiteral("已导出：")+QString::fromStdWString(p.wstring());
        }catch(const std::exception& e){r.error=text(e.what());}return r;},QStringLiteral("正在按原尺寸渲染并导出…"),"export");
}

QImage PreviewController::uncroppedImage() const {
    return uncropped_image_;
}
QImage PreviewController::originalImage() const {
    return original_image_;
}
QVariantMap PreviewController::postAdjustments() const {
    if(post_cache_key_&&*post_cache_key_==develop_)return post_cache_;
    const auto& d=develop_;
    QVariantMap result{{"enabled",d.enabled},{"temperature",d.temperature},{"tint",d.tint},{"exposure",d.exposure},
        {"contrast",d.contrast},{"brightness",d.brightness},{"saturation",d.saturation},
        {"highlights",d.highlights},{"shadows",d.shadows},{"blackPoint",d.blackPoint},{"whitePoint",d.whitePoint},
        {"vignetteAmount",d.vignetteAmount},{"vignetteMidpoint",d.vignetteMidpoint},{"workingSpace",d.legacySrgb?"sRGB（旧编辑）":"ProPhoto RGB"}};
    QVariantList curves,tables;
    for(const auto& curve:d.curves) {
        QVariantList points,samples;
        for(const auto& p:curve)points.append(QVariantMap{{"x",p.x},{"y",p.y}});
        for(unsigned i=0;i<256;++i)samples.append(output::evaluateCurve(curve,double(i)/255));
        curves.append(QVariant(points));tables.append(QVariant(samples));
    }
    result["curves"]=curves;result["curveTables"]=tables;
    result["exposureModel"]=d.legacyExposure?"gamma22-v1":"linear-v1";
    for(unsigned i=0;i<4;++i) {
        const auto prefix=QString("zone%1").arg(i);
        result[prefix+"Hue"]=d.zones[i].hue;result[prefix+"Saturation"]=d.zones[i].saturation;result[prefix+"Luminance"]=d.zones[i].luminance;
    }
    post_cache_key_=d;post_cache_=result;return post_cache_;
}
void PreviewController::setPostAdjustment(const QString& name,const QVariant& value) {
    if(!settingsEditable()||busy_||!frame_)return;
    auto next=develop_;
    if(name=="enabled")next.enabled=value.toBool();
    else if(name.startsWith("curve")) {
        bool ok=false;const int channel=name.mid(5).toInt(&ok);
        if(!ok||channel<0||channel>=5||value.metaType().id()!=QMetaType::QVariantList)return;
        auto& curve=next.curves[channel];curve.clear();
        for(const auto& point:value.toList()) {
            const auto p=point.toMap();bool xok=false,yok=false;
            const auto x=p.value("x").toDouble(&xok),y=p.value("y").toDouble(&yok);
            if(!xok||!yok)return;curve.push_back({x,y});
        }
    }
    else {
        bool ok=false;const auto v=value.toDouble(&ok);if(!ok||!std::isfinite(v))return;
        double* field=nullptr;
        if(name=="temperature")field=&next.temperature;else if(name=="tint")field=&next.tint;
        else if(name=="exposure")field=&next.exposure;else if(name=="contrast")field=&next.contrast;
        else if(name=="brightness")field=&next.brightness;else if(name=="saturation")field=&next.saturation;
        else if(name=="highlights")field=&next.highlights;else if(name=="shadows")field=&next.shadows;
        else if(name=="blackPoint")field=&next.blackPoint;else if(name=="whitePoint")field=&next.whitePoint;
        else if(name=="vignetteAmount")field=&next.vignetteAmount;else if(name=="vignetteMidpoint")field=&next.vignetteMidpoint;
        else if(name.startsWith("zone")&&name.size()>5) {
            bool ok=false;const int z=name.mid(4,1).toInt(&ok);if(!ok||z<0||z>=4)return;
            if(name.mid(5)=="Hue")field=&next.zones[z].hue;
            else if(name.mid(5)=="Saturation")field=&next.zones[z].saturation;
            else if(name.mid(5)=="Luminance")field=&next.zones[z].luminance;
            else return;
        }
        else return;
        *field=v;
    }
    if(next==develop_)return;
    try {
        output::validateDevelop(next);
        if(gesture_index_>=0&&gesture_key_!=name)endEditGesture();
        develop_=next;saveActiveSettings(true);requestView();
        failed_=false;status_=QStringLiteral("正在更新显影后预览…");
        emit settingsChanged();emit stateChanged();
    }catch(const std::exception& e){failed_=true;status_=text(e.what());emit stateChanged();}
}
void PreviewController::setCurve(int channel,const QVariantList& points) {
    if(channel<0||channel>=5)return;
    setPostAdjustment(QString("curve%1").arg(channel),points);
}
void PreviewController::resetPostAdjustments() {
    if(!settingsEditable()||busy_||develop_==output::Develop{})return;
    try {
        endEditGesture();develop_={};saveActiveSettings(true);if(frame_)requestView();
        emit settingsChanged();emit stateChanged();
    }catch(const std::exception& e){failed_=true;status_=text(e.what());emit stateChanged();}
}

QVariantMap PreviewController::adjustments() const {
    const auto& s=extra_settings_;
    return {{"filmExposureEv",s.film_exposure_ev},{"filmFormatMm",s.film_format_mm},
        {"grainActive",s.grain_active},{"grainAmount",s.grain_amount},
        {"halationActive",s.halation_active},{"halationAmount",s.halation_amount},
        {"glareActive",s.glare_active},{"glareAmount",s.glare_amount},
        {"yFilterShift",s.y_filter_shift},{"mFilterShift",s.m_filter_shift}};
}
QVariantMap PreviewController::filmFrame() const {
    const auto& e=extra_settings_.edge;const auto& d=extra_settings_.date;
    QVariantMap result{{"active",e.active},{"format",text(e.format)},{"mode",text(e.mode)},{"gate",text(e.gate)},
        {"holes",text(e.holes)},{"carrier",text(e.carrier)},{"cameraSeed",e.camera_seed},{"frameSeed",e.frame_seed},
        {"frameNumber",e.frame_number},{"fog",e.fog},{"leaks",e.leaks},
        {"dateActive",d.active},{"dateText",text(d.text)},{"dateStyle",text(d.style)},{"dateCorner",text(d.corner)},
        {"dateExposure",d.exposure_ev},{"dateSize",d.size}};
    const auto& pair=extra_settings_.pair;result["pairActive"]=pair.active;result["pairTurned"]=pair.turned;result["pairSpacing"]=pair.spacing_mm;
    for(unsigned i=0;i<2;++i){const auto prefix=QString("pair%1").arg(i);const auto& h=pair.holes[i];result[prefix+"Scale"]=h.scale;result[prefix+"X"]=h.x;result[prefix+"Y"]=h.y;result[prefix+"Exposure"]=h.exposure_ev;result[prefix+"Turns"]=h.turns;}return result;
}
void PreviewController::setFilmFrame(const QString& name,const QVariant& value) {
    if(!settingsEditable())return;
    auto next=extra_settings_;auto& e=next.edge;auto& d=next.date;
    const auto v=value.toString();
    if(name=="active")e.active=value.toBool();
    else if(name=="pairTurned"){if(!next.pair.active)return;next.pair.turned=value.toBool();}
    else if(name=="dateActive") {d.active=value.toBool();if(d.active&&d.text.empty())d.text=QDate::currentDate().toString("yy MM dd").toStdString();}
    else if(name=="dateText") {if(v.size()>64||!QRegularExpression("^[0-9 .:/\\x27-]*$").match(v).hasMatch())return;d.text=v.toStdString();}
    else if(name=="format") {if(next.pair.active||!QStringList{"135","135_half","120_645","120_6x6","120_6x7","120_6x8","120_6x9","135_xpan","120_6x12","120_6x17"}.contains(v))return;e.format=v.toStdString();}
    else if(name=="mode"){if(v!="strip"&&v!="filed")return;e.mode=v.toStdString();}
    else if(name=="holes"){if(v!="white"&&v!="black")return;e.holes=v.toStdString();}
    else if(name=="carrier"){if(!QStringList{"none","black","open"}.contains(v))return;e.carrier=v.toStdString();}
    else if(name=="gate"){if(!QStringList{"auto","square","rounded","eared","shouldered","kicked"}.contains(v))return;e.gate=v.toStdString();}
    else if(name=="dateStyle"){if(!QStringList{"lcd","dots","data"}.contains(v))return;d.style=v.toStdString();}
    else if(name=="dateCorner"){if(!QStringList{"br","bl","tr","tl"}.contains(v))return;d.corner=v.toStdString();}
    else {
        bool ok=false;const double n=value.toDouble(&ok);if(!ok||!std::isfinite(n))return;
        if(name.startsWith("pair")) {
            if(!next.pair.active)return;
            if(name=="pairSpacing"){if(e.active||n<.5||n>2)return;next.pair.spacing_mm=n;}
            else {if(name.size()<6||(name[4]!='0'&&name[4]!='1'))return;auto& h=next.pair.holes[name[4].digitValue()];const auto key=name.mid(5);
                if(key=="Scale"){if(n<1||n>4)return;h.scale=n;}else if(key=="X"||key=="Y"){if(n< -1||n>1)return;(key=="X"?h.x:h.y)=n;}
                else if(key=="Exposure"){if(n< -8||n>8)return;h.exposure_ev=n;}else if(key=="Turns"){if(n<0||n>3||n!=std::floor(n))return;h.turns=int(n);}else return;}
        }else if(name=="cameraSeed"||name=="frameSeed"||name=="frameNumber") {
            if(n<0||n>2147483647||n!=std::floor(n)||(name=="frameNumber"&&n>99))return;
            if(name=="cameraSeed")e.camera_seed=int(n);else if(name=="frameSeed")e.frame_seed=int(n);else e.frame_number=int(n);
        }else if(name=="fog"||name=="leaks"){if(n<0||n>4)return;(name=="fog"?e.fog:e.leaks)=n;}
        else if(name=="dateExposure"){if(n< -2||n>8)return;d.exposure_ev=n;}
        else if(name=="dateSize"){if(n<.4||n>3)return;d.size=n;}
        else return;
    }
    if(e.active&&(e.format=="135_xpan"||e.format=="120_6x12"||e.format=="120_6x17"))d.active=false;
    if(next==extra_settings_)return;
    extra_settings_=std::move(next);changed("filmFrame."+name);
}
void PreviewController::setAdjustment(const QString& name,const QVariant& value) {
    if(!settingsEditable())return;
    auto next=extra_settings_;
    if(name=="grainActive")next.grain_active=value.toBool();
    else if(name=="halationActive")next.halation_active=value.toBool();
    else if(name=="glareActive")next.glare_active=value.toBool();
    else {
        bool ok=false;const double v=value.toDouble(&ok);if(!ok||!std::isfinite(v))return;
        double* field=nullptr;double low=0,high=0;
        if(name=="filmExposureEv"){field=&next.film_exposure_ev;low=-8;high=8;}
        else if(name=="filmFormatMm"){field=&next.film_format_mm;low=4;high=200;}
        else if(name=="grainAmount"){field=&next.grain_amount;high=2;}
        else if(name=="halationAmount"){field=&next.halation_amount;high=4;}
        else if(name=="glareAmount"){field=&next.glare_amount;high=30;}
        else if(name=="yFilterShift"){field=&next.y_filter_shift;low=-1;high=1;}
        else if(name=="mFilterShift"){field=&next.m_filter_shift;low=-1;high=1;}
        else return;
        if(v<low||v>high)return;*field=v;
    }
    if(next==extra_settings_)return;
    extra_settings_=next;changed(name);
}

QVariantMap PreviewController::sceneReport() const {
    auto result=scene_report_;
    const bool available=frame_&&scene_reference_&&
        &frame_->original()==&scene_reference_->original()&&&frame_->prepared()==&scene_reference_->prepared()&&settings()==measured_settings_&&!dirty();
    result["available"]=available;
    result["active"]=extra_settings_.placement.active;
    result["highlightPullBack"]=extra_settings_.placement.highlight_pull_back;
    result["shadowPullBack"]=extra_settings_.placement.shadow_pull_back;
    result["reason"]=!frame_?QStringLiteral("导入照片后测量场景"):
        available?QString():QStringLiteral("请测量当前照片与胶片/相纸；设置改变后需重新测量");
    // Same readout as macOS LatitudeReadout: bin-centre shares and the
    // neutral chain's CIE L* change per stop, relative to its own white.
    const auto medium=result.value("medium").toMap();
    const auto histogram=result.value("scene").toMap().value("histogram").toMap();
    const auto bins=histogram.value(histogram.contains("placed_fractions")?"placed_fractions":"fractions").toList();
    double below=0,above=0,total=0;
    for(qsizetype i=0;i<bins.size();++i) {
        const double centre=histogram.value("lo_ev").toDouble()+(double(i)+.5)/double(bins.size())*
            (histogram.value("hi_ev").toDouble()-histogram.value("lo_ev").toDouble());
        const double v=bins[i].toDouble();total+=v;
        if(centre<medium.value("shadow_ev").toDouble())below+=v;
        else if(centre>medium.value("highlight_ev").toDouble())above+=v;
    }
    result["below"]=below;result["within"]=std::max(0.0,total-below-above);result["above"]=above;
    const auto ev=medium.value("ramp_ev").toList(),y=medium.value("ramp_y").toList();
    const double white=std::max(medium.value("y_white").toDouble(),1e-9);
    const auto lightness=[white](double v){const double t=std::max(v,0.0)/white;return t>.008856?116*std::pow(t,1.0/3)-16:903.3*t;};
    QVariantList separation;double peak=0;
    for(qsizetype i=0;i+1<std::min(ev.size(),y.size());++i)if(ev[i+1].toDouble()>ev[i].toDouble()) {
        const double slope=std::max(0.0,(lightness(y[i+1].toDouble())-lightness(y[i].toDouble()))/(ev[i+1].toDouble()-ev[i].toDouble()));
        peak=std::max(peak,slope);
        separation.append(QVariantMap{{"ev",(ev[i].toDouble()+ev[i+1].toDouble())/2},{"strength",slope}});
    }
    for(auto& value:separation){auto row=value.toMap();row["strength"]=peak>0?row["strength"].toDouble()/peak:0;value=row;}
    result["separation"]=separation;
    return result;
}
void PreviewController::measureScene(){requestScene(extra_settings_.placement.highlight_pull_back,extra_settings_.placement.shadow_pull_back,false,false);}
void PreviewController::process() {
    if(!frame_||busy_||dirty()||dialog_open_)return;
    endEditGesture();
    const auto held=frame_;const auto gen=++generation_;
    submit([held,gen](auto& host){
        Reply r;r.generation=gen;r.operation="process";
        try {
            r.frame=host.process(held);
        }catch(const std::exception& e){r.error=text(e.what());}
        return r;
    },QStringLiteral("Processing..."),"process");
}
void PreviewController::fitScene(double h,double s,bool automatic){requestScene(h,s,automatic,true);}
void PreviewController::requestScene(double h,double s,bool automatic,bool applyFit) {
    if(!frame_||busy_||dirty()||dialog_open_||!std::isfinite(h)||!std::isfinite(s)||h<0||h>8||s<0||s>8)return;
    const auto held=frame_;const auto gen=++generation_;const QString operation=applyFit?"scene-fit":"scene-measure";
    QJsonObject request;
    if(!automatic){request["highlight_pull_back"]=h;request["shadow_pull_back"]=s;}
    const auto bytes=QJsonDocument(request).toJson(QJsonDocument::Compact).toStdString();
    submit([held,gen,bytes,operation,applyFit](auto& host){
        Reply r;r.generation=gen;r.operation=operation;r.apply_scene=applyFit;
        try{
            const auto json=host.scene_latitude(held,bytes);
            QJsonParseError error;const auto document=QJsonDocument::fromJson(QByteArray::fromStdString(json),&error);
            if(error.error!=QJsonParseError::NoError||!document.isObject()||!document.object().contains("fit"))
                throw std::runtime_error("Invalid scene measurement reply");
            r.scene=document.object().toVariantMap();
        }catch(const std::exception& e){r.error=text(e.what());}
        return r;
    },QStringLiteral("正在测量真实介质响应并拟合场景…"),operation);
}
void PreviewController::resetScenePlacement() {
    if(!settingsEditable()||!frame_||extra_settings_.placement==desktop::ScenePlacement{})return;
    extra_settings_.placement={};changed("scenePlacement");
}
QVariantList PreviewController::libraryItems() const {
    QVariantList result;
    for(int i=0;i<int(library_.size());++i) {
        const auto& item=library_[i];
        result.push_back(QVariantMap{{"name",QFileInfo(item.url.toLocalFile()).fileName()},
            {"url",item.url},{"thumbnail",item.thumbnail},{"active",i==active_index_},
            {"selected",item.selected},{"error",item.error}});
    }
    return result;
}
int PreviewController::selectedCount() const {
    int count=0;for(const auto& item:library_)count+=item.selected;return count;
}
void PreviewController::saveActiveSettings(bool recordHistory) {
    if(active_index_<0||active_index_>=int(library_.size()))return;
    auto& item=library_[active_index_];const auto desired=settings();
    const auto mode=decode_mode_?desktop::DecodeMode::headroom:desktop::DecodeMode::compatible16;
    // A saved pending edit has no rendered thumbnail yet. Do not let the
    // strip present an older look as if it represented the new settings.
    const bool changed=!sameSettings(item.settings,desired)||item.mode!=mode||item.geometry!=geometry_||item.develop!=develop_;
    if(changed){
        if(recordHistory)recordEdit(item,itemState(item));
        item.thumbnail={};item.pending_save=true;
    }
    item.settings=desired;item.mode=mode;
    item.geometry=geometry_;
    item.develop=develop_;
    if(changed){scheduleSave();emit libraryChanged();emit historyChanged();}
}
void PreviewController::updateThumbnail() {
    if(active_index_<0||active_index_>=int(library_.size()))return;
    auto& item=library_[active_index_];
    item.thumbnail=(dirty()||view_busy_||view_pending_)?QImage{}:image_.scaled(180,120,Qt::KeepAspectRatio,Qt::SmoothTransformation);
    item.error.clear();emit libraryChanged();
}
int PreviewController::addFile(const QUrl& url) {
    if(!url.isLocalFile())return -1;
    const QFileInfo info(url.toLocalFile());
    if(!info.isFile())return -1;
    const auto local=QUrl::fromLocalFile(info.canonicalFilePath());
    for(int i=0;i<int(library_.size());++i)
        if(library_[i].url.toLocalFile().compare(local.toLocalFile(),Qt::CaseInsensitive)==0)return i;
    LibraryItem item;item.url=local;item.settings=settings();item.settings.pair={};
    item.mode=decode_mode_?desktop::DecodeMode::headroom:desktop::DecodeMode::compatible16;
    const auto stored=edit_store_.load(local,catalog_);
    item.save_blocked=stored.blocked;item.save_warning=stored.warning;item.material_blocked=stored.material_unavailable;
    if(stored.state){item.settings=stored.state->settings;item.mode=stored.state->mode;item.geometry=stored.state->geometry;item.develop=stored.state->develop;item.has_stored=true;}
    // A new raster must not inherit the previous RAW's headroom preset.
    const auto suffix=info.suffix().toLower();
    if(!stored.state&&(suffix=="jpg"||suffix=="jpeg"||suffix=="png"))item.mode=desktop::DecodeMode::compatible16;
    // Legacy records did not pin these fields. Resolve their existing IDs at
    // open, before producing a Frame, without rewriting the record on read.
    if(item.settings.film_profile_sha256.empty())for(const auto& stock:catalog_.films)if(stock.id==item.settings.film_stock)item.settings.film_profile_sha256=stock.profile_sha256;
    if(item.settings.print_profile_sha256.empty())for(const auto& stock:catalog_.papers)if(stock.id==item.settings.print_stock)item.settings.print_profile_sha256=stock.profile_sha256;
    if(item.settings.engine_resources_sha256.empty())item.settings.engine_resources_sha256=catalog_.engine_resources_sha256;
    if(desktop::pair_project::isPair(local.toLocalFile())&&!stored.state) {
        try{item.settings.pair=desktop::pair_project::read(local.toLocalFile()).recipe;item.settings.edge.format="135_half";}
        catch(const std::exception& e){item.error=text(e.what());}
    }
    library_.push_back(std::move(item));emit libraryChanged();return int(library_.size())-1;
}
void PreviewController::openFiles(const QVariantList& urls) {
    if(!ready_||busy_)return;
    saveActiveSettings();int first=-1;
    for(const auto& url:urls){const int index=addFile(url.toUrl());if(first<0&&index>=0)first=index;}
    if(first>=0)openItem(first);
}
void PreviewController::selectImage(int index) {
    if(!ready_||busy_||dialog_open_||index<0||index>=int(library_.size())||index==active_index_)return;
    saveActiveSettings();openItem(index);
}
void PreviewController::setItemSelected(int index,bool selected) {
    if(busy_||dialog_open_||index<0||index>=int(library_.size()))return;
    library_[index].selected=selected;emit libraryChanged();
}
void PreviewController::selectAllPhotos(bool selected) {
    if(busy_||dialog_open_)return;
    for(auto& item:library_)item.selected=selected;emit libraryChanged();
}
void PreviewController::createSelectedPair() {
    if(!frame_||busy_||dialog_open_)return;
    try {
        saveActiveSettings();std::array<QString,2> paths;std::array<desktop::DecodeMode,2> modes;desktop::HalfPair pair;pair.active=true;unsigned n=0;
        for(const auto& item:library_)if(item.selected){
            if(n>=2||item.settings.pair.active||desktop::pair_project::isPair(item.url.toLocalFile()))throw std::runtime_error("Select exactly two source photos, not existing pairs");
            paths[n]=item.url.toLocalFile();modes[n]=item.mode;pair.holes[n].geometry=filmGeometry(item.geometry);++n;
        }
        if(n!=2)throw std::runtime_error("Select exactly two source photos");
        const auto directory=QFileInfo(edit_store_.filePath(library_[active_index_].url)).absolutePath()+"/Pairs";
        const auto project=desktop::pair_project::create(directory,paths,modes,pair);
        const int index=addFile(QUrl::fromLocalFile(project));if(index<0)throw std::runtime_error("Cannot import pair project");
        library_[index].settings.pair=pair;library_[index].settings.edge.format="135_half";library_[index].geometry={};library_[index].pending_save=true;
        for(auto& item:library_)item.selected=false;library_[index].selected=true;scheduleSave();openItem(index);
    }catch(const std::exception& e){failed_=true;status_=text(e.what());emit stateChanged();}
}
void PreviewController::syncSelectedSettings(bool includeCrop) {
    if(!frame_||busy_||dialog_open_)return;
    saveActiveSettings();int count=0;
    for(int i=0;i<int(library_.size());++i)if(i!=active_index_&&library_[i].selected){
        auto& item=library_[i];auto next=currentState();
        next.settings.pair=item.settings.pair;if(next.settings.pair.active)next.settings.edge.format="135_half";
        if(item.settings.pair.active){next.geometry={};next.mode=item.mode;}
        const auto suffix=QFileInfo(item.url.toLocalFile()).suffix().toLower();
        if(suffix=="jpg"||suffix=="jpeg"||suffix=="png")next.mode=desktop::DecodeMode::compatible16;
        if(!includeCrop||item.settings.pair.active)next.geometry=item.geometry;
        if(sameState(itemState(item),next))continue;
        // Each recipient owns its own undo history, just like its settings.
        item.undo.push_back(itemState(item));if(item.undo.size()>64)item.undo.erase(item.undo.begin());item.redo.clear();
        item.settings=next.settings;item.mode=next.mode;item.geometry=next.geometry;item.develop=next.develop;item.pending_save=true;
        item.thumbnail={};item.error.clear();++count;
    }
    status_=QStringLiteral("已将当前参数同步至 %1 张照片%2").arg(count)
        .arg(includeCrop?QStringLiteral("（含裁切）"):QStringLiteral("（各自保留裁切）"));
    scheduleSave();emit libraryChanged();emit historyChanged();emit stateChanged();
}
void PreviewController::setCropRect(const QRectF& crop) {
    auto next=geometry_;next.crop=crop;setGeometry(next,"cropRect");
}
void PreviewController::setGeometry(const output::Geometry& geometry,const QString& editKey) {
    if(!frame_||busy_||dialog_open_||extra_settings_.pair.active||geometry==geometry_)return;
    try {
        output::validateGeometry(full_image_.size(),geometry);
        if(gesture_index_>=0&&gesture_key_!=editKey)endEditGesture();
        geometry_=geometry;saveActiveSettings(true);requestView();
        if(settings().edge.active||settings().date.active){++settings_revision_;schedulePreview();}
        failed_=false;emit geometryChanged();emit stateChanged();
    } catch(const std::exception& e){failed_=true;status_=text(e.what());emit stateChanged();}
}
void PreviewController::resetCrop(){setCropRect(QRectF(0,0,1,1));}
void PreviewController::setCropAspect(double ratio) {
    if(!frame_||!std::isfinite(ratio)||ratio<0)return;
    if(ratio==0){resetCrop();return;}
    // Ratios describe the displayed result, even after a quarter turn.
    if(geometry_.quarterTurns%2)ratio=1/ratio;
    const double original=frame_->input_cut()?double(frame_->source_width())/frame_->source_height():double(frame_->width())/frame_->height();
    double w=1,h=1;
    if(original>ratio)w=ratio/original;else h=original/ratio;
    setCropRect(QRectF((1-w)/2,(1-h)/2,w,h));
}
void PreviewController::cancelExport(){if(cancel_batch_)cancel_batch_->store(true);}

void PreviewController::decorateCatalog(desktop::Catalog& catalogue){
    catalogue.engine_resources_sha256=materials_->resourcesFingerprint().toStdString();
    for(auto* stocks:{&catalogue.films,&catalogue.papers})for(auto& stock:*stocks){const auto entry=materials_->entry(text(stock.id));if(!entry.isEmpty())stock.profile_sha256=entry.value("sha256").toString().toStdString();}
}
QVariantList PreviewController::materialEntries()const{return materials_?materials_->entries():QVariantList{};}
void PreviewController::refreshMaterials(){
    const auto gen=++generation_;submit([gen](auto& host){Reply r;r.generation=gen;r.operation="materials";try{host.reload_catalog();r.catalog=host.catalog();}catch(const std::exception& e){r.error=text(e.what());}return r;},QStringLiteral("正在验证并注册材料…"),"materials");
}
void PreviewController::importMaterial(const QUrl& file){
    if(!ready_||busy_||dialog_open_||!file.isLocalFile())return;
    try{saveActiveSettings();materials_->importFile(file.toLocalFile());refreshMaterials();}
    catch(const std::exception& e){failed_=true;status_=QStringLiteral("材料导入已拒绝：")+text(e.what());emit stateChanged();emit materialsChanged();}
}
void PreviewController::setMaterialEnabled(const QString& id,bool enabled){
    if(!ready_||busy_||dialog_open_)return;
    try{saveActiveSettings();materials_->setEnabled(id,enabled);refreshMaterials();}catch(const std::exception& e){failed_=true;status_=text(e.what());emit stateChanged();}
}
void PreviewController::removeMaterial(const QString& id){
    if(!ready_||busy_||dialog_open_)return;
    try{saveActiveSettings();materials_->remove(id);refreshMaterials();}catch(const std::exception& e){failed_=true;status_=text(e.what());emit stateChanged();}
}
void PreviewController::rescanMaterials(){
    if(!ready_||busy_||dialog_open_)return;
    try{saveActiveSettings();materials_->rescan();refreshMaterials();}catch(const std::exception& e){failed_=true;status_=text(e.what());emit stateChanged();}
}
void PreviewController::chooseMaterial(const QString& id){
    if(!settingsEditable())return;
    const auto detail=materials_->entry(id);if(detail.isEmpty()||!detail.value("available").toBool()||!detail.value("enabled").toBool()||detail.value("removed").toBool()){failed_=true;status_=QStringLiteral("该材料版本不可用于新选择；旧照片仍可读取未损坏的历史存档");emit stateChanged();return;}
    for(int i=0;i<int(catalog_.films.size());++i)if(text(catalog_.films[i].id)==id){setFilmIndex(i);return;}
    for(int i=0;i<int(catalog_.papers.size());++i)if(text(catalog_.papers[i].id)==id){if(filmIsPositive()||catalog_.papers[i].is_monochrome!=filmIsMonochrome()){failed_=true;status_=QStringLiteral("当前胶片不使用该印相材料；请先选择兼容的负片");emit stateChanged();return;}setPaperIndex(i);return;}
}

editing::State PreviewController::itemState(const LibraryItem& item) {
    return {item.settings,item.mode,item.geometry,item.develop};
}
editing::State PreviewController::currentState() const {
    return {settings(),decode_mode_?desktop::DecodeMode::headroom:desktop::DecodeMode::compatible16,geometry_,develop_};
}
bool PreviewController::canUndo() const {
    return settingsEditable()&&active_index_>=0&&!library_[active_index_].undo.empty();
}
bool PreviewController::canRedo() const {
    return settingsEditable()&&active_index_>=0&&!library_[active_index_].redo.empty();
}
void PreviewController::recordEdit(LibraryItem& item,const editing::State& before) {
    if(gesture_index_!=active_index_||!gesture_recorded_) {
        item.undo.push_back(before);
        if(item.undo.size()>64)item.undo.erase(item.undo.begin());
        item.redo.clear();
        if(gesture_index_==active_index_)gesture_recorded_=true;
    }
}
void PreviewController::beginEditGesture(const QString& key) {
    if(!settingsEditable()||active_index_<0||key.isEmpty())return;
    endEditGesture();gesture_index_=active_index_;gesture_key_=key;gesture_recorded_=false;gesture_redo_=library_[active_index_].redo;
    if(key=="straightenDegrees")auto_apply_.stop();
}
void PreviewController::endEditGesture() {
    const bool heldGeometry=gesture_key_=="straightenDegrees";
    if(gesture_index_>=0&&gesture_index_<int(library_.size())&&gesture_recorded_) {
        auto& item=library_[gesture_index_];
        // A drag returning to its starting value is not an undo step.
        if(!item.undo.empty()&&sameState(item.undo.back(),itemState(item))){item.undo.pop_back();item.redo=std::move(gesture_redo_);}
    }
    gesture_index_=-1;gesture_key_.clear();gesture_recorded_=false;gesture_redo_.clear();emit historyChanged();
    if(heldGeometry)schedulePreview();
}
void PreviewController::undo(){restoreHistory(false);}
void PreviewController::redo(){restoreHistory(true);}
void PreviewController::restoreHistory(bool redo) {
    if(!frame_||!settingsEditable()||active_index_<0)return;
    endEditGesture();
    auto& item=library_[active_index_];auto& from=redo?item.redo:item.undo;
    auto& to=redo?item.undo:item.redo;
    if(from.empty())return;
    const auto target=from.back();
    try {
        // Prepare geometry before moving the history cursor. Allocation or
        // validation failure leaves the frame and both stacks intact.
        const bool geometryChangedNow=target.geometry!=geometry_;
        const bool pictureChanged=geometryChangedNow||target.develop!=develop_;
        output::validateGeometry(full_image_.size(),target.geometry);output::validateDevelop(target.develop);
        to.push_back(currentState());if(to.size()>64)to.erase(to.begin());from.pop_back();
        geometry_=target.geometry;develop_=target.develop;
        loadSettings(target.settings,target.mode);saveActiveSettings();
        ++settings_revision_;auto_apply_.stop();schedulePreview();updateThumbnail();
        failed_=false;status_=redo?QStringLiteral("已重做当前照片调整"):QStringLiteral("已撤销当前照片调整");
        if(geometryChangedNow)emit geometryChanged();
        if(pictureChanged)requestView();
        emit settingsChanged();emit stateChanged();emit historyChanged();
    }catch(const std::exception& e){failed_=true;status_=text(e.what());emit stateChanged();}
}
void PreviewController::scheduleSave() {
    for(auto& item:library_)if(item.pending_save)item.save_failures=0;
    save_timer_.start(350);emit stateChanged();
}
void PreviewController::flushEdits() {
    save_timer_.stop();
    // Derived exposure/neutral-pack evidence is per-photo metadata, not an
    // editable gain. Never replay this record as a physical parameter. On
    // reopen the engine remeasures the retained recipe and displays fresh data.
    if(frame_&&!dirty()&&active_index_>=0&&!library_[active_index_].save_blocked) {
        const auto diagnosticPath=edit_store_.filePath(library_[active_index_].url)+".process.json";
        QDir().mkpath(QFileInfo(diagnosticPath).absolutePath());
        QSaveFile file(diagnosticPath);file.setDirectWriteFallback(false);
        const auto bytes=processDiagnostics().toUtf8();
        if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())
            library_[active_index_].save_warning=QStringLiteral("Cannot save Process measurement diagnostics");
    }
    int retryDelay=0;
    for(auto& item:library_)if(item.pending_save&&!item.save_blocked) {
        try {edit_store_.save(item.url,itemState(item));item.pending_save=false;item.has_stored=true;item.save_warning.clear();item.save_failures=0;}
        catch(const std::exception& e){
            item.save_warning=text(e.what());
            if(++item.save_failures<=3)retryDelay=std::max(retryDelay,500*(1<<item.save_failures));
        }
    }
    if(retryDelay)save_timer_.start(retryDelay);
    emit stateChanged();
}
void PreviewController::retrySave() {
    for(auto& item:library_)if(item.pending_save)item.save_failures=0;
    flushEdits();
}
bool PreviewController::prepareToClose() {
    endEditGesture();flushEdits();
    for(const auto& item:library_)if(item.pending_save)return false;
    return true;
}
QString PreviewController::persistenceWarning() const {
    QStringList warnings;
    for(const auto& item:library_)if(!item.save_warning.isEmpty())
        warnings.append(QFileInfo(item.url.toLocalFile()).fileName()+": "+item.save_warning);
    return warnings.join("\n");
}
QString PreviewController::savedStatus() const {
    if(!persistenceWarning().isEmpty())return QStringLiteral("部分设置未保存，请查看下方提示");
    for(const auto& item:library_)if(item.pending_save)return QStringLiteral("正在自动保存设置…");
    if(active_index_>=0&&library_[active_index_].has_stored)return QStringLiteral("当前照片设置已自动保存");
    return QStringLiteral("调整将自动保存，重新打开照片时恢复");
}
// Flips are stored in output axes. Rotating the displayed image also swaps
// those axes; without this a mirrored photo would turn the opposite way.
void PreviewController::rotateClockwise(){auto next=geometry_;next.quarterTurns=(next.quarterTurns+1)%4;std::swap(next.flipHorizontal,next.flipVertical);setGeometry(next,"quarterTurns");}
void PreviewController::rotateCounterClockwise(){auto next=geometry_;next.quarterTurns=(next.quarterTurns+3)%4;std::swap(next.flipHorizontal,next.flipVertical);setGeometry(next,"quarterTurns");}
void PreviewController::setFlipHorizontal(bool value){auto next=geometry_;next.flipHorizontal=value;setGeometry(next,"flipHorizontal");}
void PreviewController::setFlipVertical(bool value){auto next=geometry_;next.flipVertical=value;setGeometry(next,"flipVertical");}
void PreviewController::setStraightenDegrees(double value){auto next=geometry_;next.straightenDegrees=value;setGeometry(next,"straightenDegrees");}
void PreviewController::resetGeometry(){setGeometry(output::Geometry{});}

void PreviewController::exportBatch(const QUrl& folder,int format,int quality,int maxLongEdge) {
    if(!ready_||busy_)return;
    if(!folder.isLocalFile()||!selectedCount()){
        failed_=true;status_=!selectedCount()?QStringLiteral("请先选择要导出的照片"):QStringLiteral("请选择本地导出文件夹");emit stateChanged();return;
    }
    saveActiveSettings();
    std::vector<LibraryItem> items;
    for(const auto& item:library_)if(item.selected)items.push_back(item);
    // Strip thumbnails from the job snapshot. Only one developed frame is
    // retained by the worker, regardless of the number of imported photos.
    for(auto& item:items)item.thumbnail={};
    const auto held=frame_;const auto resources=resources_;const auto destination=path(folder);
    const auto gen=++generation_;cancel_batch_=std::make_shared<std::atomic_bool>(false);
    const auto cancel=cancel_batch_;batch_progress_=0;batch_total_=int(items.size());
    submit([this,items=std::move(items),held,resources,destination,gen,cancel,format,quality,maxLongEdge](auto& host){
        Reply r;r.generation=gen;r.operation="batch-export";
        int done=0,skipped=0,errors=0,processed=0;QStringList details;
        try {
            const auto options=exportOptions(format,quality,maxLongEdge);
            if(!std::filesystem::is_directory(destination))throw std::runtime_error("Export destination is not a folder");
            const QString extension=format==0?".tif":format==3?".jpg":".png";
            QSet<QString> names;
            for(const auto& item:items) {
                if(cancel->load())break;
                const auto source=path(item.url);
                const QString stem=QFileInfo(item.url.toLocalFile()).completeBaseName();
                QString name=stem+extension;int suffix=2;
                while(names.contains(name.toCaseFolded()))name=stem+"-"+QString::number(suffix++)+extension;
                names.insert(name.toCaseFolded());
                const auto target=destination/std::filesystem::path(name.toStdWString());
                try {
                    if(std::filesystem::exists(target))++skipped;
                    else {
                        const bool useShown=held&&held->source==source&&held->decode_mode==item.mode&&sameSettings(held->settings,item.settings)&&
                            (!held->input_cut()||held->input_geometry()==filmGeometry(item.geometry));
                        const auto developed=useShown?held:desktop::pair_project::open(host,source,item.mode,item.settings,filmGeometry(item.geometry));
                        auto itemOptions=options;itemOptions.develop=item.develop;
                        output::exportFrame(host.render_original(developed),target,item.geometry,itemOptions,resources);++done;
                    }
                }catch(const std::exception& e){++errors;details.append(name+": "+text(e.what()));}
                ++processed;
                QMetaObject::invokeMethod(this,[this,gen,processed]{if(generation_==gen){batch_progress_=processed;emit stateChanged();}},Qt::QueuedConnection);
            }
            r.message=QStringLiteral("%1：导出 %2 张，跳过已有 %3 张，失败 %4 张")
                .arg(cancel->load()?QStringLiteral("批量导出已取消"):QStringLiteral("批量导出完成")).arg(done).arg(skipped).arg(errors);
            if(!done&&skipped&&!errors)r.message+=QStringLiteral("。已有同名文件，请选择空文件夹后重试。");
            r.message+=QStringLiteral("\n文件夹：")+QString::fromStdWString(destination.wstring());
            if(errors)r.error=r.message+"\n"+details.join("\n");
        }catch(const std::exception& e){r.error=text(e.what());}
        return r;
    },QStringLiteral("正在批量导出…"),"batch-export");
}
