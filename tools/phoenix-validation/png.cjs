'use strict';
 const z=require('node:zlib');const {requireThat,hash}=require('./common.cjs');
const crcTable=Array.from({length:256},(_,n)=>{for(let i=0;i<8;i++)n=n&1?(n>>>1)^0xedb88320:n>>>1;return n>>>0;});
function crc32(b){let n=0xffffffff;for(const v of b)n=crcTable[(n^v)&255]^(n>>>8);return (n^0xffffffff)>>>0;}
function decode(bytes){
 requireThat(bytes.subarray(0,8).equals(Buffer.from('89504e470d0a1a0a','hex')),'Not PNG');let width,height,depth,type,palette,icc=null,srgb=false,idat=[],transparent=false;const colorChunks=[];
 for(let at=8;at<bytes.length;){requireThat(at+12<=bytes.length,'Truncated PNG');const len=bytes.readUInt32BE(at),name=bytes.toString('ascii',at+4,at+8);requireThat(at+12+len<=bytes.length,'Bad PNG chunk');const b=bytes.subarray(at+8,at+8+len);
  requireThat(crc32(bytes.subarray(at+4,at+8+len))===bytes.readUInt32BE(at+8+len),'PNG CRC mismatch');requireThat(name!=='acTL','Animated PNG is not a fixed scan');
  if(name==='IHDR'){width=b.readUInt32BE();height=b.readUInt32BE(4);depth=b[8];type=b[9];requireThat(b[10]===0&&b[11]===0&&b[12]===0,'Unsupported/interlaced PNG');}
  if(name==='IDAT')idat.push(b);if(name==='PLTE')palette=b;if(name==='sRGB')srgb=true;
  if(name==='tRNS')transparent=true;
  if(['sRGB','gAMA','cHRM','cICP','iCCP'].includes(name))colorChunks.push(name+':'+hash(b));
  if(name==='iCCP'){const end=b.indexOf(0);requireThat(end>0&&b[end+1]===0,'Bad ICC chunk');icc=z.inflateSync(b.subarray(end+2),{maxOutputLength:4*1024*1024});}
  at+=len+12;
 }
 requireThat(width>0&&height>0&&width*height<=20000000&&[8,16].includes(depth)&&[0,2,3,4,6].includes(type)&&!(type===3&&depth!==8),'Unsupported PNG dimensions/depth/type');
 const channels={0:1,2:3,3:1,4:2,6:4}[type],bpp=channels*depth/8,stride=width*bpp,expected=(stride+1)*height;
 const packed=z.inflateSync(Buffer.concat(idat),{maxOutputLength:expected});requireThat(packed.length===expected,'PNG payload length mismatch');const raw=Buffer.alloc(stride*height);
 const paeth=(a,b,c)=>{const p=a+b-c,da=Math.abs(p-a),db=Math.abs(p-b),dc=Math.abs(p-c);return da<=db&&da<=dc?a:db<=dc?b:c;};
 for(let y=0;y<height;y++)for(let x=0;x<stride;x++){const filter=packed[y*(stride+1)],i=y*stride+x,a=x>=bpp?raw[i-bpp]:0,b=y?raw[i-stride]:0,c=y&&x>=bpp?raw[i-stride-bpp]:0;const pred=[0,a,b,Math.floor((a+b)/2),paeth(a,b,c)][filter];requireThat(pred!==undefined,'Invalid PNG filter');raw[i]=(packed[y*(stride+1)+1+x]+pred)&255;}
 const max=depth===16?65535:255,pixels=new Float64Array(width*height*3);let opaque=!transparent;
 const get=(p,c)=>depth===16?raw.readUInt16BE((p*channels+c)*2):raw[p*channels+c];
 for(let p=0;p<width*height;p++){let rgb;if(type===3){const i=get(p,0)*3;requireThat(palette&&i+3<=palette.length,'Invalid PNG palette');rgb=[...palette.subarray(i,i+3)].map(v=>v/255);}else if(type===0||type===4)rgb=Array(3).fill(get(p,0)/max);else rgb=[0,1,2].map(c=>get(p,c)/max);if(type===4||type===6)opaque&&=get(p,channels-1)===max;pixels.set(rgb,p*3);}
 return {width,height,depth,pixels,opaque,icc_sha256:icc?hash(icc):null,srgb,color_contract:colorChunks.sort().join('|')};
}
function compare(first,second){const a=decode(first),b=decode(second);requireThat(a.width===b.width&&a.height===b.height,'Fixed-render dimensions differ');requireThat(a.opaque&&b.opaque&&a.icc_sha256===b.icc_sha256&&a.srgb===b.srgb&&a.color_contract===b.color_contract,'Fixed-render alpha/ICC contract differs');let sum=0,square=0,max=0;for(let i=0;i<a.pixels.length;i++){const d=Math.abs(a.pixels[i]-b.pixels[i]);sum+=d;square+=d*d;max=Math.max(max,d);}return {purpose:'engineering_encoded_RGB_difference_not_photographic_accuracy',width:a.width,height:a.height,identical_bytes:first.equals(second),mae_encoded_RGB:sum/a.pixels.length,rmse_encoded_RGB:Math.sqrt(square/a.pixels.length),max_encoded_RGB:max};}
module.exports={decode,compare,crc32};
