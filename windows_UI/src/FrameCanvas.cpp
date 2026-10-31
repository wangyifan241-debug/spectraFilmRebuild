#include "FrameCanvas.hpp"
#include <QMouseEvent>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QScreen>
#include <QFile>
#include <algorithm>
#include <cmath>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
class ImageNode final:public QSGNode {
public:
    ImageNode(){after=new QSGSimpleTextureNode;before=new QSGSimpleTextureNode;appendChildNode(after);appendChildNode(before);}
    QSGSimpleTextureNode *after,*before;
    quint64 revision=0,beforeRevision=0;
};
}
FrameCanvas::FrameCanvas(QQuickItem* parent):QQuickItem(parent) {
    setFlag(ItemHasContents,true);setClip(true);setAcceptedMouseButtons(Qt::LeftButton);
    connect(this,&QQuickItem::windowChanged,this,[this](QQuickWindow* w){
        if(w)connect(w,&QWindow::screenChanged,this,&FrameCanvas::refreshMonitorProfile,Qt::UniqueConnection);
        refreshMonitorProfile();
    });
}
void FrameCanvas::refreshMonitorProfile() {
    monitor_space_=QColorSpace::SRgb;colour_status_=QStringLiteral("sRGB 显示 · 未取得显示器 ICC");
#ifdef _WIN32
    if(window()&&window()->screen()) {
        const auto device=window()->screen()->name();
        HDC dc=CreateDCW(L"DISPLAY",reinterpret_cast<LPCWSTR>(device.utf16()),nullptr,nullptr);
        if(dc) {
            DWORD length=0;GetICMProfileW(dc,&length,nullptr);
            if(length>0&&length<=32768) {
                std::vector<wchar_t> name(length);
                if(GetICMProfileW(dc,&length,name.data())) {
                    QFile file(QString::fromWCharArray(name.data()));
                    if(file.open(QIODevice::ReadOnly)&&file.size()>=132&&file.size()<=4*1024*1024) {
                        const auto space=QColorSpace::fromIccProfile(file.readAll());
                        if(space.isValid()) {monitor_space_=space;colour_status_=QStringLiteral("sRGB → 显示器 ICC · ")+space.description();}
                        else colour_status_=QStringLiteral("显示器 ICC 无效 · 使用 sRGB，未校准");
                    }
                }
            }
            DeleteDC(dc);
        }
    }
#endif
    ++revision_;++before_revision_;update();emit viewChanged();
}
void FrameCanvas::setImage(const QImage& image) {
    if(image.cacheKey()==image_.cacheKey())return;
    const bool changedSize=image.size()!=image_.size();image_=image;++revision_;
    if(changedSize)pan_={};clampPan();update();emit imageChanged();emit viewChanged();
}
void FrameCanvas::setBeforeImage(const QImage& image){if(image.cacheKey()==before_image_.cacheKey())return;before_image_=image;++before_revision_;update();emit imageChanged();}
void FrameCanvas::setCompare(bool v){if(compare_==v)return;compare_=v;update();emit viewChanged();}
void FrameCanvas::setShowOriginal(bool v){if(show_original_==v)return;show_original_=v;update();emit viewChanged();}
void FrameCanvas::setSplitPosition(double v){if(!std::isfinite(v))return;v=std::clamp(v,0.0,1.0);if(split_position_==v)return;split_position_=v;update();emit viewChanged();}
void FrameCanvas::setFit(bool value) {if(value==fit_)return;fit_=value;pan_={};update();emit viewChanged();}
void FrameCanvas::resetView(bool value) {fit_=value;pan_={};update();emit viewChanged();}
double FrameCanvas::dpr() const {return window()?window()->effectiveDevicePixelRatio():1.0;}
double FrameCanvas::scale() const {
    if(image_.isNull()||width()<=0||height()<=0)return 1;
    return fit_?std::min(width()*dpr()/image_.width(),height()*dpr()/image_.height()):1.0;
}
double FrameCanvas::scalePercent() const {return image_.isNull()?0:scale()*100;}
QRectF FrameCanvas::imageRect() const {
    if(image_.isNull())return {};
    const QSizeF size(image_.width()*scale()/dpr(),image_.height()*scale()/dpr());
    auto position=[](double extent,double canvas,double pan){
        return extent<=canvas?(canvas-extent)*0.5:std::clamp((canvas-extent)*0.5+pan,canvas-extent,0.0);
    };
    double x=position(size.width(),width(),fit_?0:pan_.x());
    double y=position(size.height(),height(),fit_?0:pan_.y());
    // At 100%, align the image origin to the physical pixel grid as well as
    // preserving its physical size. Fractional centring must not shift grain.
    if(std::abs(scale()-1.0)<1e-12){x=std::round(x*dpr())/dpr();y=std::round(y*dpr())/dpr();}
    return {x,y,size.width(),size.height()};
}
void FrameCanvas::clampPan() {
    const auto rect=imageRect();
    pan_.setX(rect.width()>width()?rect.x()-(width()-rect.width())*0.5:0);
    pan_.setY(rect.height()>height()?rect.y()-(height()-rect.height())*0.5:0);
}
void FrameCanvas::panBy(double x,double y) {
    if(fit_||image_.isNull())return;
    pan_+=QPointF(x,y);clampPan();update();emit viewChanged();
}
QSGNode* FrameCanvas::updatePaintNode(QSGNode* old,UpdatePaintNodeData*) {
    auto* node=static_cast<ImageNode*>(old);
    if(image_.isNull()||!window()){delete node;return nullptr;}
    if(!node) {
        node=new ImageNode;
        // Build the complete subtree before returning it to the renderer.
        // Dynamically attaching a child to an already-live plain QSGNode
        // leaves Qt's software renderer with a stale transform cache. Even
        // the inactive comparison node must have a valid texture at attach.
        QImage placeholder(1,1,QImage::Format_ARGB32);placeholder.fill(Qt::transparent);
        auto* texture=window()->createTextureFromImage(placeholder);
        if(!texture){delete node;return nullptr;}
        node->before->setTexture(texture);node->before->setOwnsTexture(true);
    }
    if(node->revision!=revision_) {
        const auto display=image_.colorSpace().isValid()&&monitor_space_!=image_.colorSpace()?image_.convertedToColorSpace(monitor_space_):image_;
        auto* texture=window()->createTextureFromImage(display);
        if(!texture){delete node;return nullptr;}
        auto* retired=node->after->texture();node->after->setOwnsTexture(false);node->after->setTexture(texture);delete retired;
        node->after->setOwnsTexture(true);node->revision=revision_;
    }
    const auto rect=imageRect();
    node->after->setFiltering(scale()>=1.0?QSGTexture::Nearest:QSGTexture::Linear);
    node->after->setRect(rect);
    const bool before=(show_original_||compare_)&&!before_image_.isNull()&&before_image_.size()==image_.size();
    if(before&&node->beforeRevision!=before_revision_) {
        const auto display=before_image_.colorSpace().isValid()&&monitor_space_!=before_image_.colorSpace()?before_image_.convertedToColorSpace(monitor_space_):before_image_;
        auto* texture=window()->createTextureFromImage(display);
        if(!texture){delete node;return nullptr;}
        auto* retired=node->before->texture();node->before->setOwnsTexture(false);node->before->setTexture(texture);delete retired;
        node->before->setOwnsTexture(true);node->beforeRevision=before_revision_;
    }
    const double portion=before?(show_original_?1:split_position_):0;
    if(node->before)node->before->setRect(QRectF(rect.x(),rect.y(),rect.width()*portion,rect.height()));
    if(before) {
        node->before->setSourceRect(0,0,before_image_.width()*portion,before_image_.height());
        node->before->setFiltering(scale()>=1.0?QSGTexture::Nearest:QSGTexture::Linear);
    }
    return node;
}
void FrameCanvas::geometryChange(const QRectF& next,const QRectF& previous) {
    QQuickItem::geometryChange(next,previous);clampPan();update();emit viewChanged();
}
void FrameCanvas::itemChange(ItemChange change,const ItemChangeData& data) {
    QQuickItem::itemChange(change,data);
    if(change==ItemDevicePixelRatioHasChanged){clampPan();update();emit viewChanged();}
}
void FrameCanvas::mousePressEvent(QMouseEvent* e) {
    if(fit_||image_.isNull()){e->ignore();return;}dragging_=true;last_mouse_=e->position();e->accept();
}
void FrameCanvas::mouseMoveEvent(QMouseEvent* e) {if(dragging_){const auto delta=e->position()-last_mouse_;last_mouse_=e->position();panBy(delta.x(),delta.y());e->accept();}else e->ignore();}
void FrameCanvas::mouseReleaseEvent(QMouseEvent* e) {dragging_=false;e->accept();}
void FrameCanvas::mouseUngrabEvent(){dragging_=false;}
void FrameCanvas::mouseDoubleClickEvent(QMouseEvent* e){resetView(!fit_);e->accept();}
