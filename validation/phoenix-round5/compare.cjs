'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const baseline=path.resolve(process.argv[2]),candidate=path.resolve(process.argv[3]),out=path.resolve(process.argv[4]);fs.mkdirSync(out,{recursive:true});
const stocks=['harman_phoenix_200_exp_v1','harman_phoenix_ii_200_exp_v1'];
const read=(root,name)=>{const b=fs.readFileSync(path.join(root,name));return Array.from({length:b.length/4},(_,i)=>b.readFloatLE(i*4));};
const results={classification:'synthetic model regression only; NOT real-film accuracy',physical_accuracy:'Not Verified',models:{}};
const input=read(candidate,'input-linear-prophoto.f32');
for(const stock of stocks){const a=read(baseline,stock+'-scan-linear-prophoto.f32'),b=read(candidate,stock+'-scan-linear-prophoto.f32');assert.equal(a.length,input.length);assert.equal(b.length,input.length);
 const score=v=>{let sum=0,count=0,nonfinite=0,negative=0;for(let i=0;i<v.length;i++){if(!Number.isFinite(v[i])){nonfinite++;continue;}negative+=v[i]<0;if(input[i]<=.75){sum+=(v[i]-input[i])**2;count++;}}return {rmse_linear_prophoto_below_075:Math.sqrt(sum/count),nonfinite,negative_channels:negative};};
 results.models[stock]={round4:score(a),round5:score(b)};assert.equal(score(b).nonfinite,0);
 for(const [root,suffix] of [[baseline,'round4-scan'],[candidate,'round5-scan'],[candidate,'v1-optical-print']])fs.copyFileSync(path.join(root,stock+'-'+suffix+'.png'),path.join(out,stock+'-'+suffix+'.png'));
 const recipe=JSON.parse(fs.readFileSync(path.join(candidate,stock+'-scan-recipe.json')));for(let row=0;row<3;row++)assert(Math.abs(recipe.scanner_to_linear_prophoto.slice(row*3,row*3+3).reduce((a,b)=>a+b,0)-1)<1e-10,'neutral constraint');
}
fs.writeFileSync(path.join(out,'metrics.json'),JSON.stringify(results,null,2));
fs.writeFileSync(path.join(out,'index.html'),'<!doctype html><meta charset="utf-8"><title>Phoenix round 5 comparison</title><style>body{background:#171717;color:#eee;font:16px system-ui}section{display:flex;gap:12px}figure{margin:0;flex:1}img{width:100%}</style><h1>Phoenix 5 · physical accuracy Not Verified</h1><p>Same synthetic input. Software regression only. Columns: v1 Optical Print / round 4 / round 5.</p>'+stocks.map(s=>'<h2>'+s+'</h2><section>'+['v1-optical-print','round4-scan','round5-scan'].map(m=>'<figure><figcaption>'+m+'</figcaption><img src="'+s+'-'+m+'.png"></figure>').join('')+'</section>').join('')+'<pre>'+JSON.stringify(results,null,2)+'</pre>');console.log(JSON.stringify(results,null,2));
