'use strict';
const cp=require('node:child_process');
const {BASE,fs,path,requireThat,hash,fileHash,json,inside}=require('./common.cjs');
function git(root,args){const r=cp.spawnSync('git',['-C',root,...args],{maxBuffer:64*1024*1024});requireThat(r.status===0,'Cannot read pinned Git object: '+r.stderr.toString());return r.stdout;}
function createLock(root){
 const names=git(root,['ls-tree','-r','--name-only',BASE]).toString().trim().split('\n');
 const pinned=names.filter(n=>n.startsWith('engine/resources/')||n.startsWith('engine/resources_experimental/')||n.startsWith('engine/resources_research/phoenix')||['engine/resources_research/build-phoenix.cjs','engine/resources_research/verify-phoenix.cjs'].includes(n)||n.startsWith('validation/phoenix-review-20261102/images/'));
 pinned.push('validation/phoenix-review-20261102/ctest-details.txt','validation/phoenix-review-20261102/ctest-summary.txt');
 const files=pinned.map(file=>({path:file,sha256:hash(git(root,['show',BASE+':'+file]))}));
 const profiles=files.filter(f=>/^engine\/resources_experimental\/profiles\//.test(f.path)).map(f=>{const p=JSON.parse(git(root,['show',BASE+':'+f.path]));return {...f,stock:p.info.stock,version:p.metadata.version,source_sha256:p.metadata.source_sha256};});
 const external=[];for(const file of ['engine/resources_research/phoenix200/digitization.json','engine/resources_research/phoenix2/digitization.json','engine/resources_research/phoenix2/characteristic-candidate.json']){const value=JSON.parse(git(root,['show',BASE+':'+file]));external.push({record:file,...value.source,availability:'URL/hash record; source may not be in checkout'});}
 const renderings=files.filter(f=>f.path.startsWith('validation/phoenix-review-20261102/images/')).map(f=>({...f,purpose:'engineering_only_not_photographic_reference'}));
 return {format:'SpektraLab.PhoenixBaselineLock',version:1,pr:4,commit:BASE,tree:git(root,['rev-parse',BASE+'^{tree}']).toString().trim(),profiles,files,external_sources:external,renderings};
}
function verifyLock(root,lock){
 requireThat(lock.format==='SpektraLab.PhoenixBaselineLock'&&lock.version===1&&lock.commit===BASE,'Wrong baseline lock');
 const expected=createLock(root);requireThat(JSON.stringify(lock)===JSON.stringify(expected),'Baseline lock differs from pinned PR #4 objects');
 const failures=[],checkout_differences=[];for(const f of lock.files){const abs=inside(root,f.path),match=fs.existsSync(abs)?require('./common.cjs').frozenMatch(abs,f.sha256):null;if(!match)failures.push(f.path);else if(match!=='exact')checkout_differences.push({path:f.path,kind:match,working_bytes_sha256:fileHash(abs)});}
 return {status:failures.length?'Fail':'Pass',commit:BASE,checked_files:lock.files.length,hash_domain:'Exact Git blob SHA256; text-only CRLF checkout recognized separately, no other normalization',checkout_differences,failures};
}
module.exports={createLock,verifyLock};
