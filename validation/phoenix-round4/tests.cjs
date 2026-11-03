'use strict';
const {test}=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),os=require('node:os'),path=require('node:path'),crypto=require('node:crypto');
const {validate}=require('./dataset.cjs');
const empty={format:'SpektraLab.PhoenixRound4Dataset',version:1,samples:[]};
test('empty manifest is explicitly not verified',()=>assert.equal(validate(empty,'.').status,'EMPTY_NOT_VERIFIED'));
test('empty dataset cannot fit',()=>assert.throws(()=>validate(empty,'.',{requireFit:true}),/absent/));
function fixture(fn){let dir=fs.mkdtempSync(path.join(os.tmpdir(),'phoenix-r4-'));try{fs.writeFileSync(path.join(dir,'source.bin'),'fixture only');const f={path:'source.bin',sha256:crypto.createHash('sha256').update('fixture only').digest('hex')};const s={id:'a',generation:'Phoenix 200',batch:'test',roll_id:'roll-a',scene_id:'scene-a',split:'train',source:'synthetic test fixture',license:'test only',permission:'authorized',scene:'test',light:{description:'test',spd:{value:null,reason:'fixture'}},chart:{model:'test',serial:'test',reference:'test'},c41:{lab:'test',date:'2026-11-03',temperature_c:38,time_seconds:195,chemistry:'test',push_pull_stops:0},scanner:{model:'test',serial:'test',light:'test',icc_intent:'relative'},iso:200,ei:200,exposure_seconds:.01,aperture:8,exposure_ev:0,negative:f,scan:f,icc:f,scan_settings:{},auto_corrections:Object.fromEntries(['exposure','white_balance','contrast','saturation','sharpen','grain_reduction'].map(x=>[x,false]))};fn(dir,s);}finally{fs.rmSync(dir,{recursive:true,force:true});}}
test('same scene bracket across splits rejected',()=>fixture((dir,s)=>assert.throws(()=>validate({...empty,samples:[s,{...s,id:'b',roll_id:'roll-b',split:'validation'}]},dir),/Leakage/)));
test('same roll different scenes across splits rejected',()=>fixture((dir,s)=>assert.throws(()=>validate({...empty,samples:[s,{...s,id:'b',scene_id:'scene-b',split:'validation'}]},dir),/Leakage/)));
test('tampered input rejected',()=>fixture((dir,s)=>{fs.writeFileSync(path.join(dir,'source.bin'),'changed');assert.throws(()=>validate({...empty,samples:[s]},dir),/SHA256/);}));
test('unknown correction switch rejected',()=>fixture((dir,s)=>{delete s.auto_corrections.exposure;assert.throws(()=>validate({...empty,samples:[s]},dir),/correction/);}));
test('synthetic fixture cannot pass evidence gate',()=>fixture((dir,s)=>assert.throws(()=>validate({...empty,samples:[s,{...s,id:'b',scene_id:'b',roll_id:'b',split:'validation'}]},dir,{requireFit:true}),/evidence|Leakage/)));

test('renaming identities cannot reuse the same negative across splits',()=>fixture((dir,s)=>assert.throws(()=>validate({...empty,samples:[s,{...s,id:'b',scene_id:'b',roll_id:'b',split:'validation'}]},dir),/media hash/)));
