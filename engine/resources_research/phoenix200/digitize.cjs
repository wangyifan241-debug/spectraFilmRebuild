// Offline research only. No packages, network, runtime registration or spectral assumptions.
const fs = require('fs');
const zlib = require('zlib');
const crypto = require('crypto');
const path = require('path');
const root = __dirname;
const pdf = fs.readFileSync(path.join(root, 'source.pdf'));
const sha256 = crypto.createHash('sha256').update(pdf).digest('hex');
if (sha256 !== '9c5c63e8e0123f66edf0165a0246beb98e0e872ca2748b0f6267d33ae334472d')
  throw Error('Unsupported PDF revision: re-audit geometry before extraction');
const source = pdf.toString('latin1');
const streams = [];
let image;
for (const obj of source.matchAll(/(\d+) 0 obj\b([\s\S]*?)endobj/g)) {
  const marker = obj[0].indexOf('stream');
  if (marker < 0 || !obj[2].includes('/FlateDecode')) continue;
  const header = obj[0].slice(0, marker);
  const length = Number(header.match(/\/Length\s+(\d+)/)?.[1]);
  if (!length) continue;
  let offset = obj.index + marker + 6;
  if (source[offset] === '\r') offset++;
  if (source[offset] === '\n') offset++;
  const decoded = zlib.inflateSync(pdf.subarray(offset, offset + length));
  if (/\/Width 795\b/.test(header) && /\/Height 559\b/.test(header)) image = decoded;
  else streams.push(decoded.toString('latin1'));
}
if (!image || image.length !== 795 * 559 * 3) throw Error('Characteristic raster missing');
function png(rgb,width,height) {
  function chunk(type,data) {
    const payload=Buffer.concat([Buffer.from(type),data]);let crc=-1;
    for(const byte of payload){crc^=byte;for(let i=0;i<8;i++)crc=(crc>>>1)^((crc&1)?0xedb88320:0);}
    const length=Buffer.alloc(4),checksum=Buffer.alloc(4);
    length.writeUInt32BE(data.length);checksum.writeUInt32BE((crc^-1)>>>0);
    return Buffer.concat([length,payload,checksum]);
  }
  const header=Buffer.alloc(13);header.writeUInt32BE(width);header.writeUInt32BE(height,4);header[8]=8;header[9]=2;
  const rows=[];for(let y=0;y<height;y++)rows.push(Buffer.from([0]),rgb.subarray(y*width*3,(y+1)*width*3));
  return Buffer.concat([Buffer.from('89504e470d0a1a0a','hex'),chunk('IHDR',header),chunk('IDAT',zlib.deflateSync(Buffer.concat(rows))),chunk('IEND',Buffer.alloc(0))]);
}
fs.writeFileSync(path.join(root,'characteristic-source.png'),png(image,795,559));
const content = streams.find(s => s.includes('187.97 571.42 cm'));
if (!content) throw Error('Spectral vector paths missing');
const calibration = { x0: 68.168, x1: 307.77, y0: 571.42, y1: 719.49 };
const names = ['red', 'green', 'blue'];
const offsets = [187.97, 140.05, 96.92];
const bezier = (a,b,c,d,t) => (1-t)**3*a + 3*(1-t)**2*t*b + 3*(1-t)*t*t*c + t**3*d;
const curves = offsets.map((offset, index) => {
  const start = content.indexOf(`q 1 0 0 1 ${offset} 571.42 cm`);
  if (start < 0) throw Error('Missing spectral channel');
  const block = content.slice(start).split(/\r?\nS\r?\n/)[0];
  const segments = []; let previous = [0,0];
  for (const match of block.matchAll(/^([\d. -]+) c\s*$/gm)) {
    const values = match[1].trim().split(/\s+/).map(Number);
    if (values.length !== 6 || values[0] < previous[0] || values[2] < values[0] || values[4] < values[2])
      throw Error('Unsupported nonmonotonic vector segment');
    segments.push([...previous, ...values]); previous = values.slice(4);
  }
  if (segments.length < 10) throw Error('Incomplete curve');
  return {channel:names[index], offset, segments};
});
const spectral = ['wavelength_nm,red_chart_ordinate,green_chart_ordinate,blue_chart_ordinate'];
const round = n => Number(n.toFixed(7));
for (let nm = 300; nm <= 800; nm++) {
  const x = calibration.x0 + (nm - 300) / 500 * (calibration.x1-calibration.x0);
  const ordinates = curves.map(curve => {
    const local = x - curve.offset;
    const segment = curve.segments.find(s => local >= s[0] && local <= s[6]);
    if (!segment) return ''; // Absent plotted support is unknown, never zero-filled.
    let lo=0,hi=1;
    for(let i=0;i<48;i++){const t=(lo+hi)/2;if(bezier(segment[0],segment[2],segment[4],segment[6],t)<local)lo=t;else hi=t;}
    return round(bezier(segment[1],segment[3],segment[5],segment[7],(lo+hi)/2)*5/(calibration.y1-calibration.y0));
  });
  spectral.push([nm,...ordinates].join(','));
}
// Black curve ink; grey grid lines are excluded. Never extrapolate or smooth.
const black = (x,y) => {
  const i=(y*795+x)*3;
  return Math.max(image[i],image[i+1],image[i+2]) < 65;
};
const characteristic = ['relative_log_exposure,red_density,green_density,blue_density,pixel_x,green_y,blue_y,red_y,ink_run_count,status'];
const overlay = [];
let resolved=0, unresolved=0;
for(let x=35;x<=700;x++) {
  const groups=[];
  for(let y=145;y<=410;y++)if(black(x,y)) {
    const last=groups.at(-1);
    if(last && y===last.at(-1)+1)last.push(y);else groups.push([y]);
  }
  const centers=groups.filter(g=>g.length>=1 && g.length<=9).map(g=>g.reduce((a,b)=>a+b,0)/g.length);
  // Green remains the upper, separated trace. R/B identity only where three traces are resolved.
  if(centers.length!==2 && centers.length!==3) {
    characteristic.push([round((x-34)*3/(703-34)),'','','',x,'','','',centers.length,'unresolved_all'].join(','));
    unresolved++; continue;
  }
  // Small gaps in merged ink at the left are rasterization, not established channel identity.
  const green=centers[0], blue=centers.length===3&&x>=260?centers[1]:null, red=centers.length===3&&x>=260?centers[2]:null;
  const density=y=>y===null?'':round((452-y)*4/(452-36));
  characteristic.push([round((x-34)*3/669),density(red),density(green),density(blue),x,green,blue??'',red??'',centers.length,red===null?'red_blue_merged':'three_traces_resolved'].join(','));
  overlay.push({x,green,blue,red});
  if(red===null)unresolved++;else resolved++;
}
const metadata = {
  format:'SpektraLab.PhoenixChartDigitization',version:1,profile_ready:false,
  source:{author:'HARMAN technology Limited',edition:'November 2023',sha256,
    download_url:'https://www.revelab.es/fichas-carretes/harman-phoenix-200-datasheet.pdf'},
  spectral:{page:2,method:'PDF cubic Bezier centerlines, inversion by monotonic x, 1 nm sampling',calibration,
    channels:curves.map(c=>({channel:c.channel,offset_x:c.offset,segments:c.segments.length})),
    ordinate:'Relative Sensitivity as printed; linear/log meaning unresolved',
    illumination:'Wedge spectrogram to tungsten light (2856 K)',
    missing:'empty CSV field means outside plotted path; not zero sensitivity',
    uncertainty:'Path extraction removes tracing error, not source measurement or interpretation uncertainty'},
  characteristic:{page:3,method:'native 795x559 RGB image, black-ink vertical runs; no interpolation',
    rectangle:{left:34,right:703,top:36,bottom:452},threshold:65,
    density_standard:null,absolute_exposure_anchor:null,
    x_unit:'Relative Log Exposure as printed',y_unit:'Density as printed',process:'standard C41',
    fully_resolved_columns:resolved,partially_or_fully_unresolved_columns:unresolved,
    uncertainty:'At least pixel/line-width quantization; no certified measurement accuracy. R/B unavailable when merged.',
    identity:'Right-edge labels establish Green upper, Blue middle, Red lower; unresolved crossings not assigned'},
  gates:{digitized:true,physical_calibration:false,dye_base_reconstruction:false,independent_photographic_validation:false},
};
fs.writeFileSync(path.join(root,'spectral-chart.csv'),spectral.join('\n')+'\n');
fs.writeFileSync(path.join(root,'characteristic-chart.csv'),characteristic.join('\n')+'\n');
fs.writeFileSync(path.join(root,'digitization.json'),JSON.stringify(metadata,null,2)+'\n');
// A local review sheet uses only already-rendered source pages; not distributed.
const sourceImage=fs.readFileSync(path.join(root,'characteristic-source.png')).toString('base64');
const colors={green:'#00a050',blue:'#0070ff',red:'#ff3030'};
const review=Buffer.from(image);
for(const point of overlay)for(const c of Object.keys(colors))if(point[c]!==null) {
  const at=(Math.round(point[c])*795+point.x)*3;
  const color=colors[c].slice(1);for(let i=0;i<3;i++)review[at+i]=parseInt(color.slice(i*2,i*2+2),16);
}
fs.writeFileSync(path.join(root,'characteristic-review.png'),png(review,795,559));
const dots=overlay.flatMap(p=>Object.keys(colors).filter(c=>p[c]!==null).map(c=>`<circle cx="${p.x}" cy="${p[c]}" r="0.7" fill="${colors[c]}"/>`)).join('');
const paths=curves.map(c=>{
  let d=`M ${c.offset} ${calibration.y0}`;
  for(const s of c.segments)d+=` C ${s[2]+c.offset} ${s[3]+calibration.y0} ${s[4]+c.offset} ${s[5]+calibration.y0} ${s[6]+c.offset} ${s[7]+calibration.y0}`;
  return `<path d="${d}" fill="none" stroke="${colors[c.channel]}" stroke-width="0.35"/>`;
}).join('');
const page2=fs.readFileSync(path.join(root,'page-2.png')).toString('base64');
fs.writeFileSync(path.join(root,'digitization-review.html'),`<!doctype html><meta charset="utf-8"><title>Phoenix chart extraction review</title><h1>Source and extracted traces</h1><p>Colored points are extracted ink centers. Missing R/B points are intentional.</p><svg viewBox="0 0 795 559" width="1000"><image width="795" height="559" href="data:image/png;base64,${sourceImage}"/>${dots}</svg><p>Spectral vector paths over original page (no sensitivity-scale conversion).</p><svg viewBox="0 0 595.32 841.92" width="800"><image width="595.32" height="841.92" href="data:image/png;base64,${page2}"/><g transform="translate(0 841.92) scale(1 -1)">${paths}</g></svg>`);
console.log(JSON.stringify({spectral_rows:spectral.length-1,characteristic_rows:characteristic.length-1,resolved,unresolved,profile_ready:false}));
