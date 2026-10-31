#include "CurveFit.hpp"
#include "SpectralLift.hpp"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <iostream>

namespace {
struct Input {QJsonObject object;QByteArray bytes;};
Input readInput(const QString& path){QFile source(path);if(!source.open(QIODevice::ReadOnly)||source.size()<=0||source.size()>4*1024*1024)throw std::runtime_error("Cannot read bounded source file");const auto bytes=source.readAll();if(bytes.size()!=source.size())throw std::runtime_error("Incomplete source read");QJsonParseError error;const auto doc=QJsonDocument::fromJson(bytes,&error);if(error.error!=QJsonParseError::NoError||!doc.isObject())throw std::runtime_error("Invalid source JSON");return {doc.object(),bytes};}
QString sha256(const QByteArray& bytes){return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());}
QJsonObject provenance(const Input& input){const auto sha=sha256(input.bytes);QString filename;
    if(sha=="ec23f1756de5597a86ed779676ba8546595c6e590b9749b53abb41809ee0e75e")filename="ProImage100_datasheet_curves.json";
    if(sha=="342ad845bd70f0f70ea008e8ac670db43e15352260ca12244c14a89baf34c968")filename="ProImage100_dye_density.json";
    if(sha=="b1a6bfe6613b9a6b8430f57fb614edc8c69bb164d35e11c84271029fc94d4ea7")filename="ProImage100_spectral_sensitivity.json";
    QJsonObject p{{"sha256",sha},{"source",input.object["source"]}};if(!filename.isEmpty()){p["attribution"]="Film Scan Calibration by OwlMightyCh";p["license"]="CC-BY-4.0";p["license_url"]="https://creativecommons.org/licenses/by/4.0/";p["source_url"]="https://github.com/OwlMightyCh/film-scan-calibration/blob/8d03a0e0c111625b8a88fb8647a35d95b491249d/data/films/"+filename;}
    else{p["license"]="Not certified by this tool; third-party rights retained. Input is not redistributed by the program.";}return p;
}
}

int main(int argc,char** argv){QCoreApplication app(argc,argv);QCommandLineParser parser;parser.setApplicationDescription("Offline Status M research; never installs a material Profile");parser.addHelpOption();parser.addOptions({{"certify-bounds","Add v2 analytical density feasibility certificate (requires --spectral)"},{{"i","input"},"Digitized datasheet JSON","file"},{{"o","output"},"NEW fit-report JSON (never overwritten)","file"},{"spectral","Audit bounded spectral layer inversion"},{"dyes","Dye spectral JSON (required with --spectral)","file"},{"responsivity","Explicit external instrument-response JSON; never bundled","file"},{"sensitivity","Sensitivity JSON (required with --spectral)","file"}});parser.process(app);
try{
    if(!parser.isSet("input")||!parser.isSet("output"))throw std::runtime_error("Require --input and --output");
    const auto input=readInput(parser.value("input"));const bool spectral=parser.isSet("spectral");
    if(parser.isSet("certify-bounds")&&!spectral)throw std::runtime_error("--certify-bounds requires --spectral");
    if(spectral&&(!parser.isSet("dyes")||!parser.isSet("responsivity")||!parser.isSet("sensitivity")))throw std::runtime_error("Spectral mode requires --dyes, --responsivity and --sensitivity; no implicit operator or tail completion");
    if(!spectral&&(parser.isSet("dyes")||parser.isSet("responsivity")||parser.isSet("sensitivity")))throw std::runtime_error("Spectral inputs require --spectral; no silently ignored options");
    QJsonObject report;
    if(spectral){const auto dyes=readInput(parser.value("dyes")),response=readInput(parser.value("responsivity")),sensitivity=readInput(parser.value("sensitivity"));report=material_fit::liftDocument(input.object,dyes.object,response.object,sensitivity.object,parser.isSet("certify-bounds"));report["inputs"]=QJsonObject{{"curves",provenance(input)},{"dyes",provenance(dyes)},{"responsivity",provenance(response)},{"sensitivity",provenance(sensitivity)}};}
    else report=material_fit::fitDocument(input.object);
    const auto sha=sha256(input.bytes);report["source_sha256"]=sha;
    if(spectral&&parser.isSet("certify-bounds")&&sha=="ec23f1756de5597a86ed779676ba8546595c6e590b9749b53abb41809ee0e75e") {
        report["manufacturer_unit_audit"]=QJsonObject{{"publication","Kodak E-4L, July 1997, printed page 3"},{"source_url","https://analogfilm.space/assets/kodak-proimage-100.n3IH3y40.pdf"},{"pdf_sha256","f2d1a1faa32bbbc96ab4f69ffa5ee7d99f8bc554392dcec42f715fb934d92700"},
            {"sensitivity_energy_exposure_unit","erg/cm^2"},{"one_erg_per_cm2_in_J_per_m2",0.001},{"characteristic_exposure_unit","lux seconds"},{"sensitivity_density_above_Dmin",0.2},{"sensitivity_exposure_seconds",0.02},
            {"absolute_exposure_anchor_calibrated",false},{"scope","Units verified in the manufacturer-authored historical sheet; not a calibration of missing spectral support, radiometric illuminant or dye-layer exposure response."}};
    }
    if(sha=="ec23f1756de5597a86ed779676ba8546595c6e590b9749b53abb41809ee0e75e"){
        report["input_attribution"]="Film Scan Calibration by OwlMightyCh; digitized Kodak Pro Image 100 datasheet curves";
        report["input_license"]="CC-BY-4.0";report["input_license_url"]="https://creativecommons.org/licenses/by/4.0/";
        report["source_url"]="https://github.com/OwlMightyCh/film-scan-calibration/blob/8d03a0e0c111625b8a88fb8647a35d95b491249d/data/films/ProImage100_datasheet_curves.json";
    }else{report["input_attribution"]=input.object["source"];report["input_license"]="Not declared by this tool; retain the source author's rights and licence before redistribution";}
    report["modifications"]=spectral?"New SpektraLab spectral forward/inverse and coverage diagnostics on supplied data/operator. Failed measurements retained, spectra and exposure axes unmodified. Not an emulsion Profile.":"New SpektraLab bounded CDF-plus-offset fit and residual/withheld-sample report; source measurements and axis unmodified. Not an emulsion Profile.";
    const auto output=QJsonDocument(report).toJson();
    QFile file(parser.value("output"));if(!file.open(QIODevice::WriteOnly|QIODevice::NewOnly))throw std::runtime_error("Output exists or cannot be created; nothing overwritten");if(file.write(output)!=output.size()||!file.flush()){file.close();file.remove();throw std::runtime_error("Incomplete report write");}
    const bool passed=report["mathematical_fit_passed"].toBool();std::cout<<(spectral?"Spectral reconstruction ":"Curve fit ")<<(passed?"passed":"failed residual/conditioning gate")<<"; profile_ready=false. Report: "<<parser.value("output").toStdString()<<'\n';return passed?0:3;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
