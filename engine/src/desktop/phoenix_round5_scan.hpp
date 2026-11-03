// Shared round-five spectral scan. Extracted from checkpoint 3b618ed8.
#pragma once
#include "pipeline/pipeline.hpp"
#include <algorithm>
#include <stdexcept>
namespace spk::phoenix5 {
inline thread_local std::string err;
inline void need(bool ok){if(!ok)throw std::runtime_error(err.empty()?"Phoenix scan failed":err);}
template<class Context> struct Scanner {
 Context& lab;gpu::BufferRef chd,base,ixs,ep,x,inv,y,matrix;Vec axis,target;Json recipe;size_t k=129;
 Scanner(Context& l,const Params& p,Pipeline& pipe):lab(l){
  Vec dyes=p.film.data.channel_density,zero(81,0),weights(243),illum;need(l.colour.illuminant_values("D50",illum,err));
  size_t missing=0;for(size_t w=0;w<81;w++){bool valid=std::isfinite(p.film.data.base_density[w]);for(int c=0;c<3;c++)valid&=std::isfinite(dyes[3*w+c]);if(!valid){missing++;for(int c=0;c<3;c++)dyes[3*w+c]=0;}}
  const double centres[3]={610,540,450},sigma[3]={30,30,25};
  for(int c=0;c<3;c++){double norm=0;for(size_t w=0;w<81;w++){double z=(380.+5*w-centres[c])/sigma[c];bool valid=std::isfinite(p.film.data.base_density[w]);for(int j=0;j<3;j++)valid&=std::isfinite(p.film.data.channel_density[3*w+j]);weights[3*w+c]=valid?illum[w]*std::exp(-.5*z*z):0;norm+=weights[3*w+c];}if(!(norm>0))throw std::runtime_error("empty detector");for(size_t w=0;w<81;w++)weights[3*w+c]/=norm;}
  chd=l.gpu->upload_persistent_f32(dyes.data(),dyes.size(),err);base=l.gpu->upload_persistent_f32(zero.data(),zero.size(),err);ixs=l.gpu->upload_persistent_f32(weights.data(),weights.size(),err);const float e[6]={1,1,1,0,0,0};ep=l.gpu->upload(e,sizeof e,err);
  std::vector<float> neutral(k*3);for(size_t i=0;i<k;i++)for(int c=0;c<3;c++)neutral[3*i+c]=float(.184*std::exp2(-6.+12.*i/(k-1)));
  auto negative=l.film(pipe,l.upload(neutral,unsigned(k),1));auto measured=l.values(spectral(negative));axis.resize(k*3);target.resize(k*3);Vec steps((k-1)*3);
  for(size_t i=0;i<k;i++)for(int c=0;c<3;c++){axis[3*i+c]=measured[3*(k-1-i)+c];target[3*i+c]=std::log10(neutral[3*(k-1-i)+c]);}
  for(size_t i=0;i+1<k;i++)for(int c=0;c<3;c++){double d=axis[3*(i+1)+c]-axis[3*i+c];if(!(d>1e-8))throw std::runtime_error("Noninvertible neutral scanner response; no sorting/repair allowed");steps[3*i+c]=1/d;}
  x=l.gpu->upload_persistent_f32(axis.data(),axis.size(),err);inv=l.gpu->upload_persistent_f32(steps.data(),steps.size(),err);y=l.gpu->upload_persistent_f32(target.data(),target.size(),err);
  // Synthetic scene Jacobian around neutral, NOT a real-film parameter fit.
  std::vector<float> probes(4*3,.184f);for(int c=0;c<3;c++)probes[3*(c+1)+c]+=.01f;
  auto probesNeg=l.film(pipe,l.upload(probes,4,1));auto logs=l.values(invert(spectral(probesNeg)));Mat3 jac;
  for(int row=0;row<3;row++)for(int col=0;col<3;col++)jac.m[row][col]=(std::pow(10.,logs[3*(col+1)+row])-std::pow(10.,logs[row]))/.01;
  auto m=jac.inverse();double jnorm=0,inorm=0;for(int row=0;row<3;row++){double a=0,b=0;for(int c=0;c<3;c++){a+=std::abs(jac.m[row][c]);b+=std::abs(m.m[row][c]);}jnorm=std::max(jnorm,a);inorm=std::max(inorm,b);}if(!std::isfinite(jnorm*inorm)||jnorm*inorm>10000)throw std::runtime_error("Ill-conditioned synthetic scanner Jacobian");for(int row=0;row<3;row++){double sum=m.m[row][0]+m.m[row][1]+m.m[row][2];if(!std::isfinite(sum)||std::abs(sum)<1e-6)throw std::runtime_error("singular scanner matrix");for(int col=0;col<3;col++)m.m[row][col]/=sum;}
  // Round five: fixed multi-exposure synthetic instrument calibration.
  // Neutral is constrained exactly; no photograph or per-image statistics enter.
  std::vector<float> training;
  for(int ev=-2;ev<=2;ev++)for(int r=1;r<=4;r++)for(int g=1;g<=4;g++)for(int b=1;b<=4;b++){
   double scale=.184*std::exp2(ev)/4.;training.push_back(r*scale);training.push_back(g*scale);training.push_back(b*scale);
  }
  auto trainNeg=l.film(pipe,l.upload(training,unsigned(training.size()/3),1));
  auto trainLogs=l.values(invert(spectral(trainNeg)));
  double aa=1e-4,ab=0,bb=1e-4,ay[3]={},by[3]={};
  for(size_t i=0;i<training.size();i+=3){double v[3];for(int c=0;c<3;c++)v[c]=std::pow(10.,trainLogs[i+c]);
   double norm=std::max(.02,double(training[i]+training[i+1]+training[i+2]));double a=(v[0]-v[2])/norm,b=(v[1]-v[2])/norm;
   aa+=a*a;ab+=a*b;bb+=b*b;for(int c=0;c<3;c++){double y=(training[i+c]-v[2])/norm;ay[c]+=a*y;by[c]+=b*y;}}
  double determinant=aa*bb-ab*ab;if(!(determinant>1e-10))throw std::runtime_error("degenerate round5 calibration");
  for(int c=0;c<3;c++){m.m[c][0]=(ay[c]*bb-by[c]*ab)/determinant;m.m[c][1]=(by[c]*aa-ay[c]*ab)/determinant;m.m[c][2]=1-m.m[c][0]-m.m[c][1];}
  double raw[9];m.to_row_major(raw);for(double coefficient:raw)if(!std::isfinite(coefficient)||std::abs(coefficient)>100)throw std::runtime_error("Unstable scanner calibration coefficient");matrix=l.gpu->upload_persistent_f32(raw,9,err);
  Mat3 prophotoXYZ;need(l.colour.matrix_RGB_to_XYZ("ProPhoto RGB",nullptr,"CAT02",prophotoXYZ,err));double xyzMatrix[9];prophotoXYZ.to_row_major(xyzMatrix);
  recipe=Json::object();
  auto save=[&](const char* key,const Vec& values){Json a=Json::array();for(double v:values)a.push(Json(v));recipe.set(key,a);};
  save("linear_prophoto_to_XYZ_D50",Vec(xyzMatrix,xyzMatrix+9));save("wavelength_nm",l.colour.wavelengths());save("normalized_detector_weights",weights);save("neutral_axis_logT",axis);save("neutral_axis_logPositive",target);save("scanner_to_linear_prophoto",Vec(raw,raw+9));
  recipe.set("status",Json(std::string("IDEALIZED_PROXY_NOT_VERIFIED")));recipe.set("matrix_source",Json(std::string("neutral-constrained ridge fit, 320 fixed synthetic probes across -2..+2 EV; independent per generation; no real-film fit")));recipe.set("excluded_missing_wavelengths",Json(double(missing)));recipe.set("jacobian_condition_inf",Json(jnorm*inorm));

 }
 Image spectral(const Image& n){Image o=lab.alloc(n);uint32_t meta[3]={uint32_t(n.pixels()),81,0};need(lab.gpu->dispatch("spk_spectral_epilogue",{gpu::Arg::buf(n.buf),gpu::Arg::buf(chd),gpu::Arg::buf(base),gpu::Arg::buf(ixs),gpu::Arg::buf(ep),gpu::Arg::inline_bytes(meta,3),gpu::Arg::buf(o.buf)},n.pixels(),err));need(lab.gpu->flush(err));return o;}
 Image invert(const Image& n){Image o=lab.alloc(n);uint32_t meta[2]={uint32_t(k),uint32_t(n.pixels())};need(lab.gpu->dispatch("spk_curves",{gpu::Arg::buf(n.buf),gpu::Arg::buf(x),gpu::Arg::buf(inv),gpu::Arg::buf(y),gpu::Arg::inline_bytes(meta,2),gpu::Arg::buf(o.buf)},n.pixels(),err));need(lab.gpu->flush(err));return o;}
 Image positive(const Image& logs){Image o=lab.alloc(logs);uint32_t meta[1]={uint32_t(logs.pixels())};need(lab.gpu->dispatch("spk_round4_scan",{gpu::Arg::buf(logs.buf),gpu::Arg::buf(matrix),gpu::Arg::inline_bytes(meta,1),gpu::Arg::buf(o.buf)},logs.pixels(),err));need(lab.gpu->flush(err));return o;}
};
}
