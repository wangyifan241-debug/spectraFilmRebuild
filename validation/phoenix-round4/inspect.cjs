'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const {deltaE00,xyzToLab}=require('../../tools/phoenix-validation/metrics.cjs');
const hash=b=>crypto.createHash('sha256').update(b).digest('hex');
function buffer(root,name){const b=fs.readFileSync(path.join(root,name+'.f32'));assert.equal(b.length%12,0);return Array.from({length:b.length/4},(_,i)=>b.readFloatLE(4*i));}
function inspect(root,linearCounterfactual=false){const report=JSON.parse(fs.readFileSync(path.join(root,'report.json'),'utf8'));let files=0;for(const [name,sha]of Object.entries(report.provenance.output_sha256)){assert.equal(hash(fs.readFileSync(path.join(root,name))),sha,'output tampering: '+name);files++;}
 const output={status:'SOFTWARE_COMPARISON_ONLY',real_film_metrics:null,integrity_files:files,models:{}};
 const input=buffer(root,'input-linear-prophoto'),width=report['input-linear-prophoto'].width,height=report['input-linear-prophoto'].height;
 for(const stock of ['harman_phoenix_200_exp_v1','harman_phoenix_ii_200_exp_v1']){
 const positive=buffer(root,stock+'-scan-linear-prophoto');assert.equal(input.length,positive.length);assert(positive.every(Number.isFinite));
 const scan=JSON.parse(fs.readFileSync(path.join(root,stock+'-scan-recipe.json'),'utf8'));const logT=buffer(root,stock+'-scan-logT');let endpoints=0;for(let i=0;i<logT.length;i++){let c=i%3;if(logT[i]<=scan.neutral_axis_logT[c]||logT[i]>=scan.neutral_axis_logT[scan.neutral_axis_logT.length-3+c])endpoints++;}
 const profileBytes=fs.readFileSync(path.join(report.provenance.resources_absolute,'profiles',stock+'.json'));assert.equal(hash(profileBytes),report.provenance.resource_sha256['profiles/'+stock+'.json']);const profile=JSON.parse(profileBytes).data,negative=buffer(root,stock+'-film_grain');let spectralMaxError=0,minTotalDensity=Infinity,maxTransmission=0;
 for(let j=0;j<64;j++){let pixel=Math.floor(j*(negative.length/3-1)/63),signal=[0,0,0];for(let w=0;w<81;w++){let dye=profile.channel_density[w];if(dye.some(x=>x===null)||profile.base_density[w]===null)continue;let d=0;for(let c=0;c<3;c++)d+=negative[3*pixel+c]*dye[c];let total=d+profile.base_density[w];minTotalDensity=Math.min(minTotalDensity,total);maxTransmission=Math.max(maxTransmission,10**(-total));let t=linearCounterfactual?Math.max(0,1-Math.LN10*d):10**(-d);for(let c=0;c<3;c++)signal[c]+=t*scan.normalized_detector_weights[3*w+c];}for(let c=0;c<3;c++)spectralMaxError=Math.max(spectralMaxError,Math.abs(Math.log10(signal[c]+1e-10)-logT[3*pixel+c]));}
 assert(spectralMaxError<3e-5,'Beer-Lambert spectral oracle mismatch: '+spectralMaxError);
 const result={spectral_oracle_max_logT_error:spectralMaxError,sampled_total_density_min:minTotalDensity,sampled_transmission_max:maxTransmission,endpoint_clamp_channel_fraction:endpoints/logT.length,negative_linear_channel_fraction:positive.filter(x=>x<0).length/positive.length,above_one_linear_channel_fraction:positive.filter(x=>x>1).length/positive.length};
 if(report.provenance.input?.startsWith('synthetic')){
 let maximumNeutralRelative=0;for(let x=10;x<width-10;x++){for(let c=0;c<3;c++){let i=(12*width+x)*3+c;maximumNeutralRelative=Math.max(maximumNeutralRelative,Math.abs(positive[i]/input[i]-1));}}
 result.neutral_max_relative_error=maximumNeutralRelative;assert(maximumNeutralRelative<.015,'neutral reversal regression');
 const xyz=rgb=>[0,1,2].map(c=>[0,1,2].reduce((sum,j)=>sum+scan.linear_prophoto_to_XYZ_D50[3*c+j]*rgb[j],0));
 const rows=[];for(let band=0;band<4;band++)for(let ev of [-4,-2,0]){let x=Math.round((ev+6)/12*(width-1)),y=band*24+12,i=(y*width+x)*3;let ref=input.slice(i,i+3),model=positive.slice(i,i+3),a=xyzToLab(xyz(ref)),b=xyzToLab(xyz(model));rows.push({band,ev,delta_e00:deltaE00(a,b),delta_L:b[0]-a[0],reference_Y:xyz(ref)[1],model_Y:xyz(model)[1],delta_chroma:Math.hypot(b[1],b[2])-Math.hypot(a[1],a[2]),reference_hue_degrees:Math.atan2(a[2],a[1])*180/Math.PI,model_hue_degrees:Math.atan2(b[2],b[1])*180/Math.PI});}result.synthetic_D50_Lab_patches=rows;
 }
 output.models[stock]=result;
 }
 return output;}
module.exports={inspect};if(require.main===module){console.log(JSON.stringify(inspect(process.argv[2],process.argv.includes("--linear-counterfactual")),null,2));}
