#pragma once
#include <QJsonObject>
#include <array>
#include <vector>

// OFFLINE research tool. These are instrument Status-M densities, not the
// emulsion-layer densities consumed by the physical engine. Never register
// the resulting fit report as a material Profile.
namespace material_fit {
struct Samples { std::vector<double> x,y; };
struct Model { double offset=0;std::array<double,3> amplitudes{},centers{},sigmas{}; };
struct Metrics { double rmse=0,max_abs=0;int count=0; };
struct Result { Model model;Metrics training,held_out;bool passed=false; };
double evaluate(const Model&,double x);
Result fit(const Samples&); // every fifth interior sample is held out
QJsonObject fitDocument(const QJsonObject&); // no Profile-shaped output
}
