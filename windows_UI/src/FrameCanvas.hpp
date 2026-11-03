#pragma once
#include <QImage>
#include <QColorSpace>
#include <QQuickItem>

class FrameCanvas : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(QImage image READ image WRITE setImage NOTIFY imageChanged)
    Q_PROPERTY(QImage beforeImage READ beforeImage WRITE setBeforeImage NOTIFY imageChanged)
    Q_PROPERTY(bool compare READ compare WRITE setCompare NOTIFY viewChanged)
    Q_PROPERTY(bool showOriginal READ showOriginal WRITE setShowOriginal NOTIFY viewChanged)
    Q_PROPERTY(double splitPosition READ splitPosition WRITE setSplitPosition NOTIFY viewChanged)
    Q_PROPERTY(QString displayCapabilityStatus READ displayCapabilityStatus NOTIFY viewChanged)
    Q_PROPERTY(QString colourStatus READ colourStatus NOTIFY viewChanged)
    Q_PROPERTY(bool fit READ fit WRITE setFit NOTIFY viewChanged)
    Q_PROPERTY(double scalePercent READ scalePercent NOTIFY viewChanged)
    Q_PROPERTY(QRectF imageRect READ imageRect NOTIFY viewChanged)
public:
    explicit FrameCanvas(QQuickItem* parent=nullptr);
    QImage image() const { return image_; }
    void setImage(const QImage& image);
    QImage beforeImage() const {return before_image_;}
    void setBeforeImage(const QImage&);
    bool compare() const {return compare_;}
    void setCompare(bool);
    bool showOriginal() const {return show_original_;}
    void setShowOriginal(bool);
    double splitPosition() const {return split_position_;}
    void setSplitPosition(double);
    QString displayCapabilityStatus() const {return display_capability_status_;}
    QString colourStatus() const {return colour_status_;}
    bool fit() const { return fit_; }
    void setFit(bool fit);
    double scalePercent() const;
    QRectF imageRect() const;
    Q_INVOKABLE void resetView(bool fit=true);
    Q_INVOKABLE void panBy(double x,double y);
signals:
    void imageChanged();
    void viewChanged();
protected:
    QSGNode* updatePaintNode(QSGNode* old,UpdatePaintNodeData*) override;
    void geometryChange(const QRectF& next,const QRectF& previous) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseUngrabEvent() override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void itemChange(ItemChange,const ItemChangeData&) override;
private:
    double dpr() const;
    double scale() const;
    void clampPan();
    void refreshMonitorProfile();
    QColorSpace monitor_space_{QColorSpace::SRgb};
    QString colour_status_;
    QString display_capability_status_;
    QImage image_;
    QImage before_image_;
    quint64 before_revision_=0;
    bool compare_=false,show_original_=false;
    double split_position_=.5;
    quint64 revision_=0;
    bool fit_=true,dragging_=false;
    QPointF pan_,last_mouse_;
};
