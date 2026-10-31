#include "CurveFit.hpp"
#include <QJsonArray>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace material_fit {
namespace {
using Parameters=std::array<double,10>;
constexpr double sqrt2=1.4142135623730950488,invSqrt2Pi=.39894228040143267794;
void require(bool b,const char* reason){if(!b)throw std::runtime_error(reason);}
Model decode(const Parameters& p){Model m;m.offset=p[0];for(int k=0;k<3;++k){m.amplitudes[k]=p[1+3*k];m.centers[k]=p[2+3*k];m.sigmas[k]=std::exp(p[3+3*k]);}return m;}
bool heldOut(int i,int count){return i>0&&i<count-1&&i%5==2;}
double loss(const Samples& s,const Parameters& p){const auto m=decode(p);double v=0;for(int i=0;i<int(s.x.size());++i)if(!heldOut(i,int(s.x.size()))){const double r=evaluate(m,s.x[i])-s.y[i];v+=r*r;}return v;}
bool solve(double a[10][11],Parameters& x){
    for(int i=0;i<10;++i){int pivot=i;for(int j=i+1;j<10;++j)if(std::abs(a[j][i])>std::abs(a[pivot][i]))pivot=j;if(std::abs(a[pivot][i])<1e-18)return false;
        for(int k=i;k<11;++k)std::swap(a[i][k],a[pivot][k]);const double d=a[i][i];for(int k=i;k<11;++k)a[i][k]/=d;
        for(int j=0;j<10;++j)if(j!=i){const double f=a[j][i];for(int k=i;k<11;++k)a[j][k]-=f*a[i][k];}}
    for(int i=0;i<10;++i)x[i]=a[i][10];return true;
}
Metrics measure(const Samples& s,const Model& m,bool held){Metrics r;for(int i=0;i<int(s.x.size());++i)if(heldOut(i,int(s.x.size()))==held){const double e=std::abs(evaluate(m,s.x[i])-s.y[i]);r.rmse+=e*e;r.max_abs=std::max(r.max_abs,e);++r.count;}r.rmse=std::sqrt(r.rmse/r.count);return r;}
QJsonArray values(const std::array<double,3>& a){return {a[0],a[1],a[2]};}
QJsonObject metric(const Metrics& m){return {{"count",m.count},{"rmse_density",m.rmse},{"max_abs_density",m.max_abs}};}
std::vector<double> numbers(const QJsonValue& v){require(v.isArray(),"Expected numeric array");std::vector<double> out;require(v.toArray().size()<=4096,"Too many measured samples");for(const auto x:v.toArray()){require(x.isDouble()&&std::isfinite(x.toDouble()),"Non-finite or missing measured sample");out.push_back(x.toDouble());}return out;}
}
double evaluate(const Model& m,double x){double v=m.offset;for(int k=0;k<3;++k)v+=m.amplitudes[k]*.5*std::erfc(-(x-m.centers[k])/(sqrt2*m.sigmas[k]));return v;}
Result fit(const Samples& s){
    require(s.x.size()==s.y.size()&&s.x.size()>=30&&s.x.size()<=4096,"Need 30–4096 paired characteristic samples");
    for(size_t i=0;i<s.x.size();++i){require(std::isfinite(s.x[i])&&std::isfinite(s.y[i])&&s.y[i]>=0&&s.y[i]<=10,"Invalid exposure/density sample");require(!i||s.x[i]>s.x[i-1],"Exposure axis must strictly increase");}
    const double range=s.x.back()-s.x.front(),span=s.y.back()-s.y.front();require(range>.01&&range<100&&span>.05,"Need an increasing negative-film curve with usable range");
    Parameters best{};double bestLoss=std::numeric_limits<double>::infinity();
    for(int start=0;start<8;++start){
        Parameters p{};p[0]=s.y.front();for(int k=0;k<3;++k){p[1+3*k]=span/3;p[2+3*k]=s.x.front()+range*(.15+.30*k+.035*(start%4));p[3+3*k]=std::log(range*(start<4?.13:.25));}
        double current=loss(s,p),lambda=.01;
        for(int iteration=0;iteration<240;++iteration){
            double normal[10][11]{};const auto m=decode(p);
            for(int i=0;i<int(s.x.size());++i)if(!heldOut(i,int(s.x.size()))){
                const double residual=s.y[i]-evaluate(m,s.x[i]);Parameters j{};j[0]=1;
                for(int k=0;k<3;++k){const double z=(s.x[i]-m.centers[k])/m.sigmas[k],pdf=invSqrt2Pi*std::exp(-.5*z*z);j[1+3*k]=.5*std::erfc(-z/sqrt2);j[2+3*k]=-m.amplitudes[k]*pdf/m.sigmas[k];j[3+3*k]=-m.amplitudes[k]*pdf*z;}
                for(int a=0;a<10;++a){normal[a][10]+=j[a]*residual;for(int b=0;b<10;++b)normal[a][b]+=j[a]*j[b];}}
            for(int a=0;a<10;++a)normal[a][a]+=lambda*(normal[a][a]+1e-8);Parameters step{};if(!solve(normal,step))break;
            auto trial=p;for(int a=0;a<10;++a)trial[a]+=step[a];trial[0]=std::clamp(trial[0],0.,s.y.front()+.2);
            for(int k=0;k<3;++k){trial[1+3*k]=std::clamp(trial[1+3*k],0.,10.);trial[2+3*k]=std::clamp(trial[2+3*k],s.x.front()-range,s.x.back()+range);trial[3+3*k]=std::clamp(trial[3+3*k],std::log(range*.01),std::log(range*2));}
            const double candidate=loss(s,trial);if(candidate<current){const double change=current-candidate;p=trial;current=candidate;lambda=std::max(1e-10,lambda*.3);if(change<1e-13)break;}else{lambda*=8;if(lambda>1e14)break;}
        }
        if(current<bestLoss){bestLoss=current;best=p;}
    }
    Result r;r.model=decode(best);
    // Canonicalise equivalent mixture permutations, not measured samples.
    for(int a=0;a<3;++a)for(int b=a+1;b<3;++b)if(r.model.centers[a]>r.model.centers[b]){std::swap(r.model.centers[a],r.model.centers[b]);std::swap(r.model.amplitudes[a],r.model.amplitudes[b]);std::swap(r.model.sigmas[a],r.model.sigmas[b]);}
    r.training=measure(s,r.model,false);r.held_out=measure(s,r.model,true);
    r.passed=r.training.rmse<=.02&&r.held_out.rmse<=.02&&r.held_out.max_abs<=.08;
    return r;
}
QJsonObject fitDocument(const QJsonObject& input){
    require(input["source"].isString()&&!input["source"].toString().isEmpty(),"Source attribution is required");const auto c=input["char_curves"].toObject();
    require(c["log_exposure_units"].toString()=="log lux-seconds","Unsupported exposure units; no implicit axis rescaling");
    const auto x=numbers(c["log_exposure"]);const auto densities=c["statusM_density"].toObject();QJsonObject channels;bool passed=true;
    for(const auto key:{"R","G","B"}){const Samples s{x,numbers(densities[key])};const auto r=fit(s);passed=passed&&r.passed;
        QJsonArray predicted,residuals,indices;for(int i=0;i<int(x.size());++i){predicted.append(evaluate(r.model,x[i]));residuals.append(evaluate(r.model,x[i])-s.y[i]);if(heldOut(i,int(x.size())))indices.append(i);}
        channels[key]=QJsonObject{{"offset_density",r.model.offset},{"amplitudes",values(r.model.amplitudes)},{"centers",values(r.model.centers)},{"sigmas",values(r.model.sigmas)},
            {"training",metric(r.training)},{"held_out",metric(r.held_out)},{"passed",r.passed},{"predicted_density",predicted},{"residual_density",residuals},{"held_out_indices",indices}};}
    return {{"format","SpektraLab.ScanCurveFit"},{"version",1},{"algorithm","bounded-3-normal-cdf-plus-offset-lm-v1"},{"source",input["source"]},{"density_space","Status M instrument density, including base"},
        {"log_exposure_units","log lux-seconds"},{"log_exposure",c["log_exposure"]},{"channels",channels},{"mathematical_fit_passed",passed},{"profile_ready",false},
        {"thresholds",QJsonObject{{"training_rmse_D",.02},{"held_out_rmse_D",.02},{"held_out_max_abs_D",.08}}},
        {"validation_scope","Every fifth interior sample held out; correlated digitized points, NOT independent photographic validation. No exposure-axis shift, sample smoothing, spectral reconstruction or engine update."},
        {"blocking_gates",QJsonArray{"Convert instrument Status M densities to intrinsic dye-layer amounts with a validated spectral forward model","Resolve unmeasured spectral tails/base and surrogate dye uncertainty","Calibrate sensitivity/exposure anchor and reference illumination","Independent photographic, colour and Vulkan regression before any Profile registration"}}};
}
}
