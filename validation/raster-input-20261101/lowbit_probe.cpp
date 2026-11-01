// Independent extra probe, outside the shipped source inventory. Links the
// already-built raster reader and Qt PNG writer; no GPU or photo fixtures.
#include "image_io.hpp"
#include <QCoreApplication>
#include <QImageWriter>
#include <QTemporaryDir>
#include <QFile>
#include <cmath>
#include <iostream>
#include <iomanip>
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);QTemporaryDir owned;if(!owned.isValid())return 1;
    QImage pixels(3,1,QImage::Format_RGBA64);auto* row=reinterpret_cast<QRgba64*>(pixels.scanLine(0));
    for(int x=0;x<3;++x)row[x]=QRgba64::fromRgba64(32768+x,32768+x,32768+x,65535);
    const auto path=owned.filePath("adjacent-16bit-codes.png");QImageWriter writer(path,"PNG");if(!writer.write(pixels))return 2;
    QFile file(path);if(!file.open(QIODevice::ReadOnly)||(unsigned char)file.readAll()[24]!=16)return 3;
    spkhost::FloatImage decoded;spkhost::Probe probe;spkhost::DecodeOptions options;options.strict_icc=true;std::string error;
    if(!spkhost::decode_linear_prophoto(std::filesystem::path(path.toStdWString()),options,decoded,probe,error)){std::cerr<<error;return 4;}
    double maxError=0;
    for(int x=0;x<3;++x){
        const double encoded=(32768.+x)/65535.,expected=std::pow((encoded+.055)/1.055,2.4);
        for(int c=0;c<3;++c)maxError=std::max(maxError,std::abs(decoded.rgb[x*3+c]-expected));
    }
    const double delta=decoded.rgb[3]-decoded.rgb[0];
    std::cout<<std::setprecision(10)<<"PNG16 adjacent codes: linear delta="<<delta<<" max TRC/white-point error="<<maxError<<'\n';
    return maxError<2e-7&&delta>1e-5?0:5;
}
