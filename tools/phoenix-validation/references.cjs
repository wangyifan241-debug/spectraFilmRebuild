'use strict';
const {requireThat,text,digest,binding,json,notVerified,hash}=require('./common.cjs');
const {pairedMetrics,grainStatistics,halationStatistics}=require('./metrics.cjs');
const identities=['negative_id','roll_id','scene_id','raw_capture_id'];
function manifest(document){
 requireThat(document?.format==='SpektraLab.PhoenixReferences'&&document.version===1,'Unsupported reference manifest');
 requireThat(text(document.dataset_id)&&text(document.split_policy?.seed)&&document.split_policy.version===1,'Dataset/split identity missing');
 requireThat(['physical','synthetic_engineering'].includes(document.dataset_kind)&&Number.isInteger(document.dataset_version)&&document.dataset_version>0,'Dataset kind/version missing');
 requireThat(Array.isArray(document.samples)&&document.samples.length<=10000,'Invalid sample list');const ids=new Set();
 for(const s of document.samples){requireThat(text(s.id)&&!ids.has(s.id),'Duplicate/missing sample ID');ids.add(s.id);requireThat(['phoenix_200','phoenix_ii'].includes(s.stock),'Unknown film generation');requireThat(s.identity&&identities.every(k=>text(s.identity[k])),'Physical grouping identity missing');requireThat(!s.split||['train','validation','test'].includes(s.split),'Unknown split');}
 return document;
}
function splits(document){
 manifest(document);const samples=document.samples,parent=samples.map((_,i)=>i),seen=new Map();
 function find(x){while(parent[x]!==x){parent[x]=parent[parent[x]];x=parent[x];}return x;}
 function join(a,b){a=find(a);b=find(b);if(a!==b)parent[b]=a;}
 samples.forEach((s,i)=>{const keys=identities.map(k=>k+':'+s.identity[k]);for(const kind of ['raw','scan'])if(digest(s.files?.[kind]?.sha256))keys.push('content:'+s.files[kind].sha256);for(const key of keys){if(seen.has(key))join(i,seen.get(key));else seen.set(key,i);}});
 const groups=new Map();samples.forEach((s,i)=>{const k=find(i);if(!groups.has(k))groups.set(k,[]);groups.get(k).push(s);});
 const result=[];for(const group of groups.values()){
  const declarations=new Set(group.map(s=>s.split).filter(Boolean));requireThat(declarations.size<=1,'Data leakage: related negatives/rolls/scenes/scans declare different splits');
  // Group ID depends on all physical identities, never scan filename or sample order.
  const key=[...new Set(group.flatMap(s=>identities.map(k=>k+':'+s.identity[k])))].sort().join('\n');
  const groupId=hash(document.dataset_id+'\n'+document.split_policy.seed+'\n'+key),n=parseInt(groupId.slice(0,8),16)/0x100000000;
  const split=[...declarations][0]??(n<.7?'train':n<.85?'validation':'test');
  for(const s of group)result.push({sample_id:s.id,group_id:groupId,split});
 }
 return result.sort((a,b)=>a.sample_id.localeCompare(b.sample_id));
}
function prerequisites(sample,root,modelId){
 if(sample.origin_kind!=='physical_film')return notVerified('Not independently sourced physical film');
 const required=[['provenance.source_uri',sample.provenance?.source_uri],['provenance.author',sample.provenance?.author],['license.id',sample.provenance?.license?.id],['license.url_or_permission',sample.provenance?.license?.url_or_permission],['film.product',sample.film?.product],['film.format',sample.film?.format],['pairing.reviewer',sample.pairing?.reviewer],['pairing.method',sample.pairing?.method],['exposure.description',sample.exposure?.description],['process.description',sample.process?.description],['scan.device',sample.scan?.device],['scan.processing',sample.scan?.processing],['scan.icc_transform',sample.scan?.icc_transform]];
 for(const [field,value] of required)if(!text(value))return notVerified('Unknown reference condition: '+field);
 if(sample.pairing?.status!=='verified')return notVerified('Pairing not independently reviewed');
 const files={};for(const kind of ['raw','scan','icc','reference_observations','pairing_evidence']){const b=binding(root,sample.files?.[kind]);if(!b.ok)return notVerified(b.reason);files[kind]=b.file;}
 const pairing=require('./evidence.cjs').pairingEvidence(sample,root);if(pairing.status!=='Evidence checked')return pairing;
 const output=sample.model_outputs?.[modelId];if(!output)return notVerified('No paired output for model '+modelId);
 if(output.input_sha256!==sample.files.raw.sha256||!digest(output.recipe_sha256)||!text(output.renderer_commit))return notVerified('Model output input/recipe/renderer binding missing');
 for(const kind of ['render','observations']){const b=binding(root,output[kind]);if(!b.ok)return notVerified(b.reason);files['model_'+kind]=b.file;}
 return {status:'Ready',files,output,pairing};
}
function evaluateSample(sample,root,model){
 const ready=prerequisites(sample,root,model.id);if(ready.status!=='Ready')return {id:sample.id,...ready};
 const parent=sample.stock==='phoenix_200'?'harman_phoenix_200_exp_v1':'harman_phoenix_ii_200_exp_v1';
 const material=model.materials?.find(p=>(p.parent_stock??p.stock)===parent);
 if(!material||ready.output.material_sha256!==material.sha256||ready.output.stock!==material.stock)return {id:sample.id,...notVerified('Paired output material/film generation binding missing')};
 if(ready.output.renderer_commit!==model.renderer_commit||ready.output.recipe_sha256!==model.recipe_sha256)return {id:sample.id,...notVerified('Output renderer/recipe differs from versioned model run')};
 try{
  const reference=json(ready.files.reference_observations),output=json(ready.files.model_observations);
  if(reference.origin_kind!=='physical_film'||output.origin_kind!=='model_render')return {id:sample.id,...notVerified('Synthetic/unclassified observations cannot verify photography')};
  if(reference.sample_id!==sample.id||output.sample_id!==sample.id||reference.source_image_sha256!==sample.files.scan.sha256||output.source_image_sha256!==ready.output.render.sha256)return {id:sample.id,...notVerified('Observation source identity mismatch')};
  if(!text(reference.extraction_method)||!text(output.extraction_method)||reference.colorimetry?.icc_sha256!==sample.files.icc.sha256||!digest(output.colorimetry?.icc_sha256)||!text(sample.scan.observation_reviewed_by))return {id:sample.id,...notVerified('Missing audited observation extraction/ICC binding')};
  const expected={sample_id:sample.id,source_sha256:sample.files.raw.sha256};
  const referenceROI=require('./evidence.cjs').observationRegistration(root,reference,sample.files.reference_registration,{...expected,target_sha256:sample.files.scan.sha256});
  const modelROI=require('./evidence.cjs').observationRegistration(root,output,ready.output.registration,{...expected,target_sha256:ready.output.render.sha256});
  if(referenceROI.status!=='Geometry checked'||modelROI.status!=='Geometry checked')return {id:sample.id,...notVerified(referenceROI.reason??modelROI.reason),evidence:{pairing:ready.pairing,reference_registration:referenceROI,model_registration:modelROI}};
  for(const roi of referenceROI.regions){const modelRegion=modelROI.regions.find(r=>r.id===roi.id);if(!modelRegion||JSON.stringify(roi.source_rect)!==JSON.stringify(modelRegion.source_rect))return {id:sample.id,...notVerified('Reference/model ROIs do not identify the same RAW scene region')};}
  const metrics=pairedMetrics(reference,output);let grain=notVerified('No high-resolution paired grain ROI'),halation=notVerified('No high-resolution paired halation ROI');
  try{for(const [packet,audit] of [[reference,referenceROI],[output,modelROI]]){const spatial=packet.spatial?.grain;if(spatial){const roi=audit.regions.find(r=>r.id===spatial.registration_roi_id);requireThat(roi&&roi.target_rect[2]===spatial.width&&roi.target_rect[3]===spatial.height,'Spatial grain ROI registration missing or dimensions differ');}}const a=grainStatistics(reference),b=grainStatistics(output);if(a&&b){requireThat(a.pixel_pitch_um===b.pixel_pitch_um,'Grain scale mismatch');grain={status:'Measured',reference:a,model:b,rms_Y_error:b.rms_Y-a.rms_Y};}}catch(e){grain=notVerified(e.message);}
  try{for(const [packet,audit] of [[reference,referenceROI],[output,modelROI]]){const spatial=packet.spatial?.halation;if(spatial){const roi=audit.regions.find(r=>r.id===spatial.registration_roi_id);requireThat(roi&&roi.target_rect[2]===spatial.width&&roi.target_rect[3]===spatial.height,'Spatial halation ROI registration missing or dimensions differ');}}const a=halationStatistics(reference),b=halationStatistics(output);if(a&&b){requireThat(a.pixel_pitch_um===b.pixel_pitch_um,'Halo scale mismatch');halation={status:'Measured',reference:a,model:b,energy_ratio_error:b.halo_to_core_energy-a.halo_to_core_energy};}}catch(e){halation=notVerified(e.message);}
  return {id:sample.id,status:'Measured',metrics,grain,halation,evidence:{pairing:ready.pairing,reference_registration:referenceROI,model_registration:modelROI},scope:'Conditional paired observation metrics; correspondence reviews are not an independently authenticated physical-accuracy certificate'};
 }catch(e){return {id:sample.id,...notVerified('Invalid paired observations: '+e.message)};}
}
module.exports={manifest,splits,prerequisites,evaluateSample};
