// Offline chart extraction; raw ordinate retained, not a runtime profile.
const fs=require('fs'),zlib=require('zlib'),crypto=require('crypto'),path=require('path');
const root=__dirname,b=fs.readFileSync(path.join(root,'source.pdf')),s=b.toString('latin1');
const sha=crypto.createHash('sha256').update(b).digest('hex');
if(sha!=='020260790a3b705d38ded5573abcb2af43b228b5e3440bc23463273cf78bff08')throw Error('Unreviewed PDF revision');
function raw(n){
  const re=new RegExp('(?:^|[\\r\\n])'+n+' 0 obj([\\s\\S]*?)endobj'),m=re.exec(s);
  if(!m)throw Error('Missing object');
  let o=m.index+m[0].indexOf('stream')+6;if(s[o]==='\r')o++;if(s[o]==='\n')o++;
  const bytes=b.subarray(o,o+Number(m[1].match(/\/Length (\d+)/)[1]));
  return m[1].includes('FlateDecode')?zlib.inflateSync(bytes):bytes;
}
const indexed=raw(20),palette=raw(21),rgb=Buffer.alloc(indexed.length*3);
if(indexed.length!==795*559||palette.length!==471)throw Error('Raster dimensions changed');
for(let i=0;i<indexed.length;i++)palette.copy(rgb,i*3,indexed[i]*3,indexed[i]*3+3);
function png(data,width,height){
  function chunk(type,bytes){const p=Buffer.concat([Buffer.from(type),bytes]);let crc=-1;
    for(const byte of p){crc^=byte;for(let i=0;i<8;i++)crc=(crc>>>1)^((crc&1)?0xedb88320:0);}
    const len=Buffer.alloc(4),sum=Buffer.alloc(4);len.writeUInt32BE(bytes.length);sum.writeUInt32BE((crc^-1)>>>0);return Buffer.concat([len,p,sum]);}
  const head=Buffer.alloc(13);head.writeUInt32BE(width);head.writeUInt32BE(height,4);head[8]=8;head[9]=2;
  const rows=[];for(let y=0;y<height;y++)rows.push(Buffer.from([0]),data.subarray(y*width*3,(y+1)*width*3));
  return Buffer.concat([Buffer.from('89504e470d0a1a0a','hex'),chunk('IHDR',head),chunk('IDAT',zlib.deflateSync(Buffer.concat(rows))),chunk('IEND',Buffer.alloc(0))]);
}
fs.writeFileSync(path.join(root,'spectral-source.png'),png(rgb,795,559));
if(process.argv.includes('--image-only'))process.exit(0);
// Manually audited guide points only select a branch; exported y is always detected ink.
const guides={
  blue:[[115,384],[130,361],[144,326],[155,284],[169,266],[200,253],[225,242],[238,232],[247,220],[260,239],[280,269],[300,298],[310,335],[320,387],[329,426]],
  green:[[275,416],[286,380],[300,330],[307,306],[315,280],[335,247],[352,231],[365,244],[375,278],[390,330],[404,369],[420,408],[436,447]],
  red:[[368,434],[385,407],[400,370],[410,349],[435,325],[455,304],[474,280],[491,240],[503,227],[513,240],[523,273],[531,316],[538,359],[545,394],[552,416],[560,438],[568,450]]
};
function expected(points,x){for(let i=1;i<points.length;i++)if(x>=points[i-1][0]&&x<=points[i][0]){const a=points[i-1],b=points[i];return a[1]+(b[1]-a[1])*(x-a[0])/(b[0]-a[0]);}return null;}
const names=['red','green','blue'],colors={red:[255,40,40],green:[0,180,70],blue:[20,100,255]};
const review=Buffer.from(rgb),rows=['wavelength_nm,red_chart_ordinate,green_chart_ordinate,blue_chart_ordinate,pixel_x,red_y,green_y,blue_y,red_status,green_status,blue_status'];
const counts={};for(const c of names)counts[c]={traced:0,ambiguous:0,missing:0};
for(let x=34;x<=703;x++){
  const guesses=names.map(c=>expected(guides[c],x));
  const values=names.map((c,i)=>{
    const guess=guesses[i];if(guess===null){counts[c].missing++;return {y:null,status:'outside_plotted_support'};}
    if(guesses.some((other,j)=>j!==i&&other!==null&&Math.abs(other-guess)<7)){counts[c].ambiguous++;return {y:null,status:'crossing_ambiguous'};}
    const runs=[];
    for(let y=210;y<=450;y++)if(Math.max(...rgb.subarray((y*795+x)*3,(y*795+x)*3+3))<65){const last=runs.at(-1);if(last&&y===last.at(-1)+1)last.push(y);else runs.push([y]);}
    // Steep spectral edges span many vertical pixels in one x column.
    const candidates=runs.filter(r=>r.length<=40).map(r=>r.reduce((a,b)=>a+b,0)/r.length).filter(y=>Math.abs(y-guess)<=24).sort((a,b)=>Math.abs(a-guess)-Math.abs(b-guess));
    if(!candidates.length){counts[c].missing++;return {y:null,status:'ink_not_resolved'};}
    const y=candidates[0];counts[c].traced++;
    const at=(Math.round(y)*795+x)*3;for(let j=0;j<3;j++)review[at+j]=colors[c][j];
    return {y,status:'traced'};
  });
  const fmt=n=>Number(n.toFixed(7));
  rows.push([fmt(300+(x-34)*500/669),...values.map(v=>v.y===null?'':fmt((452-v.y)*5/415)),x,...values.map(v=>v.y??''),...values.map(v=>v.status)].join(','));
}
fs.writeFileSync(path.join(root,'spectral-chart.csv'),rows.join('\n')+'\n');
fs.writeFileSync(path.join(root,'spectral-review.png'),png(review,795,559));
const metadata={format:'SpektraLab.PhoenixChartDigitization',version:1,stock:'HARMAN Phoenix II 200',profile_ready:false,
  source:{edition:'July 2025',author:'HARMAN technology Limited',sha256:sha,download_url:'https://www.fotoimpex.com/shop/images/products/media/72015_5_PDF-Datasheet.pdf',official_url:'https://www.harmanphoto.co.uk/amfile/file/download/file/1968/product/2208/'},
  spectral:{page:2,method:'native indexed raster, black-ink centers selected by audited branch guides; no output interpolation',width:795,height:559,rectangle:{left:34,right:703,top:37,bottom:452},black_threshold:65,max_ink_run_px:40,guide_corridor_px:24,crossing_exclusion_px:7,guides,counts,
    ordinate:'Relative Sensitivity as printed; linear/log meaning unresolved',illumination:'Wedge spectrogram to daylight (D55)',missing:'Empty fields are unknown, not zero. Trace endpoints are not proven physical cutoffs.',uncertainty:'Pixel and source stroke quantization, branch/crossing ambiguity; no certified measurement bound'},
  characteristic:{status:'not_present_in_downloaded_July_2025_datasheet',curves:null,absolute_exposure_anchor:null,density_standard:null},
  gates:{spectral_digitized:true,characteristic_digitized:false,physical_calibration:false,dye_base_reconstruction:false,independent_photographic_validation:false}};
fs.writeFileSync(path.join(root,'digitization.json'),JSON.stringify(metadata,null,2)+'\n');
console.log(JSON.stringify({rows:rows.length-1,counts,profile_ready:false}));
