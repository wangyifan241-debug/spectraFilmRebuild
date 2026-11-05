#pragma once
// One controlled replacement for Tone::positive; the Device remains unchanged.
struct ToneV02 {
 const Tone& tone;
 QJsonObject receipt;
 explicit ToneV02(const Tone& t):tone(t){}
 std::vector<float> positive(const std::vector<float>& transmission){
  if(!tone.monotonic)throw std::runtime_error("v0.2 requires strictly decreasing neutral device Y");
  auto out=transmission;double maxResidual=0,maxRatio=1;size_t saturated=0;
  constexpr double weight[3]={.2880402,.7118741,.0000857};
  const double bound=std::log(4.);
  for(size_t i=0;i<out.size();i+=3){
   double y=Tone::Y(&transmission[i]);size_t k=0;double f=0;
   if(y<=tone.axis.back()){k=tone.axis.size()-2;f=1;saturated++;}
   else if(y>=tone.axis.front()){saturated++;}
   else {while(k+1<tone.axis.size()-1&&y<tone.axis[k+1])k++;f=(tone.axis[k]-y)/(tone.axis[k]-tone.axis[k+1]);}
   double exposure=.184*std::exp2(-6.+.125*(k+f)),u[3],mean=0;
   for(int c=0;c<3;c++){
    double t=transmission[i+c],ref=tone.neutral[3*k+c]+f*(tone.neutral[3*(k+1)+c]-tone.neutral[3*k+c]);
    if(!(t>0&&ref>0&&std::isfinite(t)&&std::isfinite(ref)))throw std::runtime_error("v0.2 invalid device transmission; no clamping allowed");
    u[c]=std::log(ref/t);mean+=weight[c]*u[c];
   }
   double norm=0;for(double& c:u){c-=mean;norm+=c*c;}norm=std::sqrt(norm);
   double scale=norm>1e-12?std::tanh(norm/bound)/(norm/bound):1.;
   double shape[3],sum=0;for(int c=0;c<3;c++){shape[c]=std::exp(u[c]*scale);sum+=weight[c]*shape[c];}
   for(int c=0;c<3;c++)out[i+c]=float(exposure*shape[c]/sum);
   maxResidual=std::max(maxResidual,std::abs(Tone::Y(&out[i])/exposure-1));
   maxRatio=std::max(maxRatio,double(*std::max_element(out.begin()+i,out.begin()+i+3))/double(*std::min_element(out.begin()+i,out.begin()+i+3)));
  }
  receipt={{"maximum_relative_Y_vs_scalar_exposure_error",maxResidual},{"maximum_channel_ratio",maxRatio},{"endpoint_saturated_pixels",double(saturated)},{"log_chromatic_norm_bound",bound},{"Y_constraint_pass",maxResidual<2e-6},{"ratio_bound_pass",maxRatio<=16.00001}};
  return out;
 }
};
