#include "FrameOutput.hpp"
#include "host/output.hpp"
#include <QCoreApplication>
#include <QColorSpace>
#include <QColorTransform>
#include <QFile>
#include <memory>
#include <cmath>
#include <iostream>
#include <stdexcept>

static double decode(double v,bool romm) {
    return romm?(v<16./512?v/16:std::pow(v,1.8)):
        (v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4));
}
static double encode(double v,bool romm) {
    return romm?(v<1./512?v*16:std::pow(v,1/1.8)):
        (v<=.0031308?v*12.92:1.055*std::pow(v,1/2.4)-.055);
}
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static void iccChecks(const std::filesystem::path& resources) {
    for(const auto& name:{"sRGB","Display P3","ProPhoto RGB"}) {
        std::vector<uint8_t> bytes;std::string error;
        require(spkhost::icc_for(name,resources,bytes,error),"host ICC unavailable");
        const QByteArray raw(reinterpret_cast<const char*>(bytes.data()),qsizetype(bytes.size()));
        const auto profile=QColorSpace::fromIccProfile(raw);
        require(profile.isValid(),"Qt rejected host ICC");
        const auto u32=[&](size_t at){require(at+4<=bytes.size(),"ICC offset outside profile");return (uint32_t(bytes[at])<<24)|(uint32_t(bytes[at+1])<<16)|(uint32_t(bytes[at+2])<<8)|bytes[at+3];};
        require(u32(0)==bytes.size()&&raw.mid(36,4)=="acsp"&&raw.mid(16,8)=="RGB XYZ ","ICC header contract mismatch");
        // PCS illuminant is always D50, even for D65 display primaries.
        require(std::abs(int32_t(u32(68))/65536.-.9642)<.00002&&std::abs(int32_t(u32(72))/65536.-1.)<.00002&&std::abs(int32_t(u32(76))/65536.-.8249)<.00002,"ICC PCS white differs from D50");
        const bool romm=std::string(name)=="ProPhoto RGB";
        const auto linear=profile.withTransferFunction(QColorSpace::TransferFunction::Linear);
        const auto transform=profile.transformationToColorSpace(linear);
        double maximum=0;
        for(int code=0;code<=65535;code+=257) {
            const auto p=QRgba64::fromRgba64(code,code,code,65535);
            const auto got=transform.map(p);
            maximum=std::max(maximum,std::abs(got.red()/65535.-decode(code/65535.,romm)));
        }
        std::cout<<name<<" ICC TRC max absolute error="<<maximum<<'\n';
        require(maximum<.0002,"ICC TRC and sample encoding disagree");
    }
    const QByteArray resourceBytes=QString::fromStdWString(resources.wstring()).toUtf8();
    std::unique_ptr<spk_engine,decltype(&spk_engine_destroy)> engine(spk_engine_create(resourceBytes.constData(),nullptr),spk_engine_destroy);
    require(bool(engine),"engine required for output matrix check");
    for(const auto& name:{"sRGB","Display P3","ProPhoto RGB"}) {
        spkhost::OutputTransform host;std::string error;
        require(host.fetch(engine.get(),"ProPhoto RGB",name,error),"cannot fetch output transform");
        std::vector<uint8_t> icc;require(spkhost::icc_for(name,resources,icc,error),"ICC missing");
        const auto qt=QColorSpace(QColorSpace::ProPhotoRgb).transformationToColorSpace(QColorSpace::fromIccProfile(QByteArray(reinterpret_cast<const char*>(icc.data()),icc.size())));
        double maximum=0;
        // Neutral and modest-chroma patches stay inside every destination;
        // out-of-gamut CAM16 compression intentionally differs from Qt clipping.
        for(const auto& p:{QRgba64::fromRgba64(10000,10000,10000,65535),QRgba64::fromRgba64(30000,30000,30000,65535),QRgba64::fromRgba64(37000,35000,33000,65535)}) {
            uint16_t input[]={p.red(),p.green(),p.blue(),65535};std::vector<float> linear;std::vector<uint16_t> encoded;
            host.to_linear(input,1,1,1,linear);host.encode_rgba16(linear,encoded);
            const auto expected=qt.map(p);const uint16_t q[]={expected.red(),expected.green(),expected.blue()};
            for(int c=0;c<3;++c)maximum=std::max(maximum,std::abs(encoded[c]/65535.-q[c]/65535.));
        }
        std::cout<<name<<" host/Qt in-gamut max absolute error="<<maximum<<'\n';
        require(maximum<.001,"host and ICC colour matrices disagree");
    }
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    try {
        if(argc!=2)throw std::runtime_error("resources argument required");
        bool good=true;
        for(bool romm:{false,true}) {
            QImage image(6,1,QImage::Format_RGBA64);
            image.setColorSpace(romm?QColorSpace::ProPhotoRgb:QColorSpace::SRgb);
            const double values[]={.01,.18,.46,.7,.82,.95};
            for(int x=0;x<6;++x)reinterpret_cast<QRgba64*>(image.scanLine(0))[x]=QRgba64::fromRgba64(quint16(std::lround(values[x]*65535)),30000,15000,65535);
            spk::desktop::output::Develop d;d.exposure=1;
            const auto actual=spk::desktop::output::applyDevelop(image,d);
            for(int x=0;x<6;++x) {
                const auto input=reinterpret_cast<const QRgba64*>(image.constBits())[x].red()/65535.;
                const auto expected=std::lround(std::clamp(encode(decode(input,romm)*2,romm),0.,1.)*65535);
                const auto got=reinterpret_cast<const QRgba64*>(actual.constBits())[x].red();
                std::cout<<(romm?"ROMM":"sRGB")<<","<<input<<",old22="<<std::lround(std::min(1.,input*std::pow(2.,1/2.2))*65535)<<",linear="<<expected<<",actual="<<got<<"\n";
                good &= std::abs(long(got)-expected)<=1;
            }
            // Zero EV is bit-exact; its transfer curve must not be round-tripped.
            d.exposure=0;
            if(spk::desktop::output::applyDevelop(image,d)!=image)throw std::runtime_error("Neutral image changed");
            d.exposure=1;d.legacyExposure=true;
            const auto old=spk::desktop::output::applyDevelop(image,d);
            for(int x=0;x<6;++x) {
                const auto input=reinterpret_cast<const QRgba64*>(image.constBits())[x].red()/65535.;
                const auto expected=std::lround(std::min(1.,std::pow(std::pow(input,2.2)*2,1/2.2))*65535);
                require(reinterpret_cast<const QRgba64*>(old.constBits())[x].red()==expected,"Legacy exposure changed");
            }
        }
        if(!good)throw std::runtime_error("Exposure is not linear-light EV in the declared space");
        iccChecks(std::filesystem::path(QString::fromLocal8Bit(argv[1]).toStdWString()));
        std::cout<<"PASS linear EV, neutral identity\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
