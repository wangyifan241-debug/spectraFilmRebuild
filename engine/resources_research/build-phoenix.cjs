// Deterministic OFFLINE generator. No Node/Python at application runtime.
const fs=require('fs'),path=require('path'),crypto=require('crypto');
const root=path.resolve(__dirname,'../..'),research=__dirname;
const config=JSON.parse(fs.readFileSync(path.join(research,'phoenix-model-assumptions.json')));
const donorFile=path.join(root,'engine/resources/profiles',config.dye_proxy+'.json');
const donor=JSON.parse(fs.readFileSync(donorFile));
const sha=file=>crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
function csv(file){const lines=fs.readFileSync(file,'utf8').trim().split(/\r?\n/),keys=lines.shift().split(',');return lines.map(l=>Object.fromEntries(l.split(',').map((v,i)=>[keys[i],v===''?null:Number.isFinite(Number(v))?Number(v):v])));}
const gauss=(x,m,s)=>Math.exp(-0.5*((x-m)/s)**2);
function interpolate(points,x){if(x<=points[0][0])return points[0][1];if(x>=points.at(-1)[0])return points.at(-1)[1];for(let i=1;i<points.length;i++)if(points[i][0]>=x){const a=points[i-1],b=points[i];return a[1]+(b[1]-a[1])*(x-a[0])/(b[0]-a[0]);}throw Error('Interpolation');}
const wl=donor.data.wavelengths;
const dyes=donor.data.channel_density.map(row=>row.map(x=>x===null?0:x));
const base=wl.map(w=>config.base.floor_density+config.base.green_absorption*gauss(w,config.base.center_nm,config.base.sigma_nm));
const weights=config.synthetic_densitometer.centers_nm.map((m,c)=>{const v=wl.map(w=>gauss(w,m,config.synthetic_densitometer.sigmas_nm[c])),sum=v.reduce((a,b)=>a+b,0);return v.map(x=>x/sum);});
function forward(amount){return weights.map(weight=>-Math.log10(weight.reduce((sum,w,i)=>sum+w*10**(-base[i]-dyes[i].reduce((s,d,c)=>s+d*amount[c],0)),0)));}
const baseline=forward([0,0,0]);
function solve3(a,b){const m=a.map((r,i)=>[...r,b[i]]);for(let k=0;k<3;k++){let pivot=k;for(let i=k+1;i<3;i++)if(Math.abs(m[i][k])>Math.abs(m[pivot][k]))pivot=i;[m[k],m[pivot]]=[m[pivot],m[k]];const div=m[k][k];if(Math.abs(div)<1e-12)throw Error('Singular surrogate inversion');for(let j=k;j<4;j++)m[k][j]/=div;for(let i=0;i<3;i++)if(i!==k){const f=m[i][k];for(let j=k;j<4;j++)m[i][j]-=f*m[k][j];}}return m.map(r=>r[3]);}
function lift(target){let a=[0,0,0];for(let it=0;it<24;it++){const pred=forward(a),res=target.map((v,c)=>v-pred[c]);if(Math.max(...res.map(Math.abs))<1e-8)break;const columns=a.map((_,c)=>{const b=[...a];b[c]+=1e-4;return forward(b).map((v,k)=>(v-pred[k])/1e-4);});const jac=[0,1,2].map(k=>columns.map(col=>col[k]));const step=solve3(jac,res);let best=a,bestError=res.reduce((s,x)=>s+x*x,0);for(const rate of [1,.5,.25,.1]){const next=a.map((x,c)=>Math.max(0,Math.min(8,x+rate*step[c]))),err=forward(next).reduce((s,x,c)=>s+(x-target[c])**2,0);if(err<bestError){best=next;bestError=err;}}if(best===a)break;a=best;}return {amount:a,residual:forward(a).map((v,c)=>v-target[c])};}
// Abramowitz-Stegun erf approximation; bounded monotone Gaussian CDF surrogate.
function cdf(z){const sign=z<0?-1:1,x=Math.abs(z)/Math.SQRT2,t=1/(1+.3275911*x);const erf=1-(((((1.061405429*t-1.453152027)*t)+1.421413741)*t-.284496736)*t+.254829592)*t*Math.exp(-x*x);return .5*(1+sign*erf);}
function fit(samples){
  let best;
  for(const shift of [-.3,0,.3]){
    let p=[-.65+shift,.1+shift,.8+shift,.5,.5,.5,.35,.45,.65];
    const loss=q=>samples.reduce((sum,[x,y])=>sum+(q.slice(0,3).reduce((v,m,k)=>v+q[3+k]*cdf((x-m)/q[6+k]),0)-y)**2,0)/samples.length;
    let score=loss(p);
    for(const step of [.3,.15,.07,.03,.01,.003])for(let pass=0;pass<24;pass++){
      let changed=false;
      for(let k=0;k<9;k++)for(const sign of [-1,1]){const q=[...p];q[k]+=step*sign;if(k>=3&&k<6&&q[k]<0||k>=6&&(q[k]<.08||q[k]>2.5)||k<3&&Math.abs(q[k])>4)continue;const err=loss(q);if(err<score){p=q;score=err;changed=true;}}
      if(!changed)break;
    }
    if(!best||score<best.score)best={p,score};
  }
  return {centers:best.p.slice(0,3),amplitudes:best.p.slice(3,6),sigmas:best.p.slice(6),rmse:Math.sqrt(best.score)};
}
const outDir=path.join(root,'engine/resources_experimental/profiles');fs.mkdirSync(outDir,{recursive:true});
const reports=[];
const filterDatabase=JSON.parse(fs.readFileSync(path.join(root,'engine/resources/neutral_print_filters.json')));
const calibrationPath=path.join(research,'phoenix-print-calibration.json');
const calibration=!process.argv.includes('--uncalibrated')&&fs.existsSync(calibrationPath)?JSON.parse(fs.readFileSync(calibrationPath)):null;
for(const second of [false,true]){
  const dir=path.join(research,second?'phoenix2':'phoenix200'),sensFile=path.join(dir,'spectral-chart.csv'),curveFile=path.join(dir,second?'characteristic-candidate.csv':'characteristic-chart.csv');
  const sens=csv(sensFile),curves=csv(curveFile),names=['red','green','blue'];
  const spectrum=names.map(c=>sens.filter(r=>r[c+'_chart_ordinate']!==null).map(r=>[r.wavelength_nm,r[c+'_chart_ordinate']]));
  const bb=(w,T)=>1/(w**5*Math.expm1(1.438776877e7/(w*T)));
  let sensitivity=wl.map(w=>spectrum.map(points=>w<points[0][0]||w>points.at(-1)[0]?10**config.sensitivity_outside_support_log10:10**interpolate(points,w)/(second?1:bb(w,2856)/bb(550,2856))));
  // Per-channel daylight normalization is a C-level white-balance assumption, not measured speed.
  const sums=[0,1,2].map(c=>sensitivity.reduce((sum,row,i)=>sum+row[c]*bb(wl[i],config.reference_blackbody_kelvin),0));
  const donorSums=[0,1,2].map(c=>donor.data.log_sensitivity.reduce((sum,row,i)=>sum+(row[c]===null?0:10**row[c])*bb(wl[i],config.reference_blackbody_kelvin),0));
  const logSensitivity=sensitivity.map(row=>row.map((v,c)=>Math.log10(v*donorSums[c]/sums[c])));
  const points=names.map(c=>curves.filter(r=>r[c+'_density']!==null).map(r=>[r.relative_log_exposure,r[c+'_density']]));
  // The unresolved first-generation R/B toe is explicitly extrapolated from a low-density proxy.
  for(let c=0;c<3;c++)if(points[c][0][0]>.1)points[c].unshift([0,.5]);
  const minima=points.map(p=>p[0][1]),samples=[],maxResidual=[0,0,0];
  for(let k=0;k<=120;k++){
    const relative=3*k/120,target=points.map((p,c)=>baseline[c]+Math.max(0,interpolate(p,relative)-minima[c]));
    const lifted=lift(target);samples.push({x:relative-config.relative_exposure_anchor,...lifted});
    lifted.residual.forEach((v,c)=>maxResidual[c]=Math.max(maxResidual[c],Math.abs(v)));
  }
  const models=[0,1,2].map(c=>fit(samples.map(row=>[row.x,row.amount[c]])));
  const fittedClosure=[0,0,0];
  samples.forEach(row=>{
    const amount=models.map(m=>m.centers.reduce((sum,center,l)=>sum+m.amplitudes[l]*cdf((row.x-center)/m.sigmas[l]),0));
    const prediction=forward(amount),target=forward(row.amount).map((v,c)=>v-row.residual[c]);
    prediction.forEach((v,c)=>fittedClosure[c]=Math.max(fittedClosure[c],Math.abs(v-target[c])));
  });
  const exposure=Array.from({length:256},(_,k)=>-3+6*k/255);
  const layers=exposure.map(x=>[0,1,2].map(l=>models.map(m=>m.amplitudes[l]*cdf((x-m.centers[l])/m.sigmas[l]))));
  const density=layers.map(row=>[0,1,2].map(c=>row.reduce((sum,l)=>sum+l[c],0)));
  const id=second?'harman_phoenix_ii_200_exp_v1':'harman_phoenix_200_exp_v1',name=second?'HARMAN Phoenix II 200':'HARMAN Phoenix 200';
  const effect=config[second?'phoenix_ii':'phoenix_200'];
  const profile={metadata:{version:config.material_version,quality:'实验近似 / Experimental approximation. 实拍还原度未验证 / photographic fidelity NOT validated.\nA: 官方感度图；一代官方特性曲线。\nB: 二代使用 JCH 候选特性曲线，其图下注为 Phoenix I，代际归属尚未核实。\nC: 感度刻度解释、曝光标定、Gold 染料代理、紫色底基、合成密度计、颗粒与光晕均含假设。',
    license:'Derivative of spektrafilm Kodak Gold 200 profile by Andrea Volpato, CC BY-SA 4.0. Changes: Phoenix chart-derived sensitivity/response, estimated unmasked base and effects. https://creativecommons.org/licenses/by-sa/4.0/ ; see PHOENIX_EXPERIMENTAL_NOTICE.md',
    source_url:second?'https://www.harmanphoto.co.uk/amfile/file/download/file/1968/product/2208/':'https://www.harmanphoto.co.uk/amfile/file/download/file/1963/product/2143/',
    datasource:`HARMAN charts digitized offline; Gold 200 dye proxy from spektrafilm (https://github.com/andreavolpato/spektrafilm). ${second?'II characteristic uses provisional JCH screenshot; caption says Phoenix I, generation unverified.':'Relative first-generation characteristic; unresolved R/B toe estimated.'} Gaussian synthetic densitometer, NOT Status M. Unmeasured purple base, exposure anchor, grain and halation. Evidence A/B/C in evidence.`,
    evidence:config.evidence,assumptions:config,upgrades:'Replace individual source CSV/model assumptions, regenerate a new version/stock ID; do not overwrite v1 recipes.',
    source_sha256:{spectral_csv:sha(sensFile),characteristic_csv:sha(curveFile),dye_proxy:sha(donorFile)},photographic_fidelity_validated:false},
    info:{stock:id,name:name+' · Experimental v1',type:'negative',support:'film',stage:'filming',use:'still',antihalation:'weak',target_print:'kodak_portra_endura',channel_model:'color',densitometer:'synthetic_gaussian_proxy',log_sensitivity_density_over_min:.2,reference_illuminant:'D55',viewing_illuminant:'D50',render_model:{version:1,...effect}},
    data:{wavelengths:wl,log_sensitivity:logSensitivity,channel_density:dyes,base_density:base,log_exposure:exposure,density_curves:density,density_curves_layers:layers,density_curves_model:{model_type:'norm_cdfs',centers:models.map(m=>m.centers),amplitudes:models.map(m=>m.amplitudes),sigmas:models.map(m=>m.sigmas)}}};
  const serialized=JSON.stringify(profile,null,2)+'\n';
  const profileHash=crypto.createHash('sha256').update(serialized).digest('hex');
  if(calibration){
    const entry=calibration.entries[id];
    if(!entry?.converged||entry.source_profile_sha256!==profileHash)throw Error('Stale print calibration: bootstrap with --uncalibrated, calibrate the regenerated profiles, then regenerate normally');
  }
  fs.writeFileSync(path.join(outDir,id+'.json'),serialized);
  for(const paper of Object.values(filterDatabase))for(const light of Object.values(paper))
    light[id]=config.neutral_print_filter_pack_CMY;
  if(calibration)filterDatabase[calibration.paper][calibration.illuminant][id]=calibration.entries[id].CMY;
  reports.push({stock:id,maximum_surrogate_density_closure_error:maxResidual,maximum_fitted_surrogate_density_error:fittedClosure,layer_curve_fit_rmse:models.map(m=>m.rmse),closure_is_only_to_assumed_instrument:true,photographic_fidelity_validated:false,samples});
}
fs.writeFileSync(path.join(research,'phoenix-model-report.json'),JSON.stringify({format:'SpektraLab.PhoenixApproximationReport',version:1,assumptions_sha256:sha(path.join(research,'phoenix-model-assumptions.json')),reports},null,2)+'\n');
fs.writeFileSync(path.join(outDir,'../neutral_print_filters.json'),JSON.stringify(filterDatabase,null,2)+'\n');
console.log(JSON.stringify(reports.map(({samples,...report})=>report),null,2));
