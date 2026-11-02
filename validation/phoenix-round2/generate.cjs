'use strict';
// Run from repository root. Read parents from frozen Git objects, never mutable checkout bytes.
const fs=require('fs'),cp=require('child_process'),path=require('path'),crypto=require('crypto');
const root=process.cwd(),out=path.join(root,'validation/phoenix-round2');
const sha=b=>crypto.createHash('sha256').update(b).digest('hex');
const base='ba076b2befe6586ad3095f897ebf40ea7e9ffd6c',infra='00405ee8ad5a448b09e64b1bf9195f1bb429f9a9';
const read=p=>cp.execFileSync('git',['show',base+':'+p]);
const donor=JSON.parse(read('engine/resources/profiles/kodak_gold_200.json'));
const bb=(w,T)=>1/(w**5*Math.expm1(1.438776877e7/(w*T)));
const interp=(p,w)=>{for(let i=1;i<p.length;i++)if(w<=p[i][0])return p[i-1][1]+(p[i][1]-p[i-1][1])*(w-p[i-1][0])/(p[i][0]-p[i-1][0]);return p.at(-1)[1]};
const report={base,infra,scope:'Material-array sensitivity only; not native render validation',candidates:[]};
fs.mkdirSync(path.join(out,'profiles'),{recursive:true});fs.mkdirSync(path.join(out,'parents'),{recursive:true});
for(const [tag,dir] of [['200','phoenix200'],['ii','phoenix2']]){
 const parent='harman_phoenix_'+(tag==='ii'?'ii_':'')+'200_exp_v1',p='engine/resources_experimental/profiles/'+parent+'.json';
 const bytes=read(p),before=JSON.parse(bytes);fs.writeFileSync(path.join(out,'parents',parent+'.json'),bytes);
 const csvpath='engine/resources_research/'+dir+'/spectral-chart.csv',csv=read(csvpath).toString().trim().split(/\r?\n/),keys=csv.shift().split(',');
 const rows=csv.map(l=>Object.fromEntries(l.split(',').map((v,i)=>[keys[i],v===''?null:Number(v)])));
 const points=['red','green','blue'].map(c=>rows.filter(r=>r[c+'_chart_ordinate']!==null).map(r=>[r.wavelength_nm,r[c+'_chart_ordinate']]));
 const wl=before.data.wavelengths;
 for(const mode of ['linear','shared',...(tag==='200'?['no_tungsten']:[])]){
  const a=structuredClone(before),id='phx_'+tag+'_'+mode+'_r2';
  const raw=wl.map(w=>points.map(p=>w<p[0][0]||w>p.at(-1)[0]?1e-8:(mode==='linear'?Math.max(1e-8,interp(p,w)):10**interp(p,w))/(tag==='200'&&mode!=='no_tungsten'?bb(w,2856)/bb(550,2856):1)));
  const sums=[0,1,2].map(c=>raw.reduce((s,r,i)=>s+r[c]*bb(wl[i],5500),0));
  const targets=[0,1,2].map(c=>donor.data.log_sensitivity.reduce((s,r,i)=>s+(r[c]===null?0:10**r[c])*bb(wl[i],5500),0));
  const common=targets.reduce((s,x)=>s+x,0)/sums.reduce((s,x)=>s+x,0);
  a.data.log_sensitivity=raw.map(r=>r.map((v,c)=>Math.log10(v*(mode==='shared'?common:targets[c]/sums[c]))));
  a.info.stock=id;a.info.name+=' / R2 '+mode;a.metadata.version='0.2.0';a.metadata.parent_material_sha256=sha(bytes);a.metadata.photographic_fidelity_validated=false;
  a.metadata.round2={mode,evidence:'C',generation:tag==='ii'?'Unverified Generation':'Phoenix I',infra_commit:infra,source_record:'validation/phoenix-round2/RESEARCH_DESIGN.md',frozen_print:true};
  const file='validation/phoenix-round2/profiles/'+id+'.json';fs.writeFileSync(file,JSON.stringify(a,null,2)+'\n');
  const plan={format:'SpektraLab.PhoenixCandidate',version:1,id,parent_commit:base,renderer_commit:base,comparison_mode:'frozen_parent_print',hypothesis:{category:'spectral_response',rationale:mode,evidence_level:'C',source_record:a.metadata.round2.source_record},profiles:[{parent_stock:parent,parent_path:'validation/phoenix-round2/parents/'+parent+'.json',file:{path:file,sha256:sha(fs.readFileSync(file))}}]};
  const lock=JSON.parse(fs.readFileSync('validation/phoenix-accuracy/baseline-lock.v1.json'));
  const validation=require('../../tools/phoenix-validation/candidates.cjs').validateCandidate(root,plan,lock);
  fs.writeFileSync(path.join(out,id+'.json'),JSON.stringify(plan,null,2)+'\n');
  const delta=a.data.log_sensitivity.flatMap((r,i)=>r.map((v,c)=>v-before.data.log_sensitivity[i][c]));
  report.candidates.push({id,validation,max_absolute_log10_sensitivity_change:Math.max(...delta.map(Math.abs)),finite:a.data.log_sensitivity.flat().every(Number.isFinite),decision:'Inconclusive',native_tests:'Not Run',photographic_fidelity_validated:false});
 }
}
fs.writeFileSync(path.join(out,'material-sensitivity.json'),JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify(report,null,2));
