'use strict';
// D50 ProPhoto matrix from published ROMM primaries. Native piecewise curve
// matches colour.cpp and spk_cctf_encode_matrix.comp, not generic gamma sRGB.
const mul=(m,v)=>m.map(r=>r.reduce((s,x,i)=>s+x*v[i],0));
const proToXYZ=[[.7976749,.1351917,.0313534],[.2880402,.7118741,.0000857],[0,0,.82521]];
const xyzToPro=[[1.3459433,-.2556075,-.0511118],[-.5445989,1.5081673,.0205351],[0,0,1.2118128]];
const srgbToD50=[[.4360747,.3850649,.1430804],[.2225045,.7168786,.0606169],[.0139322,.0971045,.7141733]];
const decode=v=>v<16/512?v/16:v**1.8;
const encode=v=>v<1/512?v*16:v**(1/1.8);
const srgbDecode=v=>v<=.04045?v/12.92:((v+.055)/1.055)**2.4;
function xyz(rgb,space='ProPhoto RGB',encoded=true){if(space!=='ProPhoto RGB')throw Error('Unreviewed native output space: '+space);return mul(proToXYZ,encoded?rgb.map(decode):rgb);}
function input(rgb){return mul(xyzToPro,mul(srgbToD50,rgb.map(srgbDecode)));}
function display(rgb){const X=xyz(rgb);const s=mul([[3.1338561,-1.6168667,-.4906146],[-.9787684,1.9161415,.033454], [.0719453,-.2289914,1.4052427]],X);return s.map(v=>Math.max(0,Math.min(1,v<=.0031308?12.92*v:1.055*v**(1/2.4)-.055)));}
module.exports={xyz,input,display,decode,encode,mul,proToXYZ};
