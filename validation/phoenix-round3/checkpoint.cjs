'use strict';
const fs=require('fs'),crypto=require('crypto'),path=require('path'),assert=require('assert/strict');
const hash=bytes=>crypto.createHash('sha256').update(bytes).digest('hex');
function atomic(file,value){fs.mkdirSync(path.dirname(file),{recursive:true});fs.writeFileSync(file+'.tmp',JSON.stringify(value));fs.renameSync(file+'.tmp',file);}
function bind(file,identity,existing=[]){const signature=hash(JSON.stringify(identity));if(fs.existsSync(file)){const old=JSON.parse(fs.readFileSync(file));assert.equal(old.signature,signature,'Checkpoint identity mismatch: binary, input, resources, tool scripts or experiment parameters changed');assert.deepEqual(old.identity,identity,'Checkpoint identity content mismatch');}else{assert.ok(!existing.some(f=>fs.existsSync(f)),'Unbound legacy checkpoint refused; use a fresh PHOENIX_RESULT_DIR');atomic(file,{version:1,signature,identity});}return signature;}
function tree(dir,prefix=''){let records=[];for(const e of fs.readdirSync(dir,{withFileTypes:true}).sort((a,b)=>a.name.localeCompare(b.name))){const p=path.join(dir,e.name),rel=prefix+e.name;if(e.isDirectory())records.push(...tree(p,rel+'/'));else records.push({path:rel,sha256:hash(fs.readFileSync(p))});}return records;}
exports.hash=hash;exports.atomic=atomic;exports.bind=bind;exports.tree=tree;
