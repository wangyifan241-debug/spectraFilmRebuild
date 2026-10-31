// Deterministic, offline SVG -> multi-resolution Windows DIB icon generator.
// DIB entries (rather than PNG-only ICO) also work with NSIS' icon patcher.
#include <QCoreApplication>
#include <QDataStream>
#include <QImage>
#include <QPainter>
#include <QSaveFile>
#include <QSvgRenderer>
#include <cstdio>

int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    if(argc!=3)return 2;
    QSvgRenderer svg(QString::fromLocal8Bit(argv[1]));
    if(!svg.isValid())return 2;
    const QList<int> sizes{16,20,24,32,40,48,64,128,256};
    QList<QByteArray> images;
    for(int size:sizes) {
        QImage large(size*4,size*4,QImage::Format_ARGB32);large.fill(Qt::transparent);
        {QPainter painter(&large);svg.render(&painter);}
        const QImage image=large.scaled(size,size,Qt::IgnoreAspectRatio,Qt::SmoothTransformation)
            .convertToFormat(QImage::Format_ARGB32);
        QByteArray dib;QDataStream data(&dib,QIODevice::WriteOnly);data.setByteOrder(QDataStream::LittleEndian);
        const int maskRow=((size+31)/32)*4;
        data<<quint32(40)<<qint32(size)<<qint32(size*2)<<quint16(1)<<quint16(32)
            <<quint32(0)<<quint32(size*size*4+maskRow*size)<<qint32(0)<<qint32(0)<<quint32(0)<<quint32(0);
        for(int y=size-1;y>=0;--y)for(int x=0;x<size;++x) {
            const auto color=image.pixel(x,y);
            data<<quint8(qBlue(color))<<quint8(qGreen(color))<<quint8(qRed(color))<<quint8(qAlpha(color));
        }
        for(int y=size-1;y>=0;--y) {
            QByteArray mask(maskRow,0);
            for(int x=0;x<size;++x)if(qAlpha(image.pixel(x,y))==0)
                mask[x/8]=char(quint8(mask[x/8])|(0x80>>(x%8)));
            data.writeRawData(mask.constData(),mask.size());
        }
        images.append(dib);
    }
    QSaveFile output(QString::fromLocal8Bit(argv[2]));if(!output.open(QIODevice::WriteOnly))return 1;
    QDataStream stream(&output);stream.setByteOrder(QDataStream::LittleEndian);
    stream<<quint16(0)<<quint16(1)<<quint16(sizes.size());
    quint32 offset=6+16*sizes.size();
    for(int i=0;i<sizes.size();++i) {
        const int size=sizes[i];
        stream<<quint8(size==256?0:size)<<quint8(size==256?0:size)<<quint8(0)<<quint8(0)
            <<quint16(1)<<quint16(32)<<quint32(images[i].size())<<offset;
        offset+=images[i].size();
    }
    for(const auto& dib:images)stream.writeRawData(dib.constData(),dib.size());
    if(stream.status()!=QDataStream::Ok||!output.commit())return 1;
    std::puts("Generated nine-resolution film icon from original SVG");return 0;
}
