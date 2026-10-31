#include "SpectralLift.hpp"
#include <QJsonArray>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace material_fit {
namespace {
constexpr double ln10=2.302585092994045684,amountLimit=8,maximumResidual=.02,maximumCondition=1e4;
using Matrix=std::array<Triple,3>;
void require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
void validate(const SpectralModel& m){
    const auto n=m.wavelengths.size();require(n>=3&&n<=4096&&m.base.size()==n&&m.dyes.size()==n&&m.weights.size()==n,"Invalid spectral model dimensions");Triple sum{};
    for(size_t l=0;l<n;++l){require(std::isfinite(m.wavelengths[l])&&m.wavelengths[l]>0&&m.wavelengths[l]<=10000&&(!l||m.wavelengths[l]>m.wavelengths[l-1]),"Spectral wavelength axis must increase within declared numeric bounds");require(std::isfinite(m.base[l])&&m.base[l]>=0&&m.base[l]<=20,"Invalid measured base density");for(int k=0;k<3;++k){require(std::isfinite(m.dyes[l][k])&&std::abs(m.dyes[l][k])<=10,"Invalid dye spectrum");require(std::isfinite(m.weights[l][k])&&m.weights[l][k]>=0,"Invalid instrument responsivity");sum[k]+=m.weights[l][k];}}
    for(const auto x:sum)require(std::isfinite(x)&&x>0,"Instrument channel has no positive responsivity");
}
Projection forward(const SpectralModel& m,const Triple& a){
    Projection out;
    // Log-sum-exp avoids losing a very dense sample to transmission underflow.
    for(int c=0;c<3;++c){double maxLog=-std::numeric_limits<double>::infinity(),normalization=0;
        for(size_t l=0;l<m.base.size();++l){const double w=m.weights[l][c];normalization+=w;if(w>0){double d=m.base[l];for(int k=0;k<3;++k)d+=a[k]*m.dyes[l][k];maxLog=std::max(maxLog,std::log(w)-ln10*d);}}
        double sum=0;Triple moments{};
        for(size_t l=0;l<m.base.size();++l)if(m.weights[l][c]>0){double d=m.base[l];for(int k=0;k<3;++k)d+=a[k]*m.dyes[l][k];const double light=std::exp(std::log(m.weights[l][c])-ln10*d-maxLog);sum+=light;for(int k=0;k<3;++k)moments[k]+=light*m.dyes[l][k];}
        out.density[c]=-(maxLog+std::log(sum)-std::log(normalization))/ln10;for(int k=0;k<3;++k)out.jacobian[c][k]=moments[k]/sum;
    }return out;
}
bool linearSolve(Matrix a,Triple b,Triple& result){
    for(int k=0;k<3;++k){int pivot=k;for(int r=k+1;r<3;++r)if(std::abs(a[r][k])>std::abs(a[pivot][k]))pivot=r;if(std::abs(a[pivot][k])<1e-14)return false;std::swap(a[k],a[pivot]);std::swap(b[k],b[pivot]);
        const double scale=a[k][k];for(int j=k;j<3;++j)a[k][j]/=scale;b[k]/=scale;for(int r=0;r<3;++r)if(r!=k){const double f=a[r][k];for(int j=k;j<3;++j)a[r][j]-=f*a[k][j];b[r]-=f*b[k];}}
    result=b;return true;
}
double condition(const Matrix& j){
    Matrix inv{};double norm=0,invNorm=0;for(int c=0;c<3;++c){Triple basis{},column{};basis[c]=1;if(!linearSolve(j,basis,column))return std::numeric_limits<double>::infinity();for(int r=0;r<3;++r)inv[r][c]=column[r];}
    for(int r=0;r<3;++r){double s=0,t=0;for(int c=0;c<3;++c){s+=std::abs(j[r][c]);t+=std::abs(inv[r][c]);}norm=std::max(norm,s);invNorm=std::max(invNorm,t);}return norm*invNorm;
}
double loss(const Triple& p,const Triple& y){double s=0;for(int c=0;c<3;++c)s+=(p[c]-y[c])*(p[c]-y[c]);return s;}
std::vector<double> numbers(const QJsonValue& v){require(v.isArray()&&v.toArray().size()<=4096,"Expected bounded numeric array");std::vector<double> result;for(const auto item:v.toArray()){require(item.isDouble()&&std::isfinite(item.toDouble()),"Missing/non-finite spectral sample; no implicit zero filling");result.push_back(item.toDouble());}return result;}
void increasing(const std::vector<double>& a){require(a.size()>=2,"Spectral axis is too short");for(size_t i=1;i<a.size();++i)require(a[i]>a[i-1],"Axis must increase strictly");}
double interpolate(const std::vector<double>& x,const std::vector<double>& y,double at){require(x.size()==y.size()&&at>=x.front()&&at<=x.back(),"Interpolation outside measured/operator support is forbidden");const auto upper=std::upper_bound(x.begin(),x.end(),at);if(upper==x.end())return y.back();const size_t i=size_t(upper-x.begin());if(i==0)return y.front();return y[i-1]+(y[i]-y[i-1])*(at-x[i-1])/(x[i]-x[i-1]);}
QJsonArray array(const Triple& x){return {x[0],x[1],x[2]};}
QJsonObject sensitivityAudit(const QJsonObject& s,const std::vector<double>& exposure,const std::array<std::vector<double>,3>& target,const Triple& base){
    require(s["source"].isString()&&s["measurement_note"].isString(),"Sensitivity provenance/measurement note required");const auto wl=numbers(s["wavelength_nm"]);increasing(wl);QJsonObject channels;
    for(int c=0;c<3;++c){const char* key=c==0?"cyan":c==1?"magenta":"yellow";const auto samples=s["log_sensitivity"].toObject()[key].toArray();require(samples.size()==qsizetype(wl.size()),"Sensitivity sample count mismatch");int finiteCount=0,nativeCount=0;double first=0,last=0;bool gap=false,seen=false,ended=false;
        for(int i=0;i<samples.size();++i){const auto v=samples[i];if(v.isNull()){if(seen)ended=true;continue;}require(v.isDouble()&&std::isfinite(v.toDouble()),"Invalid sensitivity sample");if(ended)gap=true;if(!seen){first=wl[i];seen=true;}last=wl[i];++finiteCount;}
        require(finiteCount>=2,"Sensitivity channel has no usable measured support");
        for(int nm=380;nm<=780;nm+=5){const auto at=std::lower_bound(wl.begin(),wl.end(),nm);if(at!=wl.end()&&*at==nm){const auto value=samples[int(at-wl.begin())];if(value.isDouble())++nativeCount;}}
        QJsonValue crossing;for(size_t i=1;i<exposure.size();++i)if(target[c][i-1]<=base[c]+.2&&target[c][i]>=base[c]+.2&&target[c][i]>target[c][i-1]){crossing=exposure[i-1]+(exposure[i]-exposure[i-1])*(base[c]+.2-target[c][i-1])/(target[c][i]-target[c][i-1]);break;}
        channels[key]=QJsonObject{{"measured_samples",finiteCount},{"support_nm",QJsonArray{first,last}},{"native_81_grid_measured_samples",nativeCount},{"internal_missing_samples",gap},{"log_lux_seconds_at_model_base_plus_0_2_D",crossing}};
    }
    return {{"source",s["source"]},{"measurement_note",s["measurement_note"]},{"channels",channels},{"exposure_anchor_calibrated",false},{"unknown_samples_are_not_zero",true},{"scope","Coverage and instrument-density crossing diagnostics only; no absolute sensitivity/photometric-to-radiometric calibration inferred."}};
}
}
Projection project(const SpectralModel& model,const Triple& amounts){validate(model);for(const auto a:amounts)require(std::isfinite(a)&&a>=0&&a<=amountLimit,"Dye amount outside declared research bounds");return forward(model,amounts);}
Triple lowerDensityBounds(const SpectralModel& model){
    validate(model);auto relaxed=model;
    // Relax the common CMY amounts into independent per-wavelength choices.
    // This can only increase maximum transmitted light, hence LOWER density.
    // A target below this bound is impossible in the original box as well:
    // no nonlinear optimiser or initial seed is involved in this certificate.
    for(size_t l=0;l<model.base.size();++l)for(int k=0;k<3;++k)relaxed.base[l]+=amountLimit*std::min(0.,model.dyes[l][k]);
    return forward(relaxed,{}).density;
}
Lifted invert(const SpectralModel& model,const Triple& target){
    validate(model);for(const auto y:target)require(std::isfinite(y)&&y>=0&&y<=20,"Invalid instrument density target");const auto baseline=forward(model,{});Triple linearSeed{},delta{};for(int c=0;c<3;++c)delta[c]=target[c]-baseline.density[c];linearSolve(baseline.jacobian,delta,linearSeed);for(auto& a:linearSeed)a=std::clamp(a,0.,amountLimit);
    Triple best{};double bestLoss=std::numeric_limits<double>::infinity();
    for(const auto seed:{Triple{},linearSeed,Triple{1,1,1},Triple{3,3,3}}){auto a=seed;double current=loss(forward(model,a).density,target),lambda=1e-3;
        for(int it=0;it<100;++it){const auto p=forward(model,a);Matrix normal{};Triple rhs{},step{};for(int c=0;c<3;++c)for(int j=0;j<3;++j){rhs[j]+=p.jacobian[c][j]*(target[c]-p.density[c]);for(int k=0;k<3;++k)normal[j][k]+=p.jacobian[c][j]*p.jacobian[c][k];}for(int j=0;j<3;++j)normal[j][j]+=lambda*(normal[j][j]+1e-8);if(!linearSolve(normal,rhs,step))break;
            auto trial=a;for(int j=0;j<3;++j)trial[j]=std::clamp(a[j]+step[j],0.,amountLimit);const auto candidate=loss(forward(model,trial).density,target);
            if(candidate<current){const double change=current-candidate;a=trial;current=candidate;lambda=std::max(1e-12,lambda*.3);if(change<1e-14||current<1e-20)break;}else{lambda*=8;if(lambda>1e12)break;}
        }if(current<bestLoss){bestLoss=current;best=a;}
    }
    Lifted out;out.amounts=best;const auto projected=forward(model,best);out.predicted=projected.density;out.condition=condition(projected.jacobian);out.identifiable=std::isfinite(out.condition)&&out.condition<=maximumCondition;
    for(int c=0;c<3;++c){out.residual[c]=out.predicted[c]-target[c];out.max_abs=std::max(out.max_abs,std::abs(out.residual[c]));if(best[c]<1e-8||best[c]>amountLimit-1e-8)++out.bound_hits;}return out;
}
QJsonObject liftDocument(const QJsonObject& curves,const QJsonObject& dye,const QJsonObject& response,const QJsonObject& sensitivity,bool certifyBounds){
    require(curves["source"].isString()&&dye["source"].isString()&&response["source"].isString(),"Every spectral input needs provenance");const auto chart=curves["char_curves"].toObject(),spectral=curves["spectral"].toObject(),dyes=dye["shared_full_curves"].toObject();require(chart["log_exposure_units"].toString()=="log lux-seconds","Unsupported exposure unit");
    const auto x=numbers(chart["log_exposure"]);increasing(x);require(x.size()>=30,"Insufficient characteristic samples");const auto wl=numbers(dyes["wavelength_nm"]),baseWl=numbers(spectral["wavelength_nm"]),operatorWl=numbers(response["wavelength_nm"]);increasing(wl);increasing(operatorWl);require(operatorWl.front()>0&&operatorWl.back()<=10000&&operatorWl.back()-operatorWl.front()<=2000,"Operator wavelength range exceeds bounded research integration");require(wl==baseWl,"Base and dye grids differ; no hidden resampling");
    SpectralModel model;model.wavelengths=wl;model.base=numbers(spectral["dmin"]);require(model.base.size()==wl.size(),"Base sample count mismatch");for(size_t i=1;i<wl.size();++i)require(wl[i]-wl[i-1]==1,"This research integration requires a declared uniform 1 nm grid");
    std::array<std::vector<double>,3> dyeColumns,operatorColumns,target;for(int c=0;c<3;++c){dyeColumns[c]=numbers(dyes[c==0?"cyan":c==1?"magenta":"yellow"]);operatorColumns[c]=numbers(response["responsivity_linear_peak1"].toObject()[c==0?"red":c==1?"green":"blue"]);target[c]=numbers(chart["statusM_density"].toObject()[c==0?"R":c==1?"G":"B"]);require(dyeColumns[c].size()==wl.size()&&operatorColumns[c].size()==operatorWl.size()&&target[c].size()==x.size(),"Spectral/characteristic sample count mismatch");for(const auto v:operatorColumns[c])require(v>=0,"Negative responsivity");}
    for(size_t l=0;l<wl.size();++l){Triple d{},w{};for(int c=0;c<3;++c){d[c]=dyeColumns[c][l];w[c]=interpolate(operatorWl,operatorColumns[c],wl[l]);}model.dyes.push_back(d);model.weights.push_back(w);}validate(model);
    const auto baseline=project(model,{}).density;Triple omitted{},lowerCount{},squared{},maxResidual{},negativeDyes{};
    for(int c=0;c<3;++c){double full=0,missing=0;for(double nm=operatorWl.front();nm<=operatorWl.back();nm+=1){const double w=interpolate(operatorWl,operatorColumns[c],nm);full+=w;if(nm<wl.front()||nm>wl.back())missing+=w;}require(std::isfinite(full)&&full>0&&std::isfinite(missing),"Invalid full instrument channel sum");omitted[c]=missing/full;for(const auto v:dyeColumns[c])if(v<0)negativeDyes[c]+=1;}
    QJsonArray rows;int rejected=0,unidentifiable=0;for(size_t i=0;i<x.size();++i){Triple y{};for(int c=0;c<3;++c){y[c]=target[c][i];if(y[c]<baseline[c])lowerCount[c]+=1;}const auto lifted=invert(model,y);const bool passed=lifted.max_abs<=maximumResidual&&lifted.identifiable;if(!passed)++rejected;if(!lifted.identifiable)++unidentifiable;
        for(int c=0;c<3;++c){squared[c]+=lifted.residual[c]*lifted.residual[c];maxResidual[c]=std::max(maxResidual[c],std::abs(lifted.residual[c]));}
        rows.append(QJsonObject{{"log_exposure",x[i]},{"target_status_m_RGB",array(y)},{"amounts_CMY",array(lifted.amounts)},{"predicted_status_m_RGB",array(lifted.predicted)},{"residual_RGB",array(lifted.residual)},
            {"condition_inf",std::isfinite(lifted.condition)?QJsonValue(lifted.condition):QJsonValue()},{"bound_hits",lifted.bound_hits},{"identifiable",lifted.identifiable},{"passed",passed}});
    }for(auto& v:squared)v=std::sqrt(v/x.size());
    QJsonObject report{{"format","SpektraLab.SpectralLift"},{"version",1},{"algorithm","log-transmission-analytic-jacobian-bounded-lm-v1"},{"profile_ready",false},{"mathematical_fit_passed",rejected==0},
        {"source",curves["source"]},{"dye_source",dye["source"]},{"dye_uncertainty",dye["uncertainty"]},{"operator_source",response["source"]},{"operator_attestation","Caller-supplied instrument table; not certified by this tool as an ISO standard; not relicensed or redistributed by the program."},
        {"integration",QJsonObject{{"band_nm",QJsonArray{wl.front(),wl.back()}},{"step_nm",1},{"rule","uniform point sum, inclusive endpoints; linear responsivity interpolation within its supplied support"},{"omitted_operator_weight_fraction_RGB",array(omitted)},{"base_status_m_RGB",array(baseline)},{"negative_surrogate_dye_samples_CMY",array(negativeDyes)}}},
        {"thresholds",QJsonObject{{"max_reconstruction_error_D",maximumResidual},{"max_local_condition_inf",maximumCondition},{"dye_amount_bounds",QJsonArray{0,amountLimit}}}},
        {"summary",QJsonObject{{"rows",int(x.size())},{"rejected_rows",rejected},{"unidentifiable_rows",unidentifiable},{"rmse_RGB_D",array(squared)},{"max_abs_RGB_D",array(maxResidual)},{"targets_below_model_base_RGB",array(lowerCount)}}},
        {"sensitivity_audit",sensitivityAudit(sensitivity,x,target,baseline)},{"rows",rows},
        {"scope","Reconstruction closes the specified truncated spectral operator only, not independent photographic validation. All failed targets retained; no offsets, clipping of measured samples, exposure shifts or spectral tail invention."},
        {"blocking_gates",QJsonArray{"Every target must reconstruct within the declared error/conditioning gate","Full spectral support and surrogate dye/base uncertainty remain unresolved","Instrument-density inversion does not establish absolute sensitivity/exposure calibration","Independent photographic colour validation required before any Profile registration"}}};
    if(certifyBounds){
        const auto lower=lowerDensityBounds(model);QJsonArray impossibleRows;Triple below{},outsideTolerance{};
        for(size_t i=0;i<x.size();++i){Triple gap{};bool impossible=false;for(int c=0;c<3;++c){gap[c]=lower[c]-target[c][i];if(gap[c]>1e-12)++below[c];if(gap[c]>maximumResidual+1e-12){++outsideTolerance[c];impossible=true;}}
            if(impossible)impossibleRows.append(QJsonObject{{"row",int(i)},{"log_exposure",x[i]},{"minimum_density_minus_target_RGB",array(gap)}});
        }
        report["version"]=2;
        report["feasibility_certificate"]=QJsonObject{{"method","per-wavelength box relaxation of transmission; stable log-sum-exp"},{"density_lower_bounds_RGB",array(lower)},{"targets_below_bound_RGB",array(below)},{"targets_beyond_error_gate_RGB",array(outsideTolerance)},{"proved_unreachable_rows",impossibleRows},
            {"scope","Necessary bound only, within supplied/truncated operator and dye amounts 0..8. Passing this bound does not prove feasibility or calibrate a Profile. No fitted baseline/offset."}};
    }
    return report;
}
}
