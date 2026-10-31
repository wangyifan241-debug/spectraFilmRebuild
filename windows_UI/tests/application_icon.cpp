// File/resource inspection only: prove the built PE contains the exact icon,
// not just a shortcut icon or a QML image. Does not launch another application.
#include <QGuiApplication>
#include <QFile>
#include <QIcon>
#include <QtEndian>
#include <iostream>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

int main(int argc,char** argv) {
    QGuiApplication app(argc,argv);
    try {
        if(argc!=3)throw std::runtime_error("Expected executable and reference ICO");
        QFile file(QString::fromLocal8Bit(argv[2]));
        if(!file.open(QIODevice::ReadOnly))throw std::runtime_error("Cannot open ICO");
        const auto ico=file.readAll();
        auto word=[&](int offset){if(offset+2>ico.size())throw std::runtime_error("Truncated ICO");return qFromLittleEndian<quint16>(ico.constData()+offset);};
        auto dword=[&](int offset){if(offset+4>ico.size())throw std::runtime_error("Truncated ICO");return qFromLittleEndian<quint32>(ico.constData()+offset);};
        if(word(0)!=0||word(2)!=1||word(4)!=9)throw std::runtime_error("Not the nine-resolution application ICO");
        const QIcon icon(QString::fromLocal8Bit(argv[2]));
        for(int size:{16,20,24,32,40,48,64,128,256}) {
            const auto image=icon.pixmap(size,size).toImage();
            if(image.size()!=QSize(size,size)||image.pixelColor(0,0).alpha()!=0||image.pixelColor(size/2,size/2).alpha()!=255)
                throw std::runtime_error("Icon size/transparency decode failed");
        }
#ifdef _WIN32
        const auto path=QString::fromLocal8Bit(argv[1]).toStdWString();
        const HMODULE module=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_AS_DATAFILE|LOAD_LIBRARY_AS_IMAGE_RESOURCE);
        if(!module)throw std::runtime_error("Cannot inspect PE resources");
        struct Guard {HMODULE value;~Guard(){FreeLibrary(value);}} guard{module};
        const auto group=FindResourceW(module,L"IDI_SPEKTRALAB",MAKEINTRESOURCEW(14));
        if(!group)throw std::runtime_error("Film icon group missing from executable");
        const auto groupSize=SizeofResource(module,group);
        const auto* directory=static_cast<const char*>(LockResource(LoadResource(module,group)));
        if(!directory||groupSize<6+14*9||qFromLittleEndian<quint16>(directory+4)!=9)
            throw std::runtime_error("PE icon group missing resolutions");
        for(int i=0;i<9;++i) {
            const auto* entry=directory+6+14*i;
            const auto resource=FindResourceW(module,MAKEINTRESOURCEW(qFromLittleEndian<quint16>(entry+12)),MAKEINTRESOURCEW(3));
            const auto offset=dword(6+16*i+12),length=dword(6+16*i+8);
            if(offset+quint64(length)>quint64(ico.size())||!resource||SizeofResource(module,resource)!=length)
                throw std::runtime_error("PE icon image size mismatch");
            const auto* bytes=static_cast<const char*>(LockResource(LoadResource(module,resource)));
            if(!bytes||QByteArrayView(bytes,length)!=QByteArrayView(ico.constData()+offset,length))
                throw std::runtime_error("PE icon image does not match reference artwork");
        }
#endif
        std::cout<<"PASS: nine transparent icon resolutions; PE icon bytes match original artwork\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
