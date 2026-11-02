'use strict';
const fs=require('fs'),cp=require('child_process'),path=require('path'),crypto=require('crypto');
const out='validation/phoenix-round2',build=path.resolve('build/round2');fs.mkdirSync(build,{recursive:true});
const source='D:/学习资料/SpektraLab/build/phoenix-20261102';const resources=path.join(build,'resources');
if(!fs.existsSync(resources))fs.cpSync(source+'/app/resources',resources,{recursive:true});
for(const f of fs.readdirSync(out+'/profiles'))fs.copyFileSync(out+'/profiles/'+f,resources+'/profiles/'+f);
const filters=JSON.parse(fs.readFileSync(resources+'/neutral_print_filters.json'));
for(const f of fs.readdirSync(out+'/profiles')){const p=JSON.parse(fs.readFileSync(out+'/profiles/'+f));const parent=p.info.stock.startsWith('phx_ii')?'harman_phoenix_ii_200_exp_v1':'harman_phoenix_200_exp_v1';for(const x of Object.values(filters))for(const y of Object.values(x))if(y[parent])y[p.info.stock]=y[parent];}
fs.writeFileSync(resources+'/neutral_print_filters.json',JSON.stringify(filters));
const w=32,h=32,b=Buffer.alloc(w*h*12);for(let y=0;y<h;y++)for(let x=0;x<w;x++)for(let c=0;c<3;c++)b.writeFloatLE(y<16?.18*2**(-6+12*x/31):[.35,.16,.08][c]*(.2+x/31),(y*w+x)*12+c*4);fs.writeFileSync(build+'/input.f32',b);
const recipe=JSON.parse(fs.readFileSync('validation/phoenix-accuracy/fixed-recipe.v1.json')).engine_parameters;
recipe.exposure_compensation_ev=recipe.film_exposure_ev;delete recipe.film_exposure_ev;recipe.auto_exposure=false;
const ids=['harman_phoenix_200_exp_v1','harman_phoenix_ii_200_exp_v1',...fs.readdirSync(out+'/profiles').map(f=>f.slice(0,-5))];
const results=[];for(const stock of ids){const params=build+'/'+stock+'-params.json',pixels=build+'/'+stock+'.rgba16',report=build+'/'+stock+'.json';fs.writeFileSync(params,JSON.stringify({...recipe,film_stock:stock}));const exe=source+'/engine/spk_render_fixture.exe';const r=cp.spawnSync(exe,[resources,build+'/input.f32',''+w,''+h,params,pixels,report],{encoding:'utf8',timeout:120000});results.push({stock,exit:r.status,stderr:r.stderr,report:fs.existsSync(report)?JSON.parse(fs.readFileSync(report)):null});console.log(stock,r.status);}
fs.writeFileSync(out+'/native-execution.json',JSON.stringify({purpose:'synthetic engineering only',renderer_sha256:crypto.createHash('sha256').update(fs.readFileSync(source+'/engine/spk_render_fixture.exe')).digest('hex'),input_sha256:crypto.createHash('sha256').update(b).digest('hex'),results},null,2));

