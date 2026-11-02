// Research gate; never asserts correspondence to actual photographed film.
const fs=require('fs'),path=require('path'),crypto=require('crypto'),assert=require('assert');
const root=path.resolve(__dirname,'../..');
const report=JSON.parse(fs.readFileSync(path.join(__dirname,'phoenix-model-report.json')));
const originalFilters=JSON.parse(fs.readFileSync(path.join(root,'engine/resources/neutral_print_filters.json')));
const extendedFilters=JSON.parse(fs.readFileSync(path.join(root,'engine/resources_experimental/neutral_print_filters.json')));
const calibration=JSON.parse(fs.readFileSync(path.join(__dirname,'phoenix-print-calibration.json')));
let checks=0;
function check(ok,message){checks++;assert(ok,message);}
for(const [paper,lights] of Object.entries(originalFilters))for(const [light,stocks] of Object.entries(lights))for(const [stock,pack] of Object.entries(stocks))
  check(JSON.stringify(extendedFilters[paper][light][stock])===JSON.stringify(pack),'Existing print calibration changed');
for(const r of report.reports){
  const p=JSON.parse(fs.readFileSync(path.join(root,'engine/resources_experimental/profiles',r.stock+'.json')));
  if(process.argv.includes('--counterfactual'))p.data.log_sensitivity[0][0]=NaN;
  check(p.metadata.photographic_fidelity_validated===false,'Unsupported photographic validation claim');
  check(p.metadata.version==='0.1.0'&&p.info.stock===r.stock,'Version/identity mismatch');
  const materialHash=crypto.createHash('sha256').update(fs.readFileSync(path.join(root,'engine/resources_experimental/profiles',r.stock+'.json'))).digest('hex');
  check(calibration.entries[r.stock].source_profile_sha256===materialHash&&calibration.entries[r.stock].converged,'Stale/failed model print calibration');
  check(calibration.entries[r.stock].log_rgb_ratio_residual.every(v=>Math.abs(v)<.003),'Model print midgray not balanced');
  check(JSON.stringify(extendedFilters[calibration.paper][calibration.illuminant][r.stock])===JSON.stringify(calibration.entries[r.stock].CMY),'Calibrated print pack not installed');
  for(const key of ['log_sensitivity','channel_density','base_density','log_exposure','density_curves','density_curves_layers']){
    const values=p.data[key].flat(3);check(values.length>0&&values.every(Number.isFinite),'Nonfinite '+key);
  }
  check(p.data.wavelengths.length===81&&p.data.wavelengths.every((v,i)=>v===380+5*i),'Wavelength grid mismatch');
  for(let c=0;c<3;c++)for(let i=1;i<256;i++)check(p.data.density_curves[i][c]>=p.data.density_curves[i-1][c],'Nonmonotonic response');
  const coupling=[[.336,.353,.302],[.154,.319,.353],[.168,.226,.273]],scale=p.info.render_model.dir_couplers_scale;
  for(let c=0;c<3;c++)for(let i=1;i<256;i++){
    const before=k=>p.data.log_exposure[k]-p.data.density_curves[k].reduce((sum,d,ch)=>sum+d*coupling[ch][c]*scale,0);
    check(before(i)>before(i-1),'DIR-coupler exposure axis folds');
  }
  check(r.maximum_surrogate_density_closure_error.every(v=>v<.02),'Surrogate inversion failed');
  check(r.maximum_fitted_surrogate_density_error.every(v=>v<.08),'Fitted response closure failed');
  check(r.layer_curve_fit_rmse.every(v=>v<.03),'Curve approximation failed');
  const second=r.stock.includes('_ii_'),directory=path.join(__dirname,second?'phoenix2':'phoenix200');
  for(const [key,file] of [['spectral_csv','spectral-chart.csv'],['characteristic_csv',second?'characteristic-candidate.csv':'characteristic-chart.csv']]){
    const hash=crypto.createHash('sha256').update(fs.readFileSync(path.join(directory,file))).digest('hex');
    check(hash===p.metadata.source_sha256[key],'Source CSV changed without regeneration');
  }
}
console.log(`${checks} research checks passed. SOFTWARE/MODEL SELF-CONSISTENCY ONLY; photographic fidelity unvalidated.`);
