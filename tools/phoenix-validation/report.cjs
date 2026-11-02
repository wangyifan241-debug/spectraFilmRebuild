'use strict';
const cp=require('node:child_process');
const {fs,path,requireThat,json,binding,notVerified,writeJson,escapeHtml:esc,fileHash,BASE}=require('./common.cjs');
const {verifyLock}=require('./baseline.cjs');const {manifest,splits,evaluateSample}=require('./references.cjs');const {compare}=require('./png.cjs');
function physicalGate(document,rows,assignments,policy,split){
 if(document.dataset_kind!=='physical')return notVerified('No physical reference dataset; synthetic data is engineering-only');
 if(rows.some(r=>document.samples.find(s=>s.id===r.id)?.origin_kind!=='physical_film'))return notVerified('Synthetic/unclassified sample cannot satisfy photographic gate');
 if(policy.status!=='reviewed')return notVerified('Photographic thresholds are provisional/unreviewed');
 requireThat(typeof policy.reviewed_by==='string'&&policy.reviewed_by.trim(),'Reviewed policy needs reviewer');
 for(const k of ['minimum_groups_per_stock','delta_e00_mean','delta_e00_p95','rmse_Y','gray_rmse_L','grain_rms_absolute_Y','halo_ratio_absolute'])requireThat(Number.isFinite(policy[k])&&policy[k]>0,'Missing/invalid acceptance threshold: '+k);
 if(!rows.length||rows.some(r=>r.status!=='Measured'))return notVerified('Missing complete independently paired observations');
 const selected=new Map(assignments.map(a=>[a.sample_id,a]));
 for(const stock of ['phoenix_200','phoenix_ii']){const ids=rows.filter(r=>document.samples.find(s=>s.id===r.id).stock===stock);const groups=new Set(ids.map(r=>selected.get(r.id).group_id));if(groups.size<policy.minimum_groups_per_stock)return notVerified('Insufficient independent '+stock+' groups in '+split);}
 if(rows.some(r=>!r.metrics.gray_response||r.grain.status!=='Measured'||r.halation.status!=='Measured'))return notVerified('Full photographic gate requires gray, grain and halo evidence');
 if(rows.some(r=>![r.metrics.delta_e00.mean,r.metrics.delta_e00.p95,r.metrics.luminance.rmse_Y,r.metrics.gray_response.rmse_L,r.grain.rms_Y_error,r.halation.energy_ratio_error].every(Number.isFinite)))return notVerified('Nonfinite photographic metric');
 const failed=[];for(const r of rows){const m=r.metrics;if(m.delta_e00.mean>policy.delta_e00_mean||m.delta_e00.p95>policy.delta_e00_p95||m.luminance.rmse_Y>policy.rmse_Y||m.gray_response.rmse_L>policy.gray_rmse_L)failed.push(r.id);if(Math.abs(r.grain.rms_Y_error)>policy.grain_rms_absolute_Y||Math.abs(r.halation.energy_ratio_error)>policy.halo_ratio_absolute)failed.push(r.id+':spatial');}
 return {status:failed.length?'Fail':'Pass',scope:'independent paired photographic observations under declared scan conditions',failed_samples:failed,policy_version:policy.version};
}
function engineering(root,lock,baselineCheck){
 const script=path.join(root,'engine/resources_research/verify-phoenix.cjs');const run=cp.spawnSync(process.execPath,[script],{encoding:'utf8',timeout:30000,maxBuffer:1024*1024});
 const log=path.join(root,'validation/phoenix-review-20261102/ctest-details.txt'),published=fs.existsSync(log)?fs.readFileSync(log,'utf8'):'';
 const native=['spk_qt_phoenix','spk_qt_frame_output','spk_qt_material_workflow'].every(name=>published.includes('Testing: '+name))&&(published.match(/Test Passed\./g)||[]).length===7;
 return {status:baselineCheck.status==='Pass'&&run.status===0&&native?'Pass':'Fail',scope:'current frozen-data gate plus pinned historical PR4 native receipts; not a fresh full native run',research_gate:{exit_code:run.status,stdout:run.stdout,stderr:run.stderr},published_native_receipt:{sha256:fs.existsSync(log)?fileHash(log):null,historical:true,coverage:native}};
}
function generate(root,options){
 const lock=json(options.lock),references=manifest(json(options.references)),policy=json(options.policy),models=json(options.models);requireThat(policy.format==='SpektraLab.PhoenixAcceptance'&&policy.version===1,'Unknown acceptance policy');
 requireThat(models.format==='SpektraLab.PhoenixModels'&&models.version===1&&Array.isArray(models.models),'Unknown model index');
 requireThat(models.models.length>0&&models.models[0].id==='experimental-v1','First model must be frozen Experimental v1');
 requireThat(policy.photographic&&Object.values(policy.photographic.thresholds??{}).every(Number.isFinite),'Invalid photographic policy');
 const baseline=verifyLock(root,lock),assignment=splits(references),split=options.split??'validation';requireThat(['train','validation','test'].includes(split),'Unknown evaluation split');
 requireThat(split!=='test'||options.purpose==='final_evaluation','Held-out test is only available for explicit final_evaluation');
 const recipe=binding(root,models.recipe);requireThat(recipe.ok,recipe.reason);const ids=new Set();
 for(const model of models.models){requireThat(!ids.has(model.id),'Duplicate model ID');ids.add(model.id);requireThat(model.renderer_commit===BASE&&model.recipe_sha256===models.recipe.sha256,'Renderer/recipe changes invalidate model-only comparisons');
  requireThat(Array.isArray(model.materials)&&new Set(model.materials.map(p=>p.stock)).size===model.materials.length,'Duplicate material identity');
  if(model.id==='experimental-v1')requireThat(model.materials.length===lock.profiles.length&&model.materials.every(p=>lock.profiles.some(b=>b.sha256===p.sha256)),'Baseline model materials differ');
  else {const bound=binding(root,model.candidate_plan);requireThat(bound.ok,bound.reason);const plan=json(bound.file);require('./candidates.cjs').validateCandidate(root,plan,lock);requireThat(plan.id===model.id&&plan.recipe.sha256===models.recipe.sha256,'Candidate plan identity/recipe mismatch');requireThat(model.materials.length===plan.profiles.length&&model.materials.every(p=>plan.profiles.some(e=>e.file.sha256===p.sha256)),'Model differs from candidate plan');}
  for(const p of model.materials){const file=binding(root,p);requireThat(file.ok,file.reason);const value=json(file.file);requireThat(value.info.stock===p.stock&&value.metadata.version===p.version,'Model material version mismatch');}}
 const selected=references.samples.filter(s=>assignment.find(a=>a.sample_id===s.id).split===split),dataRoot=path.resolve(path.dirname(options.references),references.data_root??'.');
 const results=models.models.map(model=>{
  const rows=references.dataset_kind==='physical'?selected.map(s=>evaluateSample(s,dataRoot,model)):selected.map(s=>({id:s.id,...notVerified('Synthetic engineering dataset cannot produce photographic fidelity metrics')}));
  const fixed=[];for(const item of model.fixed_renders??[]){
   const reference=lock.renderings.find(f=>f.path.endsWith('/'+item.parent_stock+'.png'));if(!reference){fixed.push(notVerified('Unknown frozen parent rendering'));continue;}
   const originalInput=lock.renderings.find(f=>f.path.endsWith('/input.png'));requireThat(item.input_sha256===originalInput.sha256,'Fixed render input differs');const bound=binding(root,item.file);if(!bound.ok){fixed.push(notVerified(bound.reason));continue;}
   try{fixed.push({id:item.parent_stock,status:'Compared',file:item.file,...compare(fs.readFileSync(insideRoot(root,reference.path)),fs.readFileSync(bound.file))});}catch(e){fixed.push(notVerified(e.message));}
  }
  return {id:model.id,label:model.label,material_versions:model.materials,engineering_gate:model.id==='experimental-v1'?null:require('./receipts.cjs').nativeReceipt(root,model),photographic_gate:baseline.status==='Pass'?physicalGate(references,rows,assignment,policy.photographic,split):notVerified('Frozen baseline integrity failed'),samples:rows,fixed_input_render_comparisons:fixed};
 });
 const eng=engineering(root,lock,baseline);for(const r of results)if(r.id==='experimental-v1')r.engineering_gate=eng;
 const versionPairs=[];if(results.length>1)for(const next of results.slice(1)){
  const parent=results[0],paired=[];for(const r of next.samples){const previous=parent.samples.find(x=>x.id===r.id);if(previous?.status==='Measured'&&r.status==='Measured')paired.push({sample_id:r.id,delta_mean_e00:r.metrics.delta_e00.mean-previous.metrics.delta_e00.mean,delta_rmse_Y:r.metrics.luminance.rmse_Y-previous.metrics.luminance.rmse_Y});}
  versionPairs.push({parent:parent.id,candidate:next.id,eligible_for_selection:split==='validation'&&paired.length>0,paired_sample_deltas:paired,status:paired.length?'Compared':'Not Verified',note:'Negative delta means lower error on the same paired sample; not cross-dataset ranking'});
 }
 const report={format:'SpektraLab.PhoenixValidationReport',version:1,run_id:options.runId,baseline_commit:BASE,lock_sha256:fileHash(options.lock),dataset:{id:references.dataset_id,version:references.dataset_version,manifest_sha256:fileHash(options.references),kind:references.dataset_kind,split,assignments:assignment},recipe_sha256:models.recipe.sha256,acceptance_policy_sha256:fileHash(options.policy),baseline_integrity:baseline,models:results,comparisons:versionPairs,limits:['Software gates and surrogate/internal error are not photographic fidelity','Missing real reference files/conditions => Not Verified, never a synthetic score','Test split cannot be used for candidate selection','ROI scan statistics include scanning/optics and are not standardized film RMS or isolated PSF']};
 report.execution={node_version:process.version,model_index_sha256:fileHash(options.models),tool_inventory:fs.readdirSync(__dirname).filter(f=>f.endsWith('.cjs')).sort().map(f=>({path:'tools/phoenix-validation/'+f,sha256:fileHash(path.join(__dirname,f))}))};
 requireThat(/^[A-Za-z0-9._-]+$/.test(options.runId),'Unsafe report run ID');const directory=path.join(options.out,options.runId);requireThat(!fs.existsSync(directory),'Versioned report already exists; choose a new run ID');fs.mkdirSync(directory,{recursive:true});
 writeJson(path.join(directory,'report.json'),report);
 const cards=results.map(r=>`<section><h2>${esc(r.label??r.id)}</h2><p>Engineering: <strong>${esc(r.engineering_gate.status)}</strong> · Photographic: <strong>${esc(r.photographic_gate.status)}</strong></p><p>${esc(r.photographic_gate.reason??r.photographic_gate.scope)}</p><h3>Fixed inputs — engineering only</h3>${r.fixed_input_render_comparisons.map(f=>f.file?`<figure><img width="384" alt="${esc(f.id)} engineering fixture" src="data:image/png;base64,${fs.readFileSync(binding(root,f.file).file).toString('base64')}"><figcaption>${esc(f.id)} · identical bytes: ${esc(f.identical_bytes)} · encoded RGB MAE: ${esc(f.mae_encoded_RGB)}</figcaption></figure>`:`<p>${esc(f.reason)}</p>`).join('')}<details><summary>Paired observations and scan statistics</summary><pre>${esc(JSON.stringify(r.samples,null,2))}</pre></details></section>`).join('');
 fs.writeFileSync(path.join(directory,'index.html'),`<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>Phoenix validation ${esc(options.runId)}</title><style>body{font:16px system-ui;max-width:1100px;margin:40px auto;padding:0 20px;background:#f5f4ef;color:#222}section{background:white;padding:24px;margin:24px 0;border:1px solid #ccc}img{max-width:100%;image-rendering:auto}pre{white-space:pre-wrap;overflow-wrap:anywhere}strong{padding:3px 8px;background:#eee}figcaption{font-size:13px}</style><h1>Phoenix validation · ${esc(options.runId)}</h1><p>Pinned PR #4: <code>${BASE}</code></p><p>Dataset: ${esc(references.dataset_id)} · ${esc(split)} · ${selected.length} samples. Missing references are <b>Not Verified</b>, not zero error.</p><p>This report evaluates software evidence separately from real-film fidelity. Synthetic fixed inputs below are engineering fixtures only.</p>${cards}<h2>Version comparisons</h2><pre>${esc(JSON.stringify(versionPairs,null,2))}</pre><h2>Acceptance and limitations</h2><pre>${esc(JSON.stringify(report.limits,null,2))}</pre><p><a href="report.json">Versioned JSON report</a></p></html>`,{flag:'wx'});
 return {directory,report};
}
function insideRoot(root,file){return require('./common.cjs').inside(root,file);}
module.exports={generate,physicalGate};
