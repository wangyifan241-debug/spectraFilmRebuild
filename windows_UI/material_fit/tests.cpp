#include "CurveFit.hpp"
#include "MaterialLibrary.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QTemporaryDir>
#include <QProcess>
#include <cmath>
#include <iostream>
#include <limits>
namespace {int checks=0;void check(bool b,const char* s){++checks;if(!b)throw std::runtime_error(s);}template<class F>void rejects(F f){try{f();}catch(const std::exception&){++checks;return;}throw std::runtime_error("Invalid fit input accepted");}}
int main(int argc,char** argv){QCoreApplication app(argc,argv);try{
    material_fit::Samples s;material_fit::Model truth{.21,{.8,.9,.6},{-1.,.3,1.2},{.4,.5,.6}};
    for(int i=0;i<121;++i){const double x=-3.+i*.05;s.x.push_back(x);s.y.push_back(material_fit::evaluate(truth,x));}
    const auto fit=material_fit::fit(s);check(fit.passed&&fit.held_out.rmse<.002&&fit.held_out.count==24,"Known synthetic CDF fit/holdout failed");
    for(int i=0;i<100;++i){const double x=-3+i*.06;check(std::abs(material_fit::evaluate(fit.model,x)-material_fit::evaluate(truth,x))<.01,"Synthetic reconstructed curve differs");}
    auto bad=s;bad.x[3]=bad.x[2];rejects([&]{material_fit::fit(bad);});bad=s;bad.y[1]=std::numeric_limits<double>::quiet_NaN();rejects([&]{material_fit::fit(bad);});bad=s;bad.y.resize(3);rejects([&]{material_fit::fit(bad);});
    auto rough=s;for(size_t i=1;i+1<rough.y.size();++i)rough.y[i]+=.2*std::sin(double(i)*1.8);check(!material_fit::fit(rough).passed,"Grossly unfit data was certified");
    check(argc==4,"Expected pinned research fixture, CLI and archived v1 report");QFile file(app.arguments()[1]);check(file.open(QIODevice::ReadOnly),"Cannot read research fixture");const auto bytes=file.readAll();check(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex()=="ec23f1756de5597a86ed779676ba8546595c6e590b9749b53abb41809ee0e75e","Research source bytes changed");const auto input=QJsonDocument::fromJson(bytes).object();
    const auto report=material_fit::fitDocument(input);check(report["mathematical_fit_passed"].toBool()&&!report["profile_ready"].toBool(),"Real Status M fit failed or was mislabeled as a Profile");
    check(report["log_exposure"]==input["char_curves"].toObject()["log_exposure"],"Fitter moved the measurement axis");
    check(QJsonDocument(report).toJson()==QJsonDocument(material_fit::fitDocument(input)).toJson(),"Fit report is not deterministic");
    rejects([&]{MaterialLibrary::validateProfile(report);});auto unsupported=input;auto curves=input["char_curves"].toObject();curves["log_exposure_units"]="unknown relative units";unsupported["char_curves"]=curves;rejects([&]{material_fit::fitDocument(unsupported);});
    QTemporaryDir owned;check(owned.isValid(),"Cannot create owned CLI evidence");const auto output=owned.filePath("fit.json");
    const auto run=[&](const QString& source,const QString& destination){QProcess p;p.start(app.arguments()[2],{"--input",source,"--output",destination});check(p.waitForFinished(15000)&&p.exitStatus()==QProcess::NormalExit,"Fit CLI crashed or timed out");return p.exitCode();};
    check(run(app.arguments()[1],output)==0,"Real CLI fit failed");QFile generated(output);check(generated.open(QIODevice::ReadOnly),"Missing CLI report");const auto reportBytes=generated.readAll();generated.close();const auto cli=QJsonDocument::fromJson(reportBytes).object();
    check(cli["input_license"]=="CC-BY-4.0"&&cli["input_attribution"].toString().contains("OwlMightyCh")&&cli["source_url"].toString().contains("8d03a0e0c111625b8a88fb8647a35d95b491249d")&&!cli["modifications"].toString().isEmpty(),"Generated report lost attribution, pinned URL, licence or change record");
    QFile previous(app.arguments()[3]);check(previous.open(QIODevice::ReadOnly)&&previous.readAll()==reportBytes,"Adding spectral mode changed the existing v1 CLI report bytes");
    check(run(app.arguments()[1],output)==1,"CLI overwrote existing output");check(generated.open(QIODevice::ReadOnly)&&generated.readAll()==reportBytes,"Refused overwrite changed the original report");generated.close();
    const auto invalid=owned.filePath("invalid.json"),invalidOutput=owned.filePath("invalid-output.json");QFile invalidFile(invalid);check(invalidFile.open(QIODevice::WriteOnly),"Cannot create invalid CLI fixture");invalidFile.write(QJsonDocument(unsupported).toJson());invalidFile.close();check(run(invalid,invalidOutput)==1&&!QFile::exists(invalidOutput),"Invalid CLI input left a partial output or was accepted");
    std::cout<<"Scan curve fit: "<<checks<<" checks; synthetic recovery, withheld samples, pinned Pro Image data, determinism and non-Profile refusal passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
