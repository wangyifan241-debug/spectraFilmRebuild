'use strict';
const {requireThat,text,binding,json,notVerified}=require('./common.cjs');
// Geometric QC policy v1, in target-image pixels. Not a film accuracy threshold.
const registrationPolicy={version:1,max_rmse_px:1,max_error_px:2,minimum_fit_points:4,minimum_held_out_points:2};
function reviewed(r){return r?.status==='reviewed'&&text(r.reviewed_by)&&text(r.method)&&text(r.reviewed_at)&&Number.isFinite(Date.parse(r.reviewed_at));}
function pairingEvidence(sample,root){
 try{
  const b=binding(root,sample.files?.pairing_evidence);requireThat(b.ok,b.reason);const p=json(b.file);
  requireThat(p.format==='SpektraLab.PhoenixPairingEvidence'&&p.version===1,'Pairing needs a structured external evidence artifact');
  requireThat(p.sample_id===sample.id&&p.raw_sha256===sample.files.raw.sha256&&p.scan_sha256===sample.files.scan.sha256,'Pairing evidence file identity differs');
  requireThat(['negative_id','roll_id','scene_id','raw_capture_id'].every(k=>p.identity?.[k]===sample.identity[k]),'Capture/negative evidence identity differs');
  requireThat(reviewed(p.human_review)&&p.human_review.scope==='capture_provenance_and_scene_correspondence','Pairing lacks a dated human provenance review');
  requireThat(['timing','lighting','motion','exposure_relation'].every(k=>text(p.capture_relation?.[k])),'Pairing lacks capture timing, lighting/motion and exposure-relation assessment');
  requireThat(Array.isArray(p.capture_records)&&['film_frame_record','scene_capture_record'].every(role=>p.capture_records.some(r=>r.role===role&&text(r.description)&&binding(root,r.file).ok)),'Pairing requires file-bound film-frame and scene-capture records, not manifest assertions');
  return {status:'Evidence checked',artifact:sample.files.pairing_evidence,layers:{file_identity:'Hash verified against available bytes',human_review:'Documentary review attested; reviewer claims are not independently authenticated',scene_registration:'Separate geometric evidence required',physical_accuracy:'Not established by pairing or hashes'},human_review:p.human_review,capture_records:p.capture_records};
 }catch(e){return notVerified('Pairing evidence: '+e.message);}
}
function project(matrix,[x,y]){const w=matrix[6]*x+matrix[7]*y+matrix[8];requireThat(Number.isFinite(w)&&Math.abs(w)>1e-10,'Singular registration projection');return [(matrix[0]*x+matrix[1]*y+matrix[2])/w,(matrix[3]*x+matrix[4]*y+matrix[5])/w];}
function point(p,size){return Array.isArray(p)&&p.length===2&&p.every(Number.isFinite)&&p[0]>=0&&p[1]>=0&&p[0]<size[0]&&p[1]<size[1];}
function rect(r,size){return Array.isArray(r)&&r.length===4&&r.every(Number.isFinite)&&r[0]>=0&&r[1]>=0&&r[2]>0&&r[3]>0&&r[0]+r[2]<=size[0]&&r[1]+r[3]<=size[1];}
function registrationEvidence(root,asset,expected){
 try{
  const b=binding(root,asset);requireThat(b.ok,b.reason);const r=json(b.file);
  requireThat(r.format==='SpektraLab.PhoenixRegistration'&&r.version===1,'Unsupported registration adapter');
  requireThat(r.sample_id===expected.sample_id&&r.source_sha256===expected.source_sha256&&r.target_sha256===expected.target_sha256,'Registration image identity differs');
  requireThat(reviewed(r)&&text(r.coordinate_source)&&r.coordinate_units==='target_image_pixels','Missing coordinate origin / dated registration review');
  const sizes=[r.source_size,r.target_size];requireThat(sizes.every(s=>Array.isArray(s)&&s.length===2&&s.every(n=>Number.isInteger(n)&&n>0)),'Missing registration image dimensions');
  const h=r.transform;requireThat(Array.isArray(h)&&h.length===9&&h.every(Number.isFinite),'Registration needs an external 3x3 projective transform');
  const det=h[0]*(h[4]*h[8]-h[5]*h[7])-h[1]*(h[3]*h[8]-h[5]*h[6])+h[2]*(h[3]*h[7]-h[4]*h[6]);requireThat(Math.abs(det)>1e-12,'Singular registration transform');
  requireThat(Array.isArray(r.control_points),'No registration control points');
  const seen=new Set();for(const p of r.control_points){requireThat(['fit','held_out'].includes(p.role)&&point(p.source,r.source_size)&&point(p.target,r.target_size),'Invalid control point');const k=JSON.stringify(p.source);requireThat(!seen.has(k),'Duplicate registration control point');seen.add(k);}
  requireThat(r.control_points.filter(p=>p.role==='fit').length>=4&&r.control_points.filter(p=>p.role==='held_out').length>=2,'Registration requires 4 fit + 2 independent held-out points');
  const fit=r.control_points.filter(p=>p.role==='fit').map(p=>p.source);let area=0;for(let i=1;i<fit.length;i++)for(let j=i+1;j<fit.length;j++)area=Math.max(area,Math.abs((fit[i][0]-fit[0][0])*(fit[j][1]-fit[0][1])-(fit[j][0]-fit[0][0])*(fit[i][1]-fit[0][1])));requireThat(area>1,'Collinear/degenerate fit controls do not constrain scene registration');
  const all=r.control_points.map(p=>{const q=project(h,p.source);return {role:p.role,error:Math.hypot(q[0]-p.target[0],q[1]-p.target[1])};});
  const held=all.filter(p=>p.role==='held_out'),rmse=Math.sqrt(held.reduce((s,p)=>s+p.error**2,0)/held.length),max=Math.max(...all.map(p=>p.error));
  requireThat(rmse<=registrationPolicy.max_rmse_px&&max<=registrationPolicy.max_error_px,'Registration residual exceeds geometric QC policy');
  requireThat(Array.isArray(r.regions)&&r.regions.length>0,'No reviewed ROI coordinates');const ids=new Set();
  for(const roi of r.regions){requireThat(text(roi.id)&&!ids.has(roi.id)&&rect(roi.source_rect,r.source_size)&&rect(roi.target_rect,r.target_size),'Invalid/duplicate reviewed ROI');ids.add(roi.id);const [x,y,w,v]=roi.source_rect,points=[[x,y],[x+w,y],[x,y+v],[x+w,y+v]].map(p=>project(h,p));const xs=points.map(p=>p[0]),ys=points.map(p=>p[1]),bounds=[Math.min(...xs),Math.min(...ys),Math.max(...xs)-Math.min(...xs),Math.max(...ys)-Math.min(...ys)];requireThat(bounds.every((n,i)=>Math.abs(n-roi.target_rect[i])<=registrationPolicy.max_error_px),'ROI does not follow reviewed registration transform');}
  return {status:'Geometry checked',artifact:asset,coordinate_source:r.coordinate_source,reviewed_by:r.reviewed_by,reviewed_at:r.reviewed_at,method:r.method,source_size:r.source_size,target_size:r.target_size,regions:r.regions,error_px:{held_out_rmse:rmse,max},policy:registrationPolicy,limits:'Residuals verify submitted geometry, not truth of correspondences or emulsion accuracy'};
 }catch(e){return notVerified('ROI registration: '+e.message);}
}
function observationRegistration(root,packet,asset,expected){
 const r=registrationEvidence(root,asset,expected);if(r.status!=='Geometry checked')return r;
 try{const p=packet.roi_registration;requireThat(p?.artifact_sha256===asset.sha256&&p.reviewed_by===r.reviewed_by&&p.method===r.method&&p.coordinate_source===r.coordinate_source,'Observation lacks audited ROI coordinate origin / reviewer / method');
  requireThat(p.error_px?.held_out_rmse===r.error_px.held_out_rmse&&p.error_px?.max===r.error_px.max,'Observation registration error is unbound or inaccurate');
  for(const patch of packet.patches){const roi=r.regions.find(x=>x.id===patch.id),declared=p.regions?.find(x=>x.id===patch.id);requireThat(roi&&JSON.stringify(roi)===JSON.stringify(declared),'Patch ROI differs from registration evidence');}return r;
 }catch(e){return notVerified('Observation registration: '+e.message);}
}
module.exports={pairingEvidence,registrationEvidence,observationRegistration,registrationPolicy};
