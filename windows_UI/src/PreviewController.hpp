#pragma once
#include "desktop/preview_host.hpp"
#include "FrameOutput.hpp"
#include "EditStore.hpp"
#include "MaterialLibrary.hpp"
#include <QImage>
#include <QObject>
#include <QTimer>
#include <optional>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <atomic>

class PreviewController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool hasFrame READ hasFrame NOTIFY frameChanged)
    Q_PROPERTY(bool failed READ failed NOTIFY stateChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY settingsChanged)
    Q_PROPERTY(bool settingsEditable READ settingsEditable NOTIFY stateChanged)
    Q_PROPERTY(bool postEditable READ postEditable NOTIFY stateChanged)
    Q_PROPERTY(bool filmIsPositive READ filmIsPositive NOTIFY settingsChanged)
    Q_PROPERTY(bool filmIsMonochrome READ filmIsMonochrome NOTIFY settingsChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY frameChanged)
    Q_PROPERTY(QUrl sourceFolder READ sourceFolder NOTIFY frameChanged)
    Q_PROPERTY(QImage previewImage READ previewImage NOTIFY frameChanged)
    Q_PROPERTY(QImage originalImage READ originalImage NOTIFY frameChanged)
    Q_PROPERTY(QString processDiagnostics READ processDiagnostics NOTIFY frameChanged)
    Q_PROPERTY(QVariantMap metadata READ metadata NOTIFY frameChanged)
    Q_PROPERTY(QVariantMap histogram READ histogram NOTIFY frameChanged)
    Q_PROPERTY(QVariantMap postAdjustments READ postAdjustments NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList films READ films NOTIFY catalogChanged)
    Q_PROPERTY(QVariantList papers READ papers NOTIFY catalogChanged)
    Q_PROPERTY(QVariantList materialEntries READ materialEntries NOTIFY materialsChanged)
    Q_PROPERTY(int filmIndex READ filmIndex WRITE setFilmIndex NOTIFY settingsChanged)
    Q_PROPERTY(int paperIndex READ paperIndex WRITE setPaperIndex NOTIFY settingsChanged)
    Q_PROPERTY(int decodeMode READ decodeMode WRITE setDecodeMode NOTIFY settingsChanged)
    Q_PROPERTY(double exposureEv READ exposureEv WRITE setExposureEv NOTIFY settingsChanged)
    Q_PROPERTY(QVariantMap adjustments READ adjustments NOTIFY settingsChanged)
    Q_PROPERTY(QVariantMap filmFrame READ filmFrame NOTIFY settingsChanged)
    Q_PROPERTY(QVariantMap sceneReport READ sceneReport NOTIFY stateChanged)
    Q_PROPERTY(QVariantList libraryItems READ libraryItems NOTIFY libraryChanged)
    Q_PROPERTY(int activeIndex READ activeIndex NOTIFY libraryChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY libraryChanged)
    Q_PROPERTY(QImage uncroppedImage READ uncroppedImage NOTIFY frameChanged)
    Q_PROPERTY(QRectF cropRect READ cropRect WRITE setCropRect NOTIFY geometryChanged)
    Q_PROPERTY(bool batchExporting READ batchExporting NOTIFY stateChanged)
    Q_PROPERTY(int batchProgress READ batchProgress NOTIFY stateChanged)
    Q_PROPERTY(int batchTotal READ batchTotal NOTIFY stateChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    Q_PROPERTY(QString persistenceWarning READ persistenceWarning NOTIFY stateChanged)
    Q_PROPERTY(QString savedStatus READ savedStatus NOTIFY stateChanged)
    Q_PROPERTY(int quarterTurns READ quarterTurns NOTIFY geometryChanged)
    Q_PROPERTY(bool flipHorizontal READ flipHorizontal NOTIFY geometryChanged)
    Q_PROPERTY(bool flipVertical READ flipVertical NOTIFY geometryChanged)
    Q_PROPERTY(double straightenDegrees READ straightenDegrees NOTIFY geometryChanged)
public:
    // Runs on the presentation/engine worker. Must not access GUI objects.
    using ImageFactory=std::function<QImage(const spk::desktop::FramePtr&)>;
    using ViewFactory=std::function<QImage(const spk::desktop::FramePtr&,const QImage&,const spk::desktop::output::Geometry&,const spk::desktop::output::Develop&)>;
    explicit PreviewController(std::filesystem::path resources, QObject* parent=nullptr, ImageFactory imageFactory={}, QString editDirectory={},ViewFactory viewFactory={});
    static QImage imageFromFrame(const spk::desktop::FramePtr&);
    ~PreviewController() override;
    bool ready() const { return ready_; }
    bool busy() const { return busy_||view_busy_; }
    bool hasFrame() const { return bool(frame_); }
    bool failed() const { return failed_; }
    bool dirty() const;
    bool settingsEditable() const { return ready_&&!dialog_open_&&(!busy_||active_operation_=="render"); }
    bool postEditable() const { return settingsEditable()&&!busy_; }
    bool filmIsPositive() const;
    bool filmIsMonochrome() const;
    QString status() const { return status_; }
    QString fileName() const;
    QUrl sourceFolder() const;
    QImage previewImage() const { return image_; }
    QImage originalImage() const;
    QVariantMap metadata() const;
    QString processDiagnostics() const;
    QVariantMap histogram() const;
    QVariantMap postAdjustments() const;
    QVariantList films() const { return films_; }
    QVariantList papers() const { return papers_; }
    QVariantList materialEntries() const;
    Q_INVOKABLE void importMaterial(const QUrl& file);
    Q_INVOKABLE void setMaterialEnabled(const QString& id,bool enabled);
    Q_INVOKABLE void removeMaterial(const QString& id);
    Q_INVOKABLE void chooseMaterial(const QString& id);
    Q_INVOKABLE void rescanMaterials();
    int filmIndex() const { return film_index_; }
    int paperIndex() const { return paper_index_; }
    int decodeMode() const { return decode_mode_; }
    double exposureEv() const { return exposure_ev_; }
    QVariantMap adjustments() const;
    QVariantMap filmFrame() const;
    Q_INVOKABLE void setFilmFrame(const QString& name,const QVariant& value);
    QVariantMap sceneReport() const;
    QVariantList libraryItems() const;
    int activeIndex() const { return active_index_; }
    int selectedCount() const;
    QImage uncroppedImage() const;
    QRectF cropRect() const { return geometry_.crop; }
    int quarterTurns() const { return geometry_.quarterTurns; }
    bool flipHorizontal() const { return geometry_.flipHorizontal; }
    bool flipVertical() const { return geometry_.flipVertical; }
    double straightenDegrees() const { return geometry_.straightenDegrees; }
    bool canUndo() const;
    bool canRedo() const;
    QString persistenceWarning() const;
    QString savedStatus() const;
    bool batchExporting() const { return busy_ && active_operation_=="batch-export"; }
    int batchProgress() const { return batch_progress_; }
    int batchTotal() const { return batch_total_; }
    void setFilmIndex(int value);
    void setPaperIndex(int value);
    void setDecodeMode(int value);
    void setExposureEv(double value);
    spk::desktop::FramePtr frame() const { return frame_; }
    Q_INVOKABLE void openRaw(const QUrl& path);
    Q_INVOKABLE void openFiles(const QVariantList& urls);
    Q_INVOKABLE void selectImage(int index);
    Q_INVOKABLE void setItemSelected(int index, bool selected);
    Q_INVOKABLE void selectAllPhotos(bool selected);
    Q_INVOKABLE void createSelectedPair();
    Q_INVOKABLE void syncSelectedSettings(bool includeCrop=false);
    Q_INVOKABLE void setAdjustment(const QString& name, const QVariant& value);
    Q_INVOKABLE void setPostAdjustment(const QString& name, const QVariant& value);
    Q_INVOKABLE void setCurve(int channel,const QVariantList& points);
    Q_INVOKABLE void resetPostAdjustments();
    Q_INVOKABLE void setCropRect(const QRectF& crop);
    Q_INVOKABLE void resetCrop();
    Q_INVOKABLE void setCropAspect(double ratio);
    Q_INVOKABLE void rotateClockwise();
    Q_INVOKABLE void rotateCounterClockwise();
    Q_INVOKABLE void setFlipHorizontal(bool value);
    Q_INVOKABLE void setFlipVertical(bool value);
    Q_INVOKABLE void setStraightenDegrees(double value);
    Q_INVOKABLE void resetGeometry();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void beginEditGesture(const QString& key);
    Q_INVOKABLE void endEditGesture();
    Q_INVOKABLE void flushEdits();
    Q_INVOKABLE void retrySave();
    Q_INVOKABLE bool prepareToClose();
    Q_INVOKABLE void apply();
    Q_INVOKABLE void exportTiff(const QUrl& path);
    Q_INVOKABLE void exportImage(const QUrl& path, int format, int quality, int maxLongEdge);
    Q_INVOKABLE void exportBatch(const QUrl& folder, int format, int quality, int maxLongEdge);
    Q_INVOKABLE void cancelExport();
    Q_INVOKABLE void resetSettings();
    Q_INVOKABLE void measureScene();
    Q_INVOKABLE void process();
    Q_INVOKABLE void fitScene(double highlight,double shadow,bool automatic=false);
    Q_INVOKABLE void resetScenePlacement();
    Q_INVOKABLE void setDialogOpen(bool open);
signals:
    void stateChanged();
    void catalogChanged();
    void materialsChanged();
    void settingsChanged();
    void frameChanged();
    void sourceOpened();
    void libraryChanged();
    void geometryChanged();
    void historyChanged();
    void initialized(bool success);
    void operationFinished(const QString& operation, bool success);
private:
    friend struct PreviewControllerReviewTest;
    struct Reply {
        quint64 generation=0;
        quint64 settings_revision=0;
        QString operation, error;
        QString message;
        int library_index=-1;
        spk::desktop::output::Geometry geometry;
        spk::desktop::output::Develop develop;
        spk::desktop::FramePtr frame;
        QImage full_image,visible_image,original_image,uncropped_image;
        spk::desktop::FramePtr view_frame;
        quint64 view_revision=0;
        spk::desktop::Catalog catalog;
        QVariantMap scene;
        bool apply_scene=false;
    };
    using Job = std::function<Reply(spk::desktop::PreviewHost&)>;
    struct Task { Job job; quint64 generation=0,revision=0; QString operation; };
    void workerLoop(std::filesystem::path resources);
    void deliver(Reply reply);
    void prepareView(Reply& reply,const spk::desktop::FramePtr& frame,const QImage& full);
    void requestView();
    void acceptView(Reply reply);
    void accept(Reply reply);
    void submit(Job job, QString status, QString operation);
    spk::desktop::RenderSettings settings() const;
    void restoreSettings();
    void loadSettings(const spk::desktop::RenderSettings&, spk::desktop::DecodeMode);
    void saveActiveSettings(bool recordHistory=false);
    void setGeometry(const spk::desktop::output::Geometry&,const QString& editKey={});
    spk::desktop::editing::State currentState() const;
    void restoreHistory(bool redo);
    void scheduleSave();
    void updateThumbnail();
    int addFile(const QUrl&);
    void openItem(int index);
    void changed(const QString& editKey={});
    void schedulePreview();
    void requestScene(double highlight,double shadow,bool automatic,bool apply);
    void refreshMaterials();
    void decorateCatalog(spk::desktop::Catalog&);
    bool ready_=false, busy_=true, failed_=false;
    QString status_=QStringLiteral("正在启动渲染引擎…");
    QVariantList films_, papers_;
    spk::desktop::Catalog catalog_;
    int film_index_=0, paper_index_=0, decode_mode_=0;
    double exposure_ev_=0;
    spk::desktop::FramePtr frame_;
    QImage image_;
    QImage full_image_;
    QImage original_image_,uncropped_image_;
    // Only the render worker reads/writes this cache. Weak ownership avoids
    // retaining an obsolete full frame merely for a comparison image.
    std::weak_ptr<const spk::desktop::Frame> worker_original_reference_;
    QImage worker_original_image_,worker_original_working_,worker_original_neutral_;
    spk::desktop::output::Geometry worker_original_geometry_;
    std::weak_ptr<const spk::desktop::Frame> worker_uncropped_reference_;
    spk::desktop::output::Develop worker_uncropped_develop_;
    QImage worker_uncropped_image_;
    bool view_busy_=false,view_pending_=false;
    quint64 view_revision_=0;
    ViewFactory view_factory_;
    mutable qint64 histogram_key_=0;
    mutable QVariantMap histogram_cache_;
    mutable std::optional<spk::desktop::output::Develop> post_cache_key_;
    mutable QVariantMap post_cache_;
    spk::desktop::output::Geometry geometry_;
    spk::desktop::output::Develop develop_;
    spk::desktop::RenderSettings extra_settings_;
    QVariantMap scene_report_;
    spk::desktop::FramePtr scene_reference_;
    spk::desktop::RenderSettings measured_settings_;
    struct LibraryItem {
        QUrl url;
        spk::desktop::RenderSettings settings;
        spk::desktop::DecodeMode mode=spk::desktop::DecodeMode::compatible16;
        spk::desktop::output::Geometry geometry;
        spk::desktop::output::Develop develop;
        QImage thumbnail;
        QString error;
        bool selected=true;
        std::vector<spk::desktop::editing::State> undo,redo;
        bool pending_save=false,save_blocked=false,has_stored=false;
        unsigned save_failures=0;
        bool material_blocked=false;
        QString save_warning;
    };
    static spk::desktop::editing::State itemState(const LibraryItem&);
    void recordEdit(LibraryItem&,const spk::desktop::editing::State& before);
    std::vector<LibraryItem> library_;
    int active_index_=-1;
    std::filesystem::path resources_;
    std::unique_ptr<MaterialLibrary> materials_;
    spk::desktop::editing::Store edit_store_;
    QTimer save_timer_;
    int gesture_index_=-1;
    QString gesture_key_;
    bool gesture_recorded_=false;
    std::vector<spk::desktop::editing::State> gesture_redo_;
    int batch_progress_=0, batch_total_=0;
    std::shared_ptr<std::atomic_bool> cancel_batch_;
    ImageFactory image_factory_;
    quint64 generation_=0;
    quint64 settings_revision_=0;
    QString active_operation_;
    QTimer auto_apply_;
    bool dialog_open_=false;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Task> jobs_;
    bool stopping_=false;
    std::thread worker_;
};
