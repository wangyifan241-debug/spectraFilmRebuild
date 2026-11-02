'use strict';
const {requireThat,hash,text,binding,notVerified}=require('./common.cjs');const {decode}=require('./png.cjs');const {xyzToLab,validateObservations}=require('./metrics.cjs');
const linear=v=>v<=.04045?v/12.92:((v+.055)/1.055)**2.4;
// sRGB D65 -> Bradford D50 XYZ. No auto exposure, fit, inversion, registration or tone matching.
function xyz(rgb){const [r,g,b]=rgb.map(linear);return [.4360747*r+.3850649*g+.1430804*b,.2225045*r+.7168786*g+.0606169*b,.0139322*r+.0971045*g+.7141733*b];}
function extract(bytes,spec,context={}){
 const im=decode(bytes);requireThat(im.opaque,'Transparent scan rejected');requireThat(text(spec.sample_id)&&['physical_film','model_render','synthetic_engineering'].includes(spec.origin_kind),'Observation identity missing');
 requireThat(spec.color_model==='sRGB_IEC61966_2_1'&&text(spec.icc_interpretation_reviewed_by),'An explicit reviewed sRGB color model is required');
 requireThat(im.icc_sha256&&im.icc_sha256===spec.icc_sha256,'Embedded ICC missing or does not match extraction specification');
 let registration=null;
 if(spec.origin_kind!=='synthetic_engineering'){
  if(!context.root)return notVerified('ROI registration evidence root unavailable');
  const raw=binding(context.root,spec.raw_source);if(!raw.ok)return notVerified(raw.reason);
  registration=require('./evidence.cjs').registrationEvidence(context.root,spec.registration,{sample_id:spec.sample_id,source_sha256:spec.raw_source.sha256,target_sha256:hash(bytes)});
  if(registration.status!=='Geometry checked')return registration;
  if(registration.target_size[0]!==im.width||registration.target_size[1]!==im.height)return notVerified('Registration dimensions differ from actual image');
  for(const roi of spec.regions??[]){const record=registration.regions.find(r=>r.id===roi.id);if(!record||JSON.stringify(record.target_rect)!==JSON.stringify(roi.rect))return notVerified('ROI extraction coordinates differ from reviewed registration');}
  for(const [kind,roi] of Object.entries(spec.spatial??{})){const record=registration.regions.find(r=>r.id===roi.registration_roi_id);if(!record||JSON.stringify(record.target_rect)!==JSON.stringify(roi.rect))return notVerified('Spatial '+kind+' ROI lacks reviewed registration coordinates');}
 }
 requireThat(Array.isArray(spec.regions)&&spec.regions.length>0,'No independently registered ROIs');const patches=[];
 for(const region of spec.regions){const [x,y,w,h]=region.rect??[];requireThat([x,y,w,h].every(Number.isInteger)&&x>=0&&y>=0&&w>0&&h>0&&x+w<=im.width&&y+h<=im.height,'Invalid patch ROI');const total=[0,0,0];for(let py=y;py<y+h;py++)for(let px=x;px<x+w;px++){const i=(py*im.width+px)*3,value=xyz([...im.pixels.subarray(i,i+3)]);value.forEach((v,c)=>total[c]+=v);}const average=total.map(v=>v/(w*h));patches.push({id:region.id,kind:region.kind,exposure_ev:region.exposure_ev,Lab:xyzToLab(average),Y:average[1]});}
 const spatial={};for(const kind of ['grain','halation'])if(spec.spatial?.[kind]){
  const region=spec.spatial[kind],[x,y,w,h]=region.rect;requireThat([x,y,w,h].every(Number.isInteger)&&x>=0&&y>=0&&w>0&&h>0&&x+w<=im.width&&y+h<=im.height&&w*h<=1048576,'Bad spatial ROI');const samples=[];
  for(let py=y;py<y+h;py++)for(let px=x;px<x+w;px++){const i=(py*im.width+px)*3;samples.push(xyz([...im.pixels.subarray(i,i+3)])[1]);}
  spatial[kind]={...region,width:w,height:h,samples,basis:'linear_relative_luminance',source_bit_depth:im.depth};delete spatial[kind].rect;
 }
 const result={format:'SpektraLab.PhoenixObservations',version:1,sample_id:spec.sample_id,origin_kind:spec.origin_kind,source_image_sha256:hash(bytes),extraction_method:'PNG8/16 reviewed tagged sRGB; linear mean XYZ then D50/2deg Lab; explicit pixel ROIs, no fitting',colorimetry:{space:'CIELAB',whitepoint:'D50',observer:'2deg',relative_y_scale:1,icc_sha256:im.icc_sha256},patches,spatial};
 result.roi_registration=registration?{artifact_sha256:registration.artifact.sha256,coordinate_source:registration.coordinate_source,reviewed_by:registration.reviewed_by,reviewed_at:registration.reviewed_at,method:registration.method,error_px:registration.error_px,regions:registration.regions}:notVerified('Synthetic arithmetic fixture has no physical registration');
 result.status=registration?'Geometry checked':'Not Verified';validateObservations(result);return result;
}
module.exports={extract,xyz};
