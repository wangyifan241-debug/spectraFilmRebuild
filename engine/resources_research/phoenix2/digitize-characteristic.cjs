// Secondary-source candidate only: article says II, image caption says I.
const fs=require('fs'),z=require('zlib'),crypto=require('crypto'),path=require('path');
const root=__dirname,b=fs.readFileSync(path.join(root,'characteristic-jch.png'));
const sha=crypto.createHash('sha256').update(b).digest('hex');
let width,height,channels,palette,compressed=[];
for(let o=8;o<b.length;){const len=b.readUInt32BE(o),type=b.toString('ascii',o+4,o+8),d=b.subarray(o+8,o+8+len);
  if(type==='IHDR'){width=d.readUInt32BE();height=d.readUInt32BE(4);if(d[8]!==8||![2,3,6].includes(d[9])||d[12]!==0)throw Error('Unsupported PNG format');channels=d[9]===3?1:d[9]===2?3:4;}
  if(type==='PLTE')palette=d;
  if(type==='IDAT')compressed.push(d);o+=len+12;}
if(width!==492||height!==314)throw Error('Source image changed; audit calibration');
const scan=z.inflateSync(Buffer.concat(compressed)),stride=width*channels,pixels=Buffer.alloc(stride*height);
function paeth(a,b,c){const p=a+b-c,pa=Math.abs(p-a),pb=Math.abs(p-b),pc=Math.abs(p-c);return pa<=pb&&pa<=pc?a:pb<=pc?b:c;}
for(let y=0;y<height;y++){const filter=scan[y*(stride+1)];for(let x=0;x<stride;x++){
  const i=y*stride+x,a=x>=channels?pixels[i-channels]:0,up=y?pixels[i-stride]:0,c=y&&x>=channels?pixels[i-stride-channels]:0;
  const pred=[0,a,up,Math.floor((a+up)/2),paeth(a,up,c)][filter];if(pred===undefined)throw Error('PNG filter');
  pixels[i]=(scan[y*(stride+1)+1+x]+pred)&255;
}}
const names=['red','green','blue'],csv=['relative_log_exposure,red_density,green_density,blue_density,pixel_x,red_y,green_y,blue_y'];
const count={red:0,green:0,blue:0};
for(let x=86;x<=383;x++){
  const ys=names.map((c,k)=>{
    const candidates=[];
    for(let y=85;y<=210;y++){const i=(y*width+x)*channels,v=channels===1?[...palette.subarray(pixels[i]*3,pixels[i]*3+3)]:[...pixels.subarray(i,i+3)];
      if(v[k]-Math.max(...v.filter((_,j)=>j!==k))>18&&v[k]>100)candidates.push(y);
    }
    if(!candidates.length||candidates.at(-1)-candidates[0]>5)return null;
    count[c]++;return candidates.reduce((a,b)=>a+b,0)/candidates.length;
  });
  const fmt=n=>Number(n.toFixed(7));
  csv.push([fmt((x-85)*3/299),...ys.map(y=>y===null?'':fmt((237-y)*2/185)),x,...ys.map(y=>y??'')].join(','));
}
fs.writeFileSync(path.join(root,'characteristic-candidate.csv'),csv.join('\n')+'\n');
fs.writeFileSync(path.join(root,'characteristic-candidate.json'),JSON.stringify({
  format:'SpektraLab.PhoenixCharacteristicCandidate',version:1,profile_ready:false,verified_generation:false,
  source:{url:'https://www.japancamerahunter.com/wp-content/uploads/2025/07/Screenshot-2025-07-14-at-15.49.04.png',
    article:'https://www.japancamerahunter.com/2025/07/film-newfilm-news-harman-phoenix-ii-risess-harman-phoenix-rises-again/',date:'2025-07-16',sha256:sha,
    conflict:'Article presents Phoenix II; image caption explicitly says HARMAN Phoenix I film. Not authenticated as a manufacturer-issued II curve.'},
  calibration:{width,height,left:85,right:384,top:52,bottom:237,x_min:0,x_max:3,y_min:0,y_max:2},
  method:'Color-dominant source pixels; no smoothing or gap filling',rows:298,channel_samples:count,
  ordinate:'Density as printed; density standard unknown',abscissa:'Relative Log Exposure; absolute anchor unknown',
  uncertainty:'Source resolution about 0.0108 D per pixel, plus stroke and axis calibration uncertainty; not a measured accuracy bound',
  required_before_use:['Resolve generation/caption conflict using an independent primary source','Confirm densitometry and exposure conditions','Dye/base reconstruction and physical validation']
},null,2)+'\n');
console.log(JSON.stringify({rows:298,count,sha256:sha,verified_generation:false}));
