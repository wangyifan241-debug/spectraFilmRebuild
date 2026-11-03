'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
const fail=s=>{throw Error(s);};
const text=(v,k)=>{if(typeof v!=='string'||!v.trim())fail('Missing '+k);};
const hash=b=>crypto.createHash('sha256').update(b).digest('hex');
function validate(m,root,{requireFit=false}={}){
 if(m.format!=='SpektraLab.PhoenixRound4Dataset'||m.version!==1||!Array.isArray(m.samples))fail('Invalid manifest');
 const ids=new Set(),rolls=new Map(),scenes=new Map(),media=new Map();
 for(const s of m.samples){
  for(const k of ['id','generation','batch','roll_id','scene_id','split','source','license','permission','scene'])text(s[k],k);
  if(ids.has(s.id))fail('Duplicate sample');ids.add(s.id);
  if(!['Phoenix 200','Phoenix II'].includes(s.generation)||!['train','validation','test'].includes(s.split))fail('Unknown generation/split');
  for(const k of ['iso','ei','exposure_seconds','aperture'])if(!Number.isFinite(s[k])||s[k]<=0)fail('Invalid '+k);
  if(!Number.isFinite(s.exposure_ev))fail('Missing exposure EV');
  for(const k of ['negative','scan','icc']){
   const f=s[k];if(!f||typeof f!=='object')fail('Missing '+k);
   text(f.path,k+' path');if(!/^[a-f0-9]{64}$/.test(f.sha256))fail('Invalid SHA256');
   const resolved=path.resolve(root,f.path),rel=path.relative(path.resolve(root),resolved);
   if(rel.startsWith('..'+path.sep)||rel==='..'||path.isAbsolute(rel))fail('File escapes dataset');
   const real=fs.realpathSync(resolved),realRel=path.relative(fs.realpathSync(root),real);
   if(realRel.startsWith('..'+path.sep)||realRel==='..'||path.isAbsolute(realRel))fail('Symlink escapes dataset');
   if(hash(fs.readFileSync(real))!==f.sha256)fail('SHA256 mismatch: '+k);
   if(k!=='icc'){if(media.has(f.sha256)&&media.get(f.sha256)!==s.split)fail('Leakage across media hash');media.set(f.sha256,s.split);}
  }
  const recorded=(v,k)=>{if(v===null||v===undefined||typeof v==='string'&&!v.trim())fail('Unrecorded '+k);if(typeof v==='object'&&v.value===null)text(v.reason,k+' unknown reason');};
  for(const [k,keys] of Object.entries({light:['description','spd'],chart:['model','serial','reference'],c41:['lab','date','temperature_c','time_seconds','chemistry','push_pull_stops'],scanner:['model','serial','light','icc_intent']})){if(!s[k]||typeof s[k]!=='object')fail('Missing structured '+k);for(const key of keys)recorded(s[k][key],k+'.'+key);}
  if(!s.scan_settings||!s.auto_corrections||typeof s.auto_corrections!=='object'||Array.isArray(s.auto_corrections))fail('Missing scan settings/switches');
  for(const k of ['exposure','white_balance','contrast','saturation','sharpen','grain_reduction'])if(typeof s.auto_corrections[k]!=='boolean')fail('Unknown automatic correction '+k);
  for(const [map,key] of [[rolls,s.roll_id],[scenes,s.scene_id]]){if(map.has(key)&&map.get(key)!==s.split)fail('Leakage across roll or scene');map.set(key,s.split);}
 }
 if(requireFit){if(m.samples.length===0||!m.samples.some(s=>s.split==='train')||!m.samples.some(s=>s.split==='validation'))fail('Real paired training/validation data absent');if(m.samples.some(s=>s.evidence!=='real-film-paired'||s.permission!=='authorized'))fail('Real paired evidence/permission gate failed');}
 return {status:m.samples.length?'MANIFEST_INTEGRITY_ONLY_NOT_PHYSICAL_VALIDATION':'EMPTY_NOT_VERIFIED',sample_count:m.samples.length,fitting_enabled:false};
}
module.exports={validate};
if(require.main===module){try{let f=process.argv[2];console.log(JSON.stringify(validate(JSON.parse(fs.readFileSync(f,'utf8')),path.dirname(path.resolve(f)),{requireFit:process.argv.includes('--require-fit')}),null,2));}catch(e){console.error(e.message);process.exitCode=1;}}
