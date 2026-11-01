#include "../../../windows_UI/tests/SyntheticDng.hpp"
#include <QCoreApplication>
int main(int argc,char** argv){QCoreApplication app(argc,argv);if(argc!=3)return 1;writeSyntheticDng(argv[1],1920,1280);QFile bad(argv[2]);if(!bad.open(QIODevice::WriteOnly|QIODevice::NewOnly))return 2;bad.write("invalid RAW");return 0;}
