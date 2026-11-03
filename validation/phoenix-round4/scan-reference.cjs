const fs = require('node:fs');
const assert = require('node:assert/strict');
function scan(p) {
  const n=p.density?.length;
  if (!(n>=2) || !(p.densityRange>0) || !Number.isFinite(p.densityRange) || !(p.wavelengthStepNm>0) || !Number.isFinite(p.wavelengthStepNm) || typeof p.detectorSpace!=='string' || !p.detectorSpace.trim()) throw Error('invalid recipe');
  const arrays=[p.density,p.base,p.illuminant,...(p.detectors||[])];
  if(p.detectors?.length!==3 || arrays.some(a=>!Array.isArray(a)||a.length!==n||a.some(x=>!Number.isFinite(x)))) throw Error('invalid spectral arrays');
  if(p.illuminant.some(x=>x<0)||p.detectors.some(a=>a.some(x=>x<0))) throw Error('negative instrument response');
  const transmission=p.density.map(d=>10**(-d));
  const relative=p.density.map((d,i)=>10**(-(d-p.base[i])));
  const channel=p.detectors.map(s=>{
    let white=0, signal=0;
    for(let i=0;i<n;i++) {const w=(i===0||i===n-1?0.5:1)*p.wavelengthStepNm*p.illuminant[i]*s[i]; white+=w; signal+=w*relative[i];}
    if(!(white>0)||!Number.isFinite(signal)) throw Error('invalid integration');
    const t=signal/white;
    if(!(t>0)) throw Error('transmission underflow');
    return -Math.log10(t)/p.densityRange;
  });
  if(transmission.some(x=>!Number.isFinite(x))) throw Error('transmission overflow');
  return {status:'IDEALIZED_PROXY_NOT_VERIFIED',detectorSpace:p.detectorSpace,transmission,relativeTransmission:relative,positiveProxy:channel,clippedChannelFraction:channel.filter(x=>x<0||x>1).length/3,recipe:p};
}
module.exports={scan};
if(require.main===module) {
 if(process.argv[2]==='--test') {
  const p={density:[1,1,1],base:[0,0,0],illuminant:[1,1,1],detectors:[[1,0,0],[0,1,0],[0,0,1]],densityRange:2,wavelengthStepNm:5,detectorSpace:'synthetic test instrument'};
  assert.deepEqual(scan(p).positiveProxy,[0.5,0.5,0.5]);
  assert.deepEqual(scan({...p,density:[2,2,2],base:[1,1,1]}).positiveProxy,[0.5,0.5,0.5]);
  assert.deepEqual(scan({...p,illuminant:[2,2,2]}).positiveProxy,scan(p).positiveProxy);
  assert.throws(()=>scan({...p,base:[0]}));
  assert.throws(()=>scan({...p,density:[NaN,1,1]}));
  assert.throws(()=>scan({...p,detectors:[[0,0,0],...p.detectors.slice(1)]}));
  assert.equal(scan({...p,density:[3,3,3]}).clippedChannelFraction,1);
  console.log('7 spectral proxy checks passed; synthetic inputs only');
 } else console.log(JSON.stringify(scan(JSON.parse(fs.readFileSync(process.argv[2],'utf8'))),null,2));
}
