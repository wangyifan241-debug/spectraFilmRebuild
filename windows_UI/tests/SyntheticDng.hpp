#pragma once
// A small, owned Bayer DNG for boundary tests. This is not a substitute for
// real-camera decoder validation; it makes controller/store/export tests run
// without ever opening a user's photographs or silently skipping a fixture.
#include <QByteArray>
#include <QFile>
#include <QString>
#include <algorithm>
#include <initializer_list>
#include <stdexcept>
#include <vector>

inline void writeSyntheticDng(const QString& path, quint32 width=128, quint32 height=96) {
    if(width<16||height<16||width%2||height%2||quint64(width)*height>50000000)
        throw std::runtime_error("Invalid owned DNG dimensions");
    const auto le16=[](quint16 v){QByteArray b;b.append(char(v));b.append(char(v>>8));return b;};
    const auto le32=[](quint32 v){QByteArray b;for(unsigned i=0;i<4;++i)b.append(char(v>>(8*i)));return b;};
    struct Tag {quint16 id,type;quint32 count;QByteArray data;};
    std::vector<Tag> tags;
    const auto shorts=[&](quint16 id,std::initializer_list<quint16> values) {
        QByteArray data;for(auto v:values)data+=le16(v);tags.push_back({id,3,quint32(values.size()),data});
    };
    const auto longs=[&](quint16 id,std::initializer_list<quint32> values) {
        QByteArray data;for(auto v:values)data+=le32(v);tags.push_back({id,4,quint32(values.size()),data});
    };
    const auto bytes=[&](quint16 id,std::initializer_list<unsigned char> values) {
        QByteArray data;for(auto v:values)data.append(char(v));tags.push_back({id,1,quint32(values.size()),data});
    };
    const auto ascii=[&](quint16 id,const char* value) {
        QByteArray data(value);data.append('\0');tags.push_back({id,2,quint32(data.size()),data});
    };
    longs(256,{width});longs(257,{height});shorts(258,{16});shorts(259,{1});shorts(262,{32803});
    ascii(271,"SpektraLab");ascii(272,"Synthetic Bayer");longs(273,{0});shorts(274,{1});
    shorts(277,{1});longs(278,{height});longs(279,{width*height*2});shorts(284,{1});
    shorts(33421,{2,2});bytes(33422,{0,1,1,2});
    bytes(50706,{1,4,0,0});bytes(50707,{1,1,0,0});ascii(50708,"SpektraLab Synthetic Bayer");
    bytes(50710,{0,1,2});shorts(50711,{1});shorts(50714,{256});longs(50717,{4095});
    longs(50719,{0,0});longs(50720,{width,height});
    QByteArray matrix;
    for(qint32 v : {6400,3300,300,3000,6000,1000,1500,600,7900}){matrix+=le32(quint32(v));matrix+=le32(10000);}
    tags.push_back({50721,10,9,matrix});
    QByteArray neutral;for(unsigned i=0;i<3;++i){neutral+=le32(1);neutral+=le32(1);}
    tags.push_back({50728,5,3,neutral});shorts(50778,{21});longs(50829,{0,0,height,width});
    std::sort(tags.begin(),tags.end(),[](const auto& a,const auto& b){return a.id<b.id;});
    quint32 pixelOffset=8+2+quint32(tags.size())*12+4;
    for(const auto& tag:tags)if(tag.data.size()>4)pixelOffset+=quint32((tag.data.size()+1)/2*2);
    for(auto& tag:tags)if(tag.id==273)tag.data=le32(pixelOffset);
    QByteArray result("II");result+=le16(42);result+=le32(8);result+=le16(quint16(tags.size()));
    QByteArray extra;
    const quint32 extraStart=8+2+quint32(tags.size())*12+4;
    for(const auto& tag:tags) {
        result+=le16(tag.id);result+=le16(tag.type);result+=le32(tag.count);
        if(tag.data.size()<=4){auto payload=tag.data;payload.resize(4,'\0');result+=payload;}
        else {result+=le32(extraStart+quint32(extra.size()));extra+=tag.data;if(extra.size()%2)extra.append('\0');}
    }
    result+=le32(0);result+=extra;
    for(quint32 y=0;y<height;++y)for(quint32 x=0;x<width;++x) {
        const auto channel=(y%2==0&&x%2==0)?0:(y%2==1&&x%2==1)?2:1;
        const double gain=channel==0?1:channel==1?0.8:0.6;
        const auto value=quint16(256+(300+3200*double(x)/double(width-1))*gain);
        result+=le16(value);
    }
    QFile file(path);
    if(!file.open(QIODevice::WriteOnly|QIODevice::NewOnly)||file.write(result)!=result.size()||!file.flush())
        throw std::runtime_error("Cannot write owned DNG fixture");
}
