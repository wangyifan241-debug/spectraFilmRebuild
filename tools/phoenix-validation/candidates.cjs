'use strict';
const cp=require('node:child_process');
const {fs,path,requireThat,json,binding,digest,text,hash,inside,fileHash}=require('./common.cjs');
const categories={
 'data.log_sensitivity':'spectral_response','info.reference_illuminant':'spectral_response',
 'data.log_exposure':'exposure_response','data.density_curves':'exposure_response','data.density_curves_layers':'exposure_response','data.density_curves_model':'exposure_response',
 'data.channel_density':'dye_density','data.base_density':'base_density',
 'info.render_model.dir_couplers_scale':'dir_couplers','info.render_model.grain_particle_area_um2':'grain',
 'info.render_model.halation_strength_rgb':'halation','info.render_model.halation_sigma_um':'halation'};
function leaves(a,b,prefix='',out=[]){if(JSON.stringify(a)===JSON.stringify(b))return out;if(a&&b&&typeof a==='object'&&typeof b==='object'&&!Array.isArray(a)&&!Array.isArray(b)){for(const k of new Set([...Object.keys(a),...Object.keys(b)]))leaves(a[k],b[k],prefix?prefix+'.'+k:k,out);}else out.push(prefix);return out;}
function version(v){requireThat(/^\d+\.\d+\.\d+$/.test(v),'Material needs semantic version');return v.split('.').map(Number);}
function newer(a,b){const x=version(a),y=version(b);for(let i=0;i<3;i++)if(x[i]!==y[i])return x[i]>y[i];return false;}
function validateCandidate(root,plan,lock){
 requireThat(plan.format==='SpektraLab.PhoenixCandidate'&&plan.version===1&&text(plan.id)&&plan.id!=='experimental-v1','Invalid/new candidate ID required');
 requireThat(plan.parent_commit===lock.commit&&plan.renderer_commit===lock.commit,'Candidate must retain pinned baseline renderer');
 requireThat(text(plan.hypothesis?.category)&&text(plan.hypothesis.rationale)&&['A','B','C'].includes(plan.hypothesis.evidence_level),'Hypothesis provenance missing');
 requireThat(Array.isArray(plan.profiles)&&plan.profiles.length>0&&plan.profiles.length<=2,'Candidate needs one or two new material files');
 const changes=[],classes=new Set(),ids=new Set();
 for(const entry of plan.profiles){const beforeLock=lock.profiles.find(p=>p.stock===entry.parent_stock);requireThat(beforeLock,'Unknown parent material');const bound=binding(root,entry.file);requireThat(bound.ok,bound.reason);
  const before=json(inside(root,entry.parent_path??beforeLock.path)),after=json(bound.file);requireThat(fileHash(inside(root,entry.parent_path??beforeLock.path))===beforeLock.sha256,'Parent material changed');
  requireThat(/^[A-Za-z0-9_]{1,48}$/.test(after.info?.stock)&&!lock.profiles.some(p=>p.stock===after.info.stock)&&!ids.has(after.info.stock),'New unique candidate stock ID required');ids.add(after.info.stock);
  requireThat(newer(after.metadata?.version,before.metadata.version)&&after.metadata.parent_material_sha256===beforeLock.sha256,'New version/parent provenance required');
  requireThat(after.metadata.photographic_fidelity_validated===false,'Candidate remains experimentally unverified');
  requireThat(text(after.metadata.license)&&text(after.metadata.datasource)&&after.metadata.source_sha256&&Object.values(after.metadata.source_sha256).every(digest),'Complete candidate source provenance required');
  for(const changed of leaves(before,after)){if(changed.startsWith('metadata.')||['info.stock','info.name'].includes(changed))continue;const key=Object.keys(categories).find(k=>changed===k||changed.startsWith(k+'.'));requireThat(key,'Unclassified physical/config change: '+changed);classes.add(categories[key]);changes.push({stock:after.info.stock,path:changed,category:categories[key]});}
 }
 requireThat(classes.size===1&&[...classes][0]===plan.hypothesis.category,'Exactly one declared physical hypothesis category may change');
 requireThat(text(plan.hypothesis.source_record),'Hypothesis source/uncertainty record missing');return {status:'Pass',changed_category:[...classes][0],changes};
}
function runRenders(root,plan,lock,destination){
 validateCandidate(root,plan,lock);requireThat(!fs.existsSync(destination),'Render run directory already exists');
 const runner=plan.renderer;requireThat(runner&&text(runner.executable)&&digest(runner.sha256)&&Array.isArray(runner.args)&&runner.args.every(text),'Explicit pinned executable/argv required');
 requireThat(fs.existsSync(runner.executable)&&fileHash(runner.executable)===runner.sha256,'Renderer executable hash mismatch');
 requireThat(!/\.(cmd|bat|ps1)$/i.test(runner.executable),'Use a direct executable, never shell commands');
 const assets=runner.assets??[];for(const asset of assets){const b=binding(root,asset);requireThat(b.ok,b.reason);}
 for(const arg of runner.args)if(!arg.includes('{')&&fs.existsSync(path.resolve(root,arg))&&fs.statSync(path.resolve(root,arg)).isFile())requireThat(assets.some(a=>path.resolve(root,a.path)===path.resolve(root,arg)),'Renderer argv file needs a hash-bound asset: '+arg);
 const recipe=binding(root,plan.recipe),resourceRoot=path.resolve(root,runner.resources);requireThat(recipe.ok,recipe.reason);requireThat(fs.statSync(resourceRoot).isDirectory(),'Missing built resources');
 requireThat(!path.resolve(destination).startsWith(resourceRoot+path.sep),'Render destination cannot be inside input resources');
 const tree=[];function inventory(dir){for(const ent of fs.readdirSync(dir,{withFileTypes:true})){const file=path.join(dir,ent.name);requireThat(!ent.isSymbolicLink(),'Renderer resources cannot contain symlinks');if(ent.isDirectory())inventory(file);else tree.push({path:path.relative(resourceRoot,file).replaceAll('\\','/'),sha256:fileHash(file)});}}inventory(resourceRoot);
 const byPath=new Map(tree.map(f=>[f.path,f.sha256]));
 for(const f of lock.files.filter(f=>f.path.startsWith('engine/resources/'))){const rel=f.path.slice('engine/resources/'.length);if(rel==='neutral_print_filters.json')continue;requireThat(byPath.has(rel)&&require('./common.cjs').frozenMatch(path.join(resourceRoot,rel),f.sha256),'Renderer baked resource differs: '+rel);}
 for(const p of lock.profiles)requireThat(byPath.get('profiles/'+path.basename(p.path))===p.sha256,'Renderer baseline material differs');
 const expectedPack=lock.files.find(f=>f.path==='engine/resources_experimental/neutral_print_filters.json');requireThat(byPath.get('neutral_print_filters.json')===expectedPack.sha256,'Renderer print pack differs');
 fs.mkdirSync(destination,{recursive:true});const staged=path.join(destination,'resources');fs.cpSync(resourceRoot,staged,{recursive:true,errorOnExist:true,force:false});
 for(const entry of plan.profiles){const candidate=json(binding(root,entry.file).file),file=path.join(staged,'profiles',candidate.info.stock+'.json');requireThat(!fs.existsSync(file),'Candidate would overwrite a material');fs.copyFileSync(binding(root,entry.file).file,file);}
 // Copy the parent's print packs to the new ID: calibration is frozen, never silently solved.
 const filterPath=path.join(staged,'neutral_print_filters.json'),filters=json(filterPath);for(const entry of plan.profiles){const id=json(binding(root,entry.file).file).info.stock;for(const lights of Object.values(filters))for(const stocks of Object.values(lights))if(stocks[entry.parent_stock])stocks[id]=stocks[entry.parent_stock];}fs.writeFileSync(filterPath,JSON.stringify(filters,null,2)+'\n');
 const records=[];requireThat(Array.isArray(plan.fixed_inputs)&&plan.fixed_inputs.length>0,'Fixed inputs missing');
 for(const input of plan.fixed_inputs){const bound=binding(root,input.file);requireThat(bound.ok,bound.reason);for(const entry of plan.profiles){const candidate=json(binding(root,entry.file).file);for(const [modelId,stock] of [['experimental-v1',entry.parent_stock],[plan.id,candidate.info.stock]]){
  requireThat(/^[a-zA-Z0-9_-]+$/.test(input.id)&&/^[a-zA-Z0-9_-]+$/.test(modelId),'Unsafe render identity');const output=path.join(destination,modelId+'-'+stock+'-'+input.id+'.png'),parameters=path.join(destination,modelId+'-'+stock+'-'+input.id+'-params.json');
  const params={...json(recipe.file).engine_parameters,film_stock:stock};fs.writeFileSync(parameters,JSON.stringify(params,null,2)+'\n',{flag:'wx'});
  const replacements={resources:staged,input:bound.file,output,stock,params:parameters,recipe:recipe.file};
  const args=runner.args.map(arg=>arg.replace(/\{(resources|input|output|stock|params|recipe)\}/g,(_,key)=>replacements[key]));requireThat(args.every(a=>!/[{}]/.test(a)),'Unknown runner placeholder');
  const result=cp.spawnSync(runner.executable,args,{shell:false,timeout:runner.timeout_ms??120000,maxBuffer:1024*1024});requireThat(result.status===0,'Renderer failed: '+(result.stderr?.toString()??result.error));requireThat(fs.existsSync(output),'Renderer produced no declared output');
  records.push({model_id:modelId,stock,input_id:input.id,input_sha256:input.file.sha256,output_path:output,output_sha256:fileHash(output),recipe_sha256:plan.recipe.sha256,renderer_sha256:runner.sha256});
 }}}
 const record={format:'SpektraLab.PhoenixRenderRun',version:1,category:plan.hypothesis.category,renderer_commit:plan.renderer_commit,renderer:{executable:runner.executable,sha256:runner.sha256,args:runner.args,assets},recipe_sha256:plan.recipe.sha256,resources_sha256:hash(JSON.stringify(tree)),resource_inventory:tree,records};fs.writeFileSync(path.join(destination,'render-run.json'),JSON.stringify(record,null,2)+'\n');return record;
}
module.exports={validateCandidate,runRenders,leaves};
