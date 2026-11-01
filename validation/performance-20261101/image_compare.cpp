// Numerical comparison only. User images and rendered outputs remain ignored.
#include <QCoreApplication>
#include <QImage>
#include <iostream>
#include <cmath>
int main(int argc,char** argv){QCoreApplication app(argc,argv);if(argc!=4)return 1;
    QImage before(argv[1]),after(argv[2]),reference(argv[3]);
    if(before.isNull()||after.isNull()||reference.isNull())return 2;
    const auto compare=[&](QImage image){image=image.scaled(reference.size(),Qt::IgnoreAspectRatio,Qt::SmoothTransformation).convertToFormat(QImage::Format_RGB888);
        auto ref=reference.convertToFormat(QImage::Format_RGB888);double sum=0,square=0;quint64 n=0;
        for(int y=0;y<ref.height();++y)for(int x=0;x<ref.width()*3;++x){const double d=int(image.constScanLine(y)[x])-int(ref.constScanLine(y)[x]);sum+=std::abs(d);square+=d*d;++n;}
        std::cout<<"MAE8="<<sum/n<<" RMSE8="<<std::sqrt(square/n)<<'\n';};
    std::cout<<"before ";compare(before);std::cout<<"after ";compare(after);return 0;
}
