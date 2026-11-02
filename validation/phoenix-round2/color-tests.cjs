const t=require('node:test'),a=require('node:assert/strict'),c=require('./color.cjs');
const near=(x,y,e=2e-6)=>a.ok(Math.abs(x-y)<e,`${x} != ${y}`);
t('independent D50 primary tristimuli',()=>{[[1,0,0],[0,1,0],[0,0,1]].forEach((v,i)=>c.xyz(v).forEach((x,j)=>near(x,[[.7976749,.2880402,0],[.1351917,.7118741,0],[.0313534,.0000857,.82521]][i][j])))});
t('white and black independent whitepoint',()=>{c.xyz([1,1,1]).forEach((x,i)=>near(x,[.96422,1,.82521][i]));a.deepEqual(c.xyz([0,0,0]),[0,0,0])});
t('ROMM published gamma and linear toe',()=>{near(c.decode(.5),.2871745887492587,1e-12);near(c.decode(.016),.001,1e-12);near(c.encode(.001),.016,1e-12)});
t('sRGB to ProPhoto agrees with independently composed Bradford D50 XYZ',()=>{[[1,0,0],[0,1,0],[0,0,1],[.5,.5,.5]].forEach((v,i)=>c.xyz(c.input(v),'ProPhoto RGB',false).forEach((x,j)=>near(x,[[.4360747,.2225045,.0139322],[.3850649,.7168786,.0971045],[.1430804,.0606169,.7141733],[.20638274,.21404114,.176628]][i][j],3e-6)))});
t('wrong-space negative control is rejected',()=>{a.throws(()=>c.xyz([.5,.5,.5],'sRGB'));const wrong=((.5+.055)/1.055)**2.4;a.ok(Math.abs(wrong-c.xyz([.5,.5,.5])[1])>.07)});

t('current native observations agree with decoded ProPhoto; historical sRGB path fails',()=>{const fs=require('fs'),{xyzToLab}=require('../../tools/phoenix-validation/metrics.cjs'),current=JSON.parse(fs.readFileSync(__dirname+'/exposure24-results.json'));for(const r of current.records)for(const p of r.patches){const X=c.xyz(p.RGB);near(X[1],p.Y,1e-12);xyzToLab(X).forEach((v,i)=>near(v,p.Lab[i],1e-10));}const old=JSON.parse(fs.readFileSync(__dirname+'/history/reviewed-6eb54e9d/exposure24-results.json'));a.ok(old.records.some(r=>r.patches.some(p=>Math.abs(c.xyz(p.RGB)[1]-p.Y)>.01)));});
