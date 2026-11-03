'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
const entries=[
 {id:'train',author:'Felipe Tofani',film:'Phoenix 200',camera:'Mamiya RZ67',processing:'Negative Lab Pro; development/scanner unknown',title:'Time_to_take_the_train_-_Flickr_-_ftrc.jpg',original:'https://flickr.com/photos/81035303@N00/54723280356',url:'https://upload.wikimedia.org/wikipedia/commons/3/36/Time_to_take_the_train_-_Flickr_-_ftrc.jpg'},
 {id:'flight',author:'Mario A. P.',film:'Phoenix II 200',camera:null,processing:'Author says unretouched; development/scanner unknown',title:'Lone_flight_-_Flickr_-_Mario_A._P..jpg',original:'https://flickr.com/photos/99058495@N00/55264341397',url:'https://upload.wikimedia.org/wikipedia/commons/1/17/Lone_flight_-_Flickr_-_Mario_A._P..jpg'}
];
async function main(){
 const root=path.resolve(process.argv[2]||'build/round5-references');fs.mkdirSync(root,{recursive:true});
 for(const e of entries){
  e.license='CC-BY-SA-4.0';e.license_url='https://creativecommons.org/licenses/by-sa/4.0/';
  e.page='https://commons.wikimedia.org/wiki/File:'+e.title;
  e.classification='inverted display JPEG; not linear RAW or original negative';
  e.use='unpaired qualitative style only; no pixel/DeltaE fidelity metric';
  e.distribution='separate share-alike collection; images not committed';
  if(process.argv.includes('--metadata-only')){e.sha256=null;e.acquisition='not downloaded: connection timeout';continue;}
  const file=path.join(root,e.id+'.jpg');
  if(!fs.existsSync(file)){
   const response=await fetch(e.url,{headers:{'User-Agent':'SpektraLabResearch/0.1'},signal:AbortSignal.timeout(60000)});
   if(!response.ok)throw Error('HTTP '+response.status);
   const bytes=Buffer.from(await response.arrayBuffer());if(bytes[0]!==255||bytes[1]!==216)throw Error('Not JPEG');
   fs.writeFileSync(file,bytes,{flag:'wx'});
  }
  e.sha256=crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
  e.attribution=e.title+' — '+e.author+'; '+e.original+'; '+e.license_url+'; unchanged original upload';
 }
 fs.writeFileSync(path.join(root,'manifest.json'),JSON.stringify(entries,null,2));
 console.log((process.argv.includes('--metadata-only')?'Metadata only; originals NOT downloaded: ':'Verified licensed uploads: ')+root);
}
main().catch(e=>{console.error(e);process.exitCode=1;});
