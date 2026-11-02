#include "FrameOutput.hpp"
#include "io/image_writer.hpp"
#include <QColorSpace>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageWriter>
#include <QTemporaryDir>
#include <QTransform>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace spk::desktop::output {
double evaluateCurve(const Curve& p,double x) {
    const auto n=p.size();
    if(n<2)return x;
    if(x<=p.front().x)return p.front().y;
    if(x>=p.back().x)return p.back().y;
    std::vector<double> d(n-1),m(n);
    for(size_t i=0;i+1<n;++i)d[i]=(p[i+1].y-p[i].y)/std::max(p[i+1].x-p[i].x,1e-6);
    m[0]=d[0];m[n-1]=d[n-2];
    for(size_t i=1;i+1<n;++i)m[i]=d[i-1]*d[i]<=0?0:(d[i-1]+d[i])*0.5;
    for(size_t i=0;i+1<n;++i)if(d[i]!=0) {
        const double a=m[i]/d[i],b=m[i+1]/d[i],s=a*a+b*b;
        if(s>9){const double t=3/std::sqrt(s);m[i]=t*a*d[i];m[i+1]=t*b*d[i];}
    }
    size_t i=0;while(i+2<n&&x>p[i+1].x)++i;
    const double h=std::max(p[i+1].x-p[i].x,1e-6),t=(x-p[i].x)/h,t2=t*t,t3=t2*t;
    return std::clamp((2*t3-3*t2+1)*p[i].y+(t3-2*t2+t)*h*m[i]+(-2*t3+3*t2)*p[i+1].y+(t3-t2)*h*m[i+1],0.0,1.0);
}
void validateDevelop(const Develop& d) {
    const auto range=[](double v,double lo,double hi){
        if(!std::isfinite(v)||v<lo||v>hi)throw std::runtime_error("Post-develop adjustment outside valid range");
    };
    range(d.temperature,-100,100);range(d.tint,-100,100);range(d.exposure,-3,3);
    range(d.contrast,-50,50);range(d.brightness,-50,50);range(d.saturation,-100,100);
    range(d.highlights,-100,100);range(d.shadows,-100,100);
    range(d.blackPoint,0,50);range(d.whitePoint,0,50);
    range(d.vignetteAmount,-100,100);range(d.vignetteMidpoint,0,100);
    for(const auto& z:d.zones){range(z.hue,0,360);range(z.saturation,0,1);range(z.luminance,-1,1);}
    for(const auto& curve:d.curves) {
        if(curve.size()<2||curve.size()>32||curve.front().x!=0||curve.back().x!=1)
            throw std::runtime_error("Curve requires 2–32 points and fixed domain endpoints");
        double last=-1;
        for(const auto& p:curve) {
            range(p.x,0,1);range(p.y,0,1);
            if(p.x<=last)throw std::runtime_error("Curve points must have distinct increasing x coordinates");
            last=p.x;
        }
    }
}
QImage applyDevelop(const QImage& source,const Develop& d) {
    validateDevelop(d);
    if(source.isNull()||source.format()!=QImage::Format_RGBA64)
        throw std::runtime_error("Post-develop requires RGBA16 pixels");
    if(d.neutral())return source;
    QImage result(source.size(),QImage::Format_RGBA64);
    if(result.isNull())throw std::runtime_error("Cannot allocate post-develop pixels");
    result.setColorSpace(source.colorSpace());
    const double mid=source.colorSpace()==QColorSpace(QColorSpace::ProPhotoRgb)?0.3857:0.4614;
    const double wb[]={1+0.18*d.temperature/100,1-0.12*d.tint/100,1-0.18*d.temperature/100};
    const double exposure=std::exp2(d.exposure),contrast=1+d.contrast/100*1.2;
    const unsigned mode=source.colorSpace()==QColorSpace(QColorSpace::ProPhotoRgb)?1:0;
    std::array<std::vector<double>,3> tone;
    for(unsigned c=0;c<3;++c) {
        tone[c].resize(65536);
        for(unsigned i=0;i<65536;++i) {
            double v=double(i)/65535*wb[c];
            if(d.exposure!=0) {
                if(d.legacyExposure)v=std::pow(std::pow(std::max(v,0.0),2.2)*exposure,1/2.2);
                else v=spkhost::cctf_encode_mode(float(spkhost::cctf_decode_mode(float(std::max(v,0.0)),mode)*exposure),mode);
            }
            if(d.contrast!=0)v=(v-mid)*contrast+mid;
            if(d.brightness!=0)v=std::pow(std::max(v,0.0),1/(1+d.brightness/100*0.8));
            tone[c][i]=v;
        }
    }
    const auto luma=[](const double* c){return c[0]*0.2126+c[1]*0.7152+c[2]*0.0722;};
    const double black=d.blackPoint/100,span=std::max(1-black-d.whitePoint/100,0.05);
    const bool curvesActive=d.curves!=Develop{}.curves;
    std::array<std::array<double,256>,5> tables{};
    if(curvesActive)for(unsigned row=0;row<5;++row)for(unsigned i=0;i<256;++i)tables[row][i]=evaluateCurve(d.curves[row],double(i)/255);
    const auto curveSample=[&](unsigned row,double v) {
        // Metal normalized linear texture sampling: centres are (i+.5)/256.
        const double t=std::clamp(v*256-0.5,0.0,255.0);const int a=int(t),b=std::min(a+1,255);
        return tables[row][a]+(tables[row][b]-tables[row][a])*(t-a);
    };
    double offsets[4][3]{};
    for(unsigned z=0;z<4;++z)for(unsigned k=0;k<3;++k)
        offsets[z][k]=std::cos(d.zones[z].hue*std::acos(-1.0)/180+(k==1?-2:k==2?2:0)*std::acos(-1.0)/3)*d.zones[z].saturation*.25;
    for(int y=0;y<source.height();++y) {
        const auto* in=reinterpret_cast<const QRgba64*>(source.constScanLine(y));
        auto* out=reinterpret_cast<QRgba64*>(result.scanLine(y));
        for(int x=0;x<source.width();++x) {
            if(in[x].alpha()!=65535)throw std::runtime_error("Post-develop requires opaque pixels");
            double c[]={tone[0][in[x].red()],tone[1][in[x].green()],tone[2][in[x].blue()]};
            const double l=luma(c),p=l<=mid?0.5*l/mid:0.5+0.5*(l-mid)/(1-mid);
            for(auto& v:c) {
                v+=d.shadows/100*0.25*(1-p)*(1-p)*(1-v);
                v+=d.highlights/100*0.25*p*p*(d.highlights>0?1-v:v);
                v=(v-black)/span;
            }
            const double grey=luma(c);
            const double ls=std::clamp(grey<=mid?.5*grey/mid:.5+.5*(grey-mid)/(1-mid),0.0,1.0);
            const double ws=(1-ls)*(1-ls),wh=ls*ls,wm=std::max(1-ws-wh,0.0),weights[]={1,ws,wm,wh};
            double lumGain=1;for(unsigned z=0;z<4;++z)lumGain+=d.zones[z].luminance*.25*weights[z];
            for(unsigned k=0;k<3;++k) {
                c[k]=grey+(c[k]-grey)*(1+d.saturation/100);
                for(unsigned z=0;z<4;++z)c[k]+=offsets[z][k]*weights[z];
                c[k]*=lumGain;
            }
            if(curvesActive) {
                for(auto& v:c)v=std::clamp(v,0.0,1.0);
                const double ly=luma(c),ly2=curveSample(1,ly),gain=ly>1e-4?ly2/ly:1;
                for(unsigned k=0;k<3;++k)c[k]=curveSample(k+2,curveSample(0,std::clamp(c[k]*gain,0.0,1.0)));
            }
            if(d.vignetteAmount!=0) {
                const double px=(double(x)+.5)/source.width()*2-1,py=(double(y)+.5)/source.height()*2-1;
                const double low=d.vignetteMidpoint/100*1.2,t=std::clamp((std::hypot(px,py)-low)/(1.5-low),0.0,1.0);
                const double gain=1+d.vignetteAmount/100*t*t*(3-2*t)*.9;
                for(auto& v:c)v*=gain;
            }
            const auto sample=[&](unsigned k){return quint16(std::lround(std::clamp(c[k],0.0,1.0)*65535));};
            out[x]=QRgba64::fromRgba64(sample(0),sample(1),sample(2),65535);
        }
    }
    return result;
}
Histogram displayHistogram(const QImage& visible) {
    Histogram result;
    if(visible.isNull())return result;
    result.stride=std::max(1,int(std::ceil(std::sqrt(double(visible.width())*visible.height()/131072.0))));
    // Only convert sampled pixels; an 8 MP RGBA64 frame must not allocate a
    // second full-size image merely to draw a 256-bin plot.
    for(int y=0;y<visible.height();y+=result.stride)
        for(int x=0;x<visible.width();x+=result.stride) {
            const auto c=visible.pixelColor(x,y).rgba64();
            const auto bin=[](quint16 v){return (quint32(v)+128u)/257u;};
            ++result.bins[0][bin(c.red())];
            ++result.bins[1][bin(c.green())];
            ++result.bins[2][bin(c.blue())];
            const auto luma=std::clamp(int(std::floor((0.2126*c.red()+0.7152*c.green()+0.0722*c.blue())/257.0+0.5)),0,255);
            ++result.bins[3][luma];
            ++result.samples;
        }
    return result;
}
namespace {
namespace fs = std::filesystem;
[[noreturn]] void fail(const QString& message) {
    throw std::runtime_error(message.toUtf8().toStdString());
}
QString pathText(const fs::path& path) {
#ifdef _WIN32
    return QString::fromStdWString(path.wstring());
#else
    return QString::fromStdString(path.string());
#endif
}
fs::path nativePath(const QString& path) {
#ifdef _WIN32
    return fs::path(path.toStdWString());
#else
    return fs::path(path.toStdString());
#endif
}
void validateOptions(const ExportOptions& options) {
    if(options.negativeScan && options.format!=Format::TIFF16)
        fail(QStringLiteral("Negative scan export requires 16-bit TIFF"));
    switch(options.format) {
    case Format::TIFF16: case Format::PNG8: case Format::PNG16: case Format::JPEG8: break;
    default: fail(QStringLiteral("Unsupported export format"));
    }
    if(options.jpegQuality < 1 || options.jpegQuality > 100)
        fail(QStringLiteral("JPEG quality must be between 1 and 100"));
    if(options.maxLongEdge < 0)
        fail(QStringLiteral("Export long edge cannot be negative"));
}
void validateExtension(const QFileInfo& destination,Format format) {
    const QString suffix=destination.suffix().toLower();
    const bool matches=format==Format::TIFF16?(suffix=="tif" || suffix=="tiff"):
        format==Format::JPEG8?(suffix=="jpg" || suffix=="jpeg"):suffix=="png";
    if(!matches) {
        const QString expected=format==Format::TIFF16?QStringLiteral(".tif or .tiff"):
            format==Format::JPEG8?QStringLiteral(".jpg or .jpeg"):QStringLiteral(".png");
        fail(QStringLiteral("Export file extension must match the selected format: ")+expected);
    }
}
QColorSpace profile(const fs::path& resources) {
    QFile file(pathText(resources / "io" / "sRGB.icc"));
    if(!file.open(QIODevice::ReadOnly) || file.size() < 132 || file.size() > 4*1024*1024)
        fail(QStringLiteral("Cannot read the bundled sRGB ICC profile"));
    const auto bytes=file.readAll();
    if(bytes.size()!=file.size())fail(QStringLiteral("Cannot read the complete sRGB ICC profile"));
    const auto space=QColorSpace::fromIccProfile(bytes);
    if(!space.isValid())fail(QStringLiteral("The bundled sRGB ICC profile is invalid"));
    return space;
}
void publish(const fs::path& partial,const fs::path& destination) {
#ifdef _WIN32
    // No MOVEFILE_REPLACE_EXISTING: the filesystem, not a racy exists check,
    // is the final arbiter. Same-directory staging keeps this on one volume.
    if(!MoveFileExW(partial.c_str(),destination.c_str(),MOVEFILE_WRITE_THROUGH))
        fail(QStringLiteral("Cannot publish export without replacing an existing file (Windows error %1)")
             .arg(GetLastError()));
#else
    if(!QFile::rename(pathText(partial),pathText(destination)))
        fail(QStringLiteral("Cannot publish export without replacing an existing file"));
#endif
}

QImage framePixels(const FramePtr& frame) {
    if(!frame)fail(QStringLiteral("No rendered photograph to export"));
    const auto& result=frame->pixels();
    if(!result.rgba16 || result.width==0 || result.height==0 || result.row_stride_px<result.width ||
       result.width>std::uint32_t(std::numeric_limits<int>::max()) ||
       result.height>std::uint32_t(std::numeric_limits<int>::max()))
        fail(QStringLiteral("The rendered photograph has invalid storage"));
    // The caller retains its immutable FramePtr for every use of this view.
    QImage image(reinterpret_cast<const uchar*>(result.rgba16),int(result.width),int(result.height),
                  qsizetype(result.row_stride_px)*8,QImage::Format_RGBA64);
    image.setColorSpace(frame->prophoto()?QColorSpace::ProPhotoRgb:QColorSpace::SRgb);
    if(frame->auto_negative_active) {
        // Engine DI pixels are Cineon printing-density log, never display RGB.
        // Decode before grading, then apply a luminance shoulder in the output
        // stage. This preserves the measured film and keeps neutral greys neutral.
        image=image.copy();
        const double black=std::pow(10.0,(95.0-685.0)/300.0);
        for(int y=0;y<image.height();++y) {
            auto* row=reinterpret_cast<QRgba64*>(image.scanLine(y));
            for(int x=0;x<image.width();++x) {
                const auto px=row[x];double rgb[3];
                const quint16 codes[3]={px.red(),px.green(),px.blue()};
                for(int c=0;c<3;++c)rgb[c]=std::max(0.0,(std::pow(10.0,(codes[c]/65535.0*1023.0-685.0)/300.0)-black)/(1.0-black));
                const double lum=.28804*rgb[0]+.71187*rgb[1]+.00009*rgb[2];
                const double above=std::max(0.0,lum-.184);
                const double mapped=lum<=.184?lum:.184+.816*above/(.816+above);
                const double scale=lum>0?mapped/lum:0;
                quint16 encoded[3];
                for(int c=0;c<3;++c) {
                    const double v=std::clamp(rgb[c]*scale,0.0,1.0);
                    encoded[c]=quint16(std::lround((v<1.0/512?v*16:std::pow(v,1.0/1.8))*65535));
                }
                row[x]=QRgba64::fromRgba64(encoded[0],encoded[1],encoded[2],65535);
            }
        }
    }
    return image;
}

QImage displaySpace(const QImage& working,const FramePtr& frame) {
    if(!frame->prophoto()||working.colorSpace()==QColorSpace(QColorSpace::SRgb))return working;
    QImage image(working.size(),QImage::Format_RGBA64);
    if(image.isNull())fail(QStringLiteral("Cannot allocate colour-managed output"));
    // Bound temporary float planes even for a 45 MP original export.
    std::vector<float> linear;std::vector<uint16_t> encoded;
    for(int y=0;y<working.height();y+=128) {
        const int rows=std::min(128,working.height()-y);
        frame->display_transform()->to_linear(reinterpret_cast<const uint16_t*>(working.constScanLine(y)),
            working.width(),rows,working.bytesPerLine()/8,linear);
        frame->display_transform()->encode_rgba16(linear,encoded);
        std::memcpy(image.scanLine(y),encoded.data(),size_t(rows)*working.width()*8);
    }
    image.setColorSpace(QColorSpace::SRgb);
    return image;
}
QImage developedWorking(const FramePtr& frame,const Develop& develop) {
    const auto working=framePixels(frame);
    if(develop.legacySrgb)return applyDevelop(displaySpace(working,frame),develop);
    return applyDevelop(working,develop);
}

QImage straighten(const QImage& source,double degrees) {
    if(degrees==0)return source;
    const auto pixels=source.convertToFormat(QImage::Format_RGBA64);
    QImage output(pixels.size(),QImage::Format_RGBA64);
    if(output.isNull() || pixels.isNull())fail(QStringLiteral("Cannot allocate the straightened photograph"));
    const int width=pixels.width(),height=pixels.height();
    const double angle=degrees*std::acos(-1.0)/180.0;
    const double cosine=std::cos(angle),sine=std::sin(angle);
    // Inverse-map the fixed output rectangle into the cropped source. The
    // larger projection determines the zoom that keeps all four image edges
    // covered. Edge-clamping is the usual half-pixel image sampling boundary.
    const double zoom=std::max(std::abs(cosine)+std::abs(sine)*height/width,
                               std::abs(cosine)+std::abs(sine)*width/height);
    const double c=cosine/zoom,s=sine/zoom;
    const double centerX=(width-1)*.5,centerY=(height-1)*.5;
    for(int y=0;y<height;++y) {
        auto* row=reinterpret_cast<QRgba64*>(output.scanLine(y));
        for(int x=0;x<width;++x) {
            const double sourceX=std::clamp(centerX+c*(x-centerX)+s*(y-centerY),0.0,double(width-1));
            const double sourceY=std::clamp(centerY-s*(x-centerX)+c*(y-centerY),0.0,double(height-1));
            const int x0=int(sourceX),y0=int(sourceY),x1=std::min(x0+1,width-1),y1=std::min(y0+1,height-1);
            const double fx=sourceX-x0,fy=sourceY-y0;
            const auto* top=reinterpret_cast<const QRgba64*>(pixels.constScanLine(y0));
            const auto* bottom=reinterpret_cast<const QRgba64*>(pixels.constScanLine(y1));
            auto channel=[&](quint16 a,quint16 b,quint16 d,quint16 e) {
                return quint16(std::clamp(std::lround((a+(b-a)*fx)*(1-fy)+(d+(e-d)*fx)*fy),0L,65535L));
            };
            row[x]=QRgba64::fromRgba64(channel(top[x0].red(),top[x1].red(),bottom[x0].red(),bottom[x1].red()),
                channel(top[x0].green(),top[x1].green(),bottom[x0].green(),bottom[x1].green()),
                channel(top[x0].blue(),top[x1].blue(),bottom[x0].blue(),bottom[x1].blue()),
                channel(top[x0].alpha(),top[x1].alpha(),bottom[x0].alpha(),bottom[x1].alpha()));
        }
    }
    output.setColorSpace(source.colorSpace());
    return output;
}

QImage applyGeometry(const QImage& source,const Geometry& geometry) {
    validateGeometry(source.size(),geometry);
    const auto rect=cropPixels(source.size(),geometry.crop);
    QImage output=rect==source.rect()?source:source.copy(rect);
    if(output.isNull())fail(QStringLiteral("Cannot allocate the cropped photograph"));
    output=straighten(output,geometry.straightenDegrees);
    if(geometry.quarterTurns)
        output=output.transformed(QTransform().rotate(90*geometry.quarterTurns),Qt::FastTransformation);
    if(geometry.flipHorizontal || geometry.flipVertical) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
        Qt::Orientations axes;
        if(geometry.flipHorizontal)axes|=Qt::Horizontal;
        if(geometry.flipVertical)axes|=Qt::Vertical;
        output=output.flipped(axes);
#else
        output=output.mirrored(geometry.flipHorizontal,geometry.flipVertical);
#endif
    }
    if(output.isNull())fail(QStringLiteral("Cannot allocate the transformed photograph"));
    return output;
}
}

QRect cropPixels(QSize fullSize,QRectF crop) {
    if(fullSize.width() <= 0 || fullSize.height() <= 0 ||
       !std::isfinite(crop.x()) || !std::isfinite(crop.y()) ||
       !std::isfinite(crop.width()) || !std::isfinite(crop.height()) ||
       crop.width() <= 0 || crop.height() <= 0 || crop.x() < 0 || crop.y() < 0 ||
       crop.right() > 1 || crop.bottom() > 1)
        fail(QStringLiteral("Crop must be a finite, positive rectangle inside the photograph"));
    const int left=std::clamp(int(std::lround(crop.left()*fullSize.width())),0,fullSize.width()-1);
    const int top=std::clamp(int(std::lround(crop.top()*fullSize.height())),0,fullSize.height()-1);
    const int right=std::clamp(int(std::lround(crop.right()*fullSize.width())),left+1,fullSize.width());
    const int bottom=std::clamp(int(std::lround(crop.bottom()*fullSize.height())),top+1,fullSize.height());
    return {left,top,right-left,bottom-top};
}

QImage displayImage(const QImage& full8,QRectF crop) {
    return displayImage(full8,Geometry{crop});
}

void validateGeometry(QSize size,const Geometry& geometry) {
    cropPixels(size,geometry.crop);
    if(geometry.quarterTurns<0 || geometry.quarterTurns>3)
        fail(QStringLiteral("Quarter turns must be between 0 and 3"));
    if(!std::isfinite(geometry.straightenDegrees) || std::abs(geometry.straightenDegrees)>45)
        fail(QStringLiteral("Straightening must be between -45 and +45 degrees"));
}

QImage displayImage(const QImage& full8,const Geometry& geometry) {
    auto output=applyGeometry(full8,geometry);
    if(output.format()!=full8.format())output=output.convertToFormat(full8.format());
    if(output.isNull())fail(QStringLiteral("Cannot allocate the transformed preview"));
    return output;
}

QImage exportImage(const QImage& fullRgba64,QRectF crop,const ExportOptions& options) {
    return exportImage(fullRgba64,Geometry{crop},options);
}

QImage exportImage(const QImage& fullRgba64,const Geometry& geometry,const ExportOptions& options) {
    validateOptions(options);
    if(fullRgba64.isNull() || fullRgba64.format()!=QImage::Format_RGBA64)
        fail(QStringLiteral("Export requires an RGBA16 frame"));
    validateGeometry(fullRgba64.size(),geometry);
    const auto rect=cropPixels(fullRgba64.size(),geometry.crop);
    // Validate only the exported region: padding is not part of the image.
    for(int y=rect.top();y<=rect.bottom();++y) {
        const auto* row=reinterpret_cast<const QRgba64*>(fullRgba64.constScanLine(y));
        for(int x=rect.left();x<=rect.right();++x)
            if(row[x].alpha()!=65535)fail(QStringLiteral("Export requires opaque pixels"));
    }
    QImage output=applyGeometry(applyDevelop(fullRgba64,options.develop),geometry);
    if(options.maxLongEdge && std::max(output.width(),output.height())>options.maxLongEdge) {
        const double ratio=double(options.maxLongEdge)/std::max(output.width(),output.height());
        const QSize size(std::max(1,int(std::lround(output.width()*ratio))),
                         std::max(1,int(std::lround(output.height()*ratio))));
        output=output.scaled(size,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
        if(output.isNull())fail(QStringLiteral("Cannot allocate the resized export"));
    }
    if(options.format==Format::PNG8 || options.format==Format::JPEG8)
        output=output.convertToFormat(QImage::Format_RGB888);
    if(output.isNull())fail(QStringLiteral("Cannot allocate the export pixels"));
    return output;
}

QImage displayFrame(const FramePtr& frame,const Geometry& geometry,const Develop& develop) {
    if(!frame)fail(QStringLiteral("Display requires a frame"));
    // Mac order: grade -> geometry/resample in working space -> destination.
    const auto pixels=displaySpace(exportImage(developedWorking(frame,develop),frame->input_cut()?Geometry{}:geometry,{}),frame);
    auto output=pixels.convertToFormat(QImage::Format_ARGB32);
    if(output.isNull())fail(QStringLiteral("Cannot allocate the transformed preview"));
    output.setColorSpace(QColorSpace::SRgb);
    return output;
}

QImage originalWorkingFrame(const FramePtr& frame,bool sourceOnly) {
    if(!frame||!frame->prophoto())fail(QStringLiteral("Original comparison requires a working-space frame"));
    const auto& raw=frame->input_cut()&&!sourceOnly?frame->prepared():frame->original();
    const double previewScale=std::min(1.,3400./std::max(raw.width,raw.height));
    const QSize size=sourceOnly?QSize(std::max(1,int(std::lround(raw.width*previewScale))),std::max(1,int(std::lround(raw.height*previewScale)))):
        QSize(int(frame->width()),int(frame->height()));
    QImage image=frame->film_gate_valid&&!sourceOnly?framePixels(frame).copy():QImage(size,QImage::Format_RGBA64);
    if(image.isNull())fail(QStringLiteral("Cannot allocate original comparison"));
    for(int y=0;y<image.height();++y) {
        auto* row=reinterpret_cast<QRgba64*>(image.scanLine(y));
        for(int x=0;x<image.width();++x) {
            double u=(x+.5)/image.width(),v=(y+.5)/image.height();
            if(frame->film_gate_valid&&!sourceOnly) {
                bool inside=false;
                for(int side=0;side<(frame->settings.pair.active?2:1);++side) {
                    const auto& q=side?frame->film_gate_b:frame->film_gate;
                    const double ax=q[2]-q[0],ay=q[3]-q[1],bx=q[6]-q[0],by=q[7]-q[1],det=ax*by-ay*bx;
                    const double dx=u-q[0],dy=v-q[1],s=(dx*by-dy*bx)/det,t=(ax*dy-ay*dx)/det;
                    if(s<0||s>1||t<0||t>1)continue;
                    const double along=frame->settings.pair.active?(s*18+side*19)/37:s;
                    u=frame->film_vertical?1-t:along;v=frame->film_vertical?along:t;
                    inside=true;break;
                }
                if(!inside)continue; // keep the same film canvas around the original
            }
            const double sx=std::clamp(u*raw.width-.5,0.0,double(raw.width-1));
            const double sy=std::clamp(v*raw.height-.5,0.0,double(raw.height-1));
            const unsigned y0=unsigned(sy),y1=std::min(y0+1,raw.height-1);const double fy=sy-y0;
            const unsigned x0=unsigned(sx),x1=std::min(x0+1,raw.width-1);const double fx=sx-x0;
            quint16 c[3];
            for(unsigned k=0;k<3;++k) {
                const auto at=[&](unsigned xx,unsigned yy){return raw.rgb[(size_t(yy)*raw.width+xx)*3+k];};
                const float v=float((at(x0,y0)*(1-fx)+at(x1,y0)*fx)*(1-fy)+(at(x0,y1)*(1-fx)+at(x1,y1)*fx)*fy);
                c[k]=quint16(std::lround(std::clamp(spkhost::cctf_encode_mode(v,1),0.0f,1.0f)*65535));
            }
            row[x]=QRgba64::fromRgba64(c[0],c[1],c[2],65535);
        }
    }
    image.setColorSpace(QColorSpace::ProPhotoRgb);
    return image;
}
QImage displayWorkingImage(const QImage& working,const FramePtr& frame,const Geometry& geometry) {
    return displaySpace(exportImage(working,geometry,{}),frame).convertToFormat(QImage::Format_ARGB32);
}
QImage originalFrame(const FramePtr& frame,const Geometry& geometry,bool sourceOnly) {
    return displayWorkingImage(originalWorkingFrame(frame,sourceOnly),frame,frame->input_cut()&&!sourceOnly?Geometry{}:geometry);
}

bool formatAvailable(Format format) {
    const auto formats=QImageWriter::supportedImageFormats();
    switch(format) {
    case Format::TIFF16: return true;
    case Format::PNG8: case Format::PNG16: return formats.contains("png");
    case Format::JPEG8: return formats.contains("jpeg") || formats.contains("jpg");
    }
    return false;
}

void exportImageFile(const QImage& fullRgba64,const fs::path& destination,
                     QRectF crop,const ExportOptions& options,const fs::path& resources) {
    exportImageFile(fullRgba64,destination,Geometry{crop},options,resources);
}

void exportImageFile(const QImage& fullRgba64,const fs::path& destination,
                     const Geometry& geometry,const ExportOptions& options,const fs::path& resources) {
    validateOptions(options);
    const auto outputSpace=options.negativeScan?QColorSpace(QColorSpace::ProPhotoRgb):QColorSpace(QColorSpace::SRgb);
    if(options.negativeScan && fullRgba64.colorSpace()!=outputSpace)
        fail(QStringLiteral("Negative scan pixels must be ProPhoto RGB"));
    if(fullRgba64.colorSpace().isValid()&&fullRgba64.colorSpace()!=outputSpace)
        fail(QStringLiteral("File pixels must be converted to the selected output space, not merely tagged"));
    if(!formatAvailable(options.format))fail(QStringLiteral("This export format is not installed"));
    if(destination.empty() || destination.filename().empty())fail(QStringLiteral("Choose an export file name"));
    const QFileInfo target(pathText(destination));
    validateExtension(target,options.format);
    if(target.exists() || target.isSymLink())fail(QStringLiteral("The export file already exists"));
    if(!target.absoluteDir().exists())fail(QStringLiteral("The export folder does not exist"));
    auto pixels=exportImage(fullRgba64,geometry,options);
    pixels.setColorSpace(options.negativeScan?outputSpace:profile(resources));
    QTemporaryDir staging(target.absoluteDir().filePath(QStringLiteral(".spektralab-export-XXXXXX")));
    if(!staging.isValid())fail(QStringLiteral("Cannot create an export temporary file in this folder"));
    const auto partial=nativePath(staging.filePath(QStringLiteral("output.partial")));
    auto iccPath=resources/"io"/"sRGB.icc";
    if(options.negativeScan) {
        const auto name=staging.filePath(QStringLiteral("prophoto.icc"));
        QFile icc(name);const auto bytes=outputSpace.iccProfile();
        if(bytes.isEmpty()||!icc.open(QIODevice::WriteOnly)||icc.write(bytes)!=bytes.size())
            fail(QStringLiteral("Cannot write the negative scan ICC profile"));
        icc.close();iccPath=nativePath(name);
    }
    if(options.format==Format::TIFF16) {
        std::string error;
        if(!io::write_srgb_tiff(partial,reinterpret_cast<const std::uint16_t*>(pixels.constBits()),
                std::uint32_t(pixels.width()),std::uint32_t(pixels.height()),
                std::uint32_t(pixels.bytesPerLine()/8),iccPath,error))
            throw std::runtime_error(error);
    } else {
        const auto format=options.format==Format::JPEG8?QByteArray("jpeg"):QByteArray("png");
        // Closing the QFile before publication ensures no partially written
        // image can become the destination, including on encoder failure.
        QFile file(pathText(partial));
        if(!file.open(QIODevice::WriteOnly|QIODevice::NewOnly))fail(file.errorString());
        QImageWriter writer(&file,format);
        if(options.format==Format::JPEG8)writer.setQuality(options.jpegQuality);
        if(!writer.write(pixels))fail(QStringLiteral("Cannot encode export: ")+writer.errorString());
        if(!file.flush())fail(QStringLiteral("Cannot flush export: ")+file.errorString());
        file.close();
    }
    publish(partial,nativePath(target.absoluteFilePath()));
}

void exportFrame(const FramePtr& frame,const fs::path& destination,QRectF crop,
                 const ExportOptions& options,const fs::path& resources) {
    exportFrame(frame,destination,Geometry{crop},options,resources);
}

void exportFrame(const FramePtr& frame,const fs::path& destination,const Geometry& geometry,
                 const ExportOptions& options,const fs::path& resources) {
    if(!frame)fail(QStringLiteral("Export requires a frame"));
    if(options.negativeScan && (!frame->scan_film || !frame->prophoto()))
        fail(QStringLiteral("Negative export requires a directly scanned ProPhoto frame"));
    // The caller's immutable FramePtr stays alive throughout this synchronous
    // function; crop/downsize/detach cannot modify its retained RGBA16 result.
    auto workingOptions=options;workingOptions.develop={};workingOptions.format=Format::TIFF16;
    QImage pixels;
    {
        const auto working=exportImage(developedWorking(frame,options.negativeScan?Develop{}:options.develop),frame->input_cut()?Geometry{}:geometry,workingOptions);
        pixels=options.negativeScan?working:displaySpace(working,frame);
    } // release the full-size working intermediate before the encoder allocates
    auto resolved=options;resolved.develop={};resolved.maxLongEdge=0;
    exportImageFile(pixels,destination,Geometry{},resolved,resources);
}
} // namespace spk::desktop::output
