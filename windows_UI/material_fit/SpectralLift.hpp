#pragma once
#include <QJsonObject>
#include <array>
#include <vector>

// Offline inverse of an explicitly supplied instrument operator. None of
// this is a renderer fallback, a Profile exporter or a new physical control.
namespace material_fit {
using Triple=std::array<double,3>;
struct SpectralModel {
    std::vector<double> wavelengths,base;
    std::vector<Triple> dyes,weights; // wavelength x CMY / instrument RGB
};
struct Projection { Triple density{};std::array<Triple,3> jacobian{}; };
struct Lifted {
    Triple amounts{},predicted{},residual{};
    double max_abs=0,condition=0;
    int bound_hits=0;
    bool identifiable=false;
};
Projection project(const SpectralModel&,const Triple& amounts);
Triple lowerDensityBounds(const SpectralModel&);
Lifted invert(const SpectralModel&,const Triple& instrumentDensity);
QJsonObject liftDocument(const QJsonObject& curves,const QJsonObject& dyes,
                         const QJsonObject& responsivity,const QJsonObject& sensitivity,bool certifyBounds=false);
}
