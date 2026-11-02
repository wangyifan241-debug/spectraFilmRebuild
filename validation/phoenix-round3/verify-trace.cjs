const fs=require('fs'),cp=require('child_process'),path=require('path'),crypto=require('crypto'),assert=require('assert/strict');
const hash=b=>crypto.createHash('sha256').update(b).digest('hex');const config=require('./config.cjs'),root=config.root,out='validation/phoenix-round3',dir=path.resolve(config.setting('REVIEW_TRACE_BUILD','build/review-trace')),resources=path.join(config.paths.round2,'resources');
const records=[];for(const id of ['harman_phoenix_200_exp_v1','harman_phoenix_ii_200_exp_v1'])for(const on of [true,false])for(let ev=-6;ev<=6;ev++){
 const stem=path.join(dir,`${id}-${on}-${ev}`),dest=stem+'-untraced';
 const r=cp.spawnSync(path.join(config.paths.clean,'spk_render_fixture.exe'),[resources,path.join(dir,'gray.f32'),'1','1',stem+'-params.json',dest+'.rgba16',dest+'.json'],{encoding:'utf8',timeout:120000});assert.equal(r.status,0,r.stderr);
 const a=fs.readFileSync(stem+'.rgba16'),b=fs.readFileSync(dest+'.rgba16');assert.ok(a.equals(b),stem);
 records.push({id,dir:on,ev,input_sha256:hash(fs.readFileSync(path.join(dir,'gray.f32'))),params_sha256:hash(fs.readFileSync(stem+'-params.json')),traced_sha256:hash(a),untraced_sha256:hash(b),equal:true});
}
fs.writeFileSync(out+'/trace-byte-equivalence.json',JSON.stringify({scope:'52 existing traced constant-gray outputs versus freshly executed uninstrumented renderer; final RGBA16 only, not all floating intermediates',records},null,2));console.log('52/52 traced versus untraced byte equality');
