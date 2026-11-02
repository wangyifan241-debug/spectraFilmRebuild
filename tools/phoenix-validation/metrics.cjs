'use strict';
const {requireThat,finite}=require('./common.cjs');
const rad=d=>d*Math.PI/180,deg=r=>r*180/Math.PI;
function lab(value){requireThat(Array.isArray(value)&&value.length===3&&value.every(finite)&&value[0]>=0&&value[0]<=100&&Math.abs(value[1])<=200&&Math.abs(value[2])<=200,'Invalid finite CIELAB triplet');return value;}
// Independent implementation of Sharma/Wu/Dalal 2005, kL=kC=kH=1.
function deltaE00(first,second){
 const [L1,a1,b1]=lab(first),[L2,a2,b2]=lab(second),C1=Math.hypot(a1,b1),C2=Math.hypot(a2,b2),meanC=(C1+C2)/2;
 const G=.5*(1-Math.sqrt(meanC**7/(meanC**7+25**7))),ap1=(1+G)*a1,ap2=(1+G)*a2;
 const cp1=Math.hypot(ap1,b1),cp2=Math.hypot(ap2,b2),h=(a,b)=>{let v=deg(Math.atan2(b,a));return v<0?v+360:v;},hp1=cp1===0?0:h(ap1,b1),hp2=cp2===0?0:h(ap2,b2);
 const dL=L2-L1,dC=cp2-cp1;let dh=hp2-hp1;if(cp1*cp2===0)dh=0;else if(dh>180)dh-=360;else if(dh< -180)dh+=360;
 const dH=2*Math.sqrt(cp1*cp2)*Math.sin(rad(dh/2)),meanL=(L1+L2)/2,meanCp=(cp1+cp2)/2;
 let meanH;if(cp1*cp2===0)meanH=hp1+hp2;else if(Math.abs(hp1-hp2)<=180)meanH=(hp1+hp2)/2;else meanH=(hp1+hp2+(hp1+hp2<360?360:-360))/2;
 const T=1-.17*Math.cos(rad(meanH-30))+.24*Math.cos(rad(2*meanH))+.32*Math.cos(rad(3*meanH+6))-.20*Math.cos(rad(4*meanH-63));
 const SL=1+.015*(meanL-50)**2/Math.sqrt(20+(meanL-50)**2),SC=1+.045*meanCp,SH=1+.015*meanCp*T;
 const RT=-2*Math.sqrt(meanCp**7/(meanCp**7+25**7))*Math.sin(rad(60*Math.exp(-(((meanH-275)/25)**2))));
 const x=dL/SL,y=dC/SC,z=dH/SH;return Math.sqrt(Math.max(0,x*x+y*y+z*z+RT*y*z));
}
function xyzToLab(xyz){requireThat(xyz.length===3&&xyz.every(finite),'Invalid XYZ');const white=[.96422,1,.82521],f=t=>t>216/24389?Math.cbrt(t):(24389/27*t+16)/116;const [x,y,z]=xyz.map((v,i)=>f(v/white[i]));return [116*y-16,500*(x-y),200*(y-z)];}
function mean(a){return a.reduce((s,x)=>s+x,0)/a.length;}
function rmse(a){return Math.sqrt(mean(a.map(x=>x*x)));}
function percentile(a,p){const s=[...a].sort((a,b)=>a-b),i=(s.length-1)*p;return s[Math.floor(i)]+(s[Math.ceil(i)]-s[Math.floor(i)])*(i%1);}
function validateObservations(packet){
 requireThat(packet?.format==='SpektraLab.PhoenixObservations'&&packet.version===1,'Unsupported observations');
 const c=packet.colorimetry;requireThat(c?.space==='CIELAB'&&c.whitepoint==='D50'&&c.observer==='2deg'&&c.relative_y_scale===1,'Observations require common D50/2deg Lab and relative Y=1');
 requireThat(Array.isArray(packet.patches)&&packet.patches.length>0&&packet.patches.length<=10000,'No/bad patches');const ids=new Set();
 for(const p of packet.patches){requireThat(typeof p.id==='string'&&!ids.has(p.id),'Duplicate/missing patch identity');ids.add(p.id);lab(p.Lab);requireThat(finite(p.Y)&&p.Y>=0&&p.Y<=2,'Invalid relative Y');requireThat(['gray','color'].includes(p.kind),'Unknown patch kind');if(p.kind==='gray')requireThat(finite(p.exposure_ev),'Gray exposure coordinate missing');}
}
function pairedMetrics(reference,model){
 validateObservations(reference);validateObservations(model);
 requireThat(reference.patches.length===model.patches.length,'Patch coverage mismatch');const byId=new Map(model.patches.map(p=>[p.id,p]));
 const rows=reference.patches.map(r=>{const m=byId.get(r.id);requireThat(m&&m.kind===r.kind&&m.exposure_ev===r.exposure_ev,'Patch pairing/exposure mismatch');return {id:r.id,kind:r.kind,exposure_ev:r.exposure_ev??null,delta_e00:deltaE00(r.Lab,m.Lab),delta_L:m.Lab[0]-r.Lab[0],delta_Y:m.Y-r.Y,reference_L:r.Lab[0],model_L:m.Lab[0],reference_Y:r.Y,model_Y:m.Y};});
 const colors=rows.map(r=>r.delta_e00),gray=rows.filter(r=>r.kind==='gray').sort((a,b)=>a.exposure_ev-b.exposure_ev);
 const slopes=[];for(let i=1;i<gray.length;i++){const a=gray[i-1],b=gray[i],dx=b.exposure_ev-a.exposure_ev;requireThat(dx>0,'Duplicate gray EV');slopes.push(((b.model_L-a.model_L)-(b.reference_L-a.reference_L))/dx);}
 return {patch_count:rows.length,delta_e00:{mean:mean(colors),p95:percentile(colors,.95),max:Math.max(...colors)},luminance:{rmse_Y:rmse(rows.map(r=>r.delta_Y)),mae_Y:mean(rows.map(r=>Math.abs(r.delta_Y)))},gray_response:gray.length>=3?{count:gray.length,rmse_L:rmse(gray.map(r=>r.delta_L)),slope_rmse_L_per_EV:rmse(slopes),points:gray}:null,patches:rows};
}
// Scan ROI statistics: not standardized film RMS granularity or a physical PSF.
function spatial(packet,kind){
 const p=packet?.spatial?.[kind];if(!p)return null;
 requireThat(Number.isInteger(p.width)&&Number.isInteger(p.height)&&p.width>=32&&p.height>=32&&p.width*p.height<=1048576,'Scan ROI must be 32px+ and <=1M pixels');
 requireThat(finite(p.pixel_pitch_um)&&p.pixel_pitch_um>0&&p.pixel_pitch_um<=10&&Number.isInteger(p.source_bit_depth)&&p.source_bit_depth>=16&&p.source_bit_depth<=64,'Insufficient scan spatial/depth calibration');
 requireThat(p.basis==='linear_relative_luminance'&&p.processing?.denoise===false&&p.processing?.sharpen===false&&p.processing?.tone_curve==='linear','Uncontrolled scan processing');
 requireThat(Array.isArray(p.samples)&&p.samples.length===p.width*p.height&&p.samples.every(v=>finite(v)&&v>=0&&v<=2),'Invalid relative-Y ROI samples');return p;
}
function grainStatistics(packet){const p=spatial(packet,'grain');if(!p)return null;const {width:w,height:h,samples:a}=p;
 const mx=(w-1)/2,my=(h-1)/2,avg=mean(a);let xx=0,yy=0,xv=0,yv=0;
 for(let y=0;y<h;y++)for(let x=0;x<w;x++){const dx=x-mx,dy=y-my,v=a[y*w+x]-avg;xx+=dx*dx;yy+=dy*dy;xv+=dx*v;yv+=dy*v;}
 const bx=xv/xx,by=yv/yy,res=a.map((v,i)=>v-avg-bx*(i%w-mx)-by*(Math.floor(i/w)-my)),sigma=rmse(res);let m3=0,neighbor=0,pairs=0;
 for(let y=0;y<h;y++)for(let x=0;x<w;x++){const i=y*w+x;m3+=res[i]**3;if(x+1<w){neighbor+=res[i]*res[i+1];pairs++;}}
 return {method:'plane-detrended linear scan Y ROI; not standardized density granularity; moments zero below 1e-12 RMS',count:a.length,pixel_pitch_um:p.pixel_pitch_um,mean_Y:avg,rms_Y:sigma,skewness:sigma>1e-12?m3/a.length/sigma**3:0,lag1_x:sigma>1e-12?neighbor/pairs/sigma**2:0};}
function halationStatistics(packet){const p=spatial(packet,'halation');if(!p)return null;const [cx,cy]=p.center_px??[];
 requireThat(finite(cx)&&finite(cy)&&finite(p.core_radius_px)&&p.core_radius_px>0&&Number.isInteger(p.outer_radius_px)&&p.outer_radius_px>p.core_radius_px,'Invalid halation geometry');
 requireThat(cx-p.outer_radius_px>=0&&cy-p.outer_radius_px>=0&&cx+p.outer_radius_px<p.width&&cy+p.outer_radius_px<p.height,'Truncated halo ROI');
 requireThat(finite(p.background_Y)&&p.background_Y>=0,'Background measurement missing');const bins=Array.from({length:p.outer_radius_px+1},()=>[]);let core=0,halo=0;
 for(let y=0;y<p.height;y++)for(let x=0;x<p.width;x++){const r=Math.hypot(x-cx,y-cy);if(r>p.outer_radius_px)continue;const v=p.samples[y*p.width+x];bins[Math.floor(r)].push(v);if(r<=p.core_radius_px)core+=Math.max(0,v-p.background_Y);else halo+=Math.max(0,v-p.background_Y);}
 requireThat(core>0,'No measured highlight core');return {method:'background-subtracted positive scan-Y annular energy; not isolated film PSF',pixel_pitch_um:p.pixel_pitch_um,halo_to_core_energy:halo/core,radial_profile:bins.map((b,i)=>({radius_um:i*p.pixel_pitch_um,count:b.length,mean_Y:b.length?mean(b):null}))};}
module.exports={deltaE00,xyzToLab,pairedMetrics,grainStatistics,halationStatistics,validateObservations};
