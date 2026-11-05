#pragma once
// Research-only CPU reconstruction around real Vulkan film/spectral stages.
struct Device {
 Lab& l; Vec weights, xyzWeights; Mat3 detectorXYZ, xyzRGB; QJsonObject receipt;
 Device(Lab& lab,const Params& a,const Params& b):l(lab),weights(243),xyzWeights(243){
  Vec illum;need(l.colour.illuminant_values("D50",illum,err));auto cmf=l.colour.cmfs_1931_2deg();
  double norms[3]={},yn=0;int excluded=0;
  for(int w=0;w<81;w++){
   bool valid=std::isfinite(a.film.data.base_density[w])&&std::isfinite(b.film.data.base_density[w]);
   for(int c=0;c<3;c++)valid &= std::isfinite(a.film.data.channel_density[3*w+c])&&std::isfinite(b.film.data.channel_density[3*w+c]);
   excluded+=!valid;yn+=illum[w]*cmf[3*w+1];
   for(int c=0;c<3;c++){double z=(380+5*w-(c==0?610:c==1?540:450))/double(c==2?25:30);weights[3*w+c]=valid?illum[w]*std::exp(-.5*z*z):0;norms[c]+=weights[3*w+c];}
  }
  for(int w=0;w<81;w++)for(int c=0;c<3;c++){weights[3*w+c]/=norms[c];xyzWeights[3*w+c]=illum[w]*cmf[3*w+c]/yn;}
  double white[3]={};for(int w=0;w<81;w++)for(int c=0;c<3;c++)white[c]+=xyzWeights[3*w+c];
  // Solve two coefficients per XYZ row; the third enforces a flat-transmission white.
  double aa=1e-8,ab=0,bb=1e-8,ay[3]={},by[3]={};
  for(int i=0;i<400;i++){double s[3],xyz[3];target(i,false,s,xyz);double u=s[0]-s[2],v=s[1]-s[2];aa+=u*u;ab+=u*v;bb+=v*v;for(int c=0;c<3;c++){double y=xyz[c]-white[c]*s[2];ay[c]+=u*y;by[c]+=v*y;}}
  for(int c=0;c<3;c++){detectorXYZ.m[c][0]=(ay[c]*bb-by[c]*ab)/(aa*bb-ab*ab);detectorXYZ.m[c][1]=(by[c]*aa-ay[c]*ab)/(aa*bb-ab*ab);detectorXYZ.m[c][2]=white[c]-detectorXYZ.m[c][0]-detectorXYZ.m[c][1];}
  need(l.colour.matrix_XYZ_to_RGB("ProPhoto RGB",nullptr,"CAT02",xyzRGB,err));
  double mae=0,maxe=0;QJsonArray held;
  for(int i=0;i<100;i++){double s[3],xyz[3],pred[3];target(i,true,s,xyz);detectorXYZ.apply(s,pred);QJsonArray row;for(int c=0;c<3;c++){double e=std::abs(pred[c]-xyz[c]);mae+=e;maxe=std::max(maxe,e);row.append(pred[c]);row.append(xyz[c]);}held.append(row);}
  double m[9];detectorXYZ.to_row_major(m);receipt={{"detector_to_XYZ_row_major",array(Vec(m,m+9))},{"weights",array(weights)},{"excluded_wavelengths",excluded},{"heldout_XYZ_MAE",mae/300},{"heldout_XYZ_max_error",maxe},{"heldout_pass",mae/300<.03&&maxe<.15},{"heldout_predicted_target",held},{"calibration","400 known smooth spectral transmissions; 100 disjoint held-out; no film or scene RGB targets"}};
 }
 void target(int i,bool held,double* s,double* xyz){
  for(int c=0;c<3;c++)s[c]=xyz[c]=0;
  // Disjoint low-discrepancy parameter sequences; optical density, then transmission.
  const double offset=held?1000.5:0.;double j=i+offset;
  for(int w=0;w<81;w++){double wl=380+5*w,d=.03+.3*std::fmod(j*.61803398875,1.);
   for(int k=0;k<3;k++){double centre=405+345*std::fmod((j+1)*(k+1)*.41421356237,1.);double sigma=30+65*std::fmod((j+2)*(k+2)*.73205080757,1.);double z=(wl-centre)/sigma;d+=(.1+1.3*std::fmod((j+3)*(k+1)*.2360679775,1.))*std::exp(-.5*z*z);}
   double t=std::pow(10.,-d);for(int c=0;c<3;c++){s[c]+=t*weights[3*w+c];xyz[c]+=t*xyzWeights[3*w+c];}
  }
 }
 std::vector<float> optical(const std::vector<float>& logT){auto out=logT;for(size_t i=0;i<out.size();i+=3){double s[3],xyz[3],rgb[3];for(int c=0;c<3;c++)s[c]=std::pow(10.,logT[i+c])-1e-10;detectorXYZ.apply(s,xyz);xyzRGB.apply(xyz,rgb);for(int c=0;c<3;c++)out[i+c]=float(rgb[c]);}return out;}
};
struct Tone {
 std::vector<float> neutral;std::vector<double> axis;bool monotonic=true;
 static double Y(const float* p){return .2880402*p[0]+.7118741*p[1]+.0000857*p[2];}
 Tone(const std::vector<float>& n):neutral(n){for(size_t i=0;i<n.size();i+=3){axis.push_back(Y(&n[i]));if(i&&!(axis.back()<axis[axis.size()-2]))monotonic=false;}}
 std::vector<float> positive(const std::vector<float>& transmission){auto out=transmission;for(size_t i=0;i<out.size();i+=3){double y=Y(&transmission[i]);size_t k=0;double f=0;
   if(y<=axis.back()){k=axis.size()-2;f=1;}else if(y<axis.front()){while(k+1<axis.size()-1&&y<axis[k+1])k++;f=(axis[k]-y)/(axis[k]-axis[k+1]);}
   double exposure=.184*std::exp2(-6.+.125*(k+f));
   for(int c=0;c<3;c++){double ref=neutral[3*k+c]+f*(neutral[3*(k+1)+c]-neutral[3*k+c]);out[i+c]=transmission[i+c]>0?float(exposure*ref/transmission[i+c]):std::numeric_limits<float>::quiet_NaN();}
  }return out;}
};
