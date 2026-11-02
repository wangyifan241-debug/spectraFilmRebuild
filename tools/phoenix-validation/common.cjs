'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
const BASE='ba076b2befe6586ad3095f897ebf40ea7e9ffd6c';
function requireThat(ok,message){if(!ok)throw Error(message);}
function hash(bytes){return crypto.createHash('sha256').update(bytes).digest('hex');}
function fileHash(file){return hash(fs.readFileSync(file));}
function frozenMatch(file,expected){const bytes=fs.readFileSync(file);if(hash(bytes)===expected)return 'exact';if(/\.(json|txt|md|cjs)$/.test(file)&&hash(Buffer.from(bytes.toString('latin1').replace(/\r\n/g,'\n'),'latin1'))===expected)return 'CRLF checkout of exact LF Git blob';return null;}
function json(file){return JSON.parse(fs.readFileSync(file,'utf8').replace(/^\uFEFF/,''));}
function finite(x){return typeof x==='number'&&Number.isFinite(x);}
function text(x){return typeof x==='string'&&x.trim().length>0;}
function digest(x){return typeof x==='string'&&/^[a-f0-9]{64}$/.test(x);}
function inside(root,relative){requireThat(text(relative)&&!path.isAbsolute(relative),'Expected relative artifact path');const abs=path.resolve(root,relative);requireThat(abs.startsWith(path.resolve(root)+path.sep),'Artifact escapes root');if(fs.existsSync(abs)){const real=fs.realpathSync(abs),base=fs.realpathSync(root);requireThat(real.startsWith(base+path.sep),'Symlink escapes artifact root');}return abs;}
function binding(root,asset){
 if(!asset||!text(asset.path)||!digest(asset.sha256))return {ok:false,reason:'Missing artifact path/SHA256'};
 const file=inside(root,asset.path);if(!fs.existsSync(file))return {ok:false,reason:'Missing file: '+asset.path};
 if(fileHash(file)!==asset.sha256)return {ok:false,reason:'SHA256 mismatch: '+asset.path};return {ok:true,file};
}
function notVerified(reason){return {status:'Not Verified',reason,metrics:null};}
function stable(value){if(Array.isArray(value))return value.map(stable);if(value&&typeof value==='object')return Object.fromEntries(Object.keys(value).sort().map(k=>[k,stable(value[k])]));return value;}
function semanticHash(value){return hash(JSON.stringify(stable(value)));}
function writeJson(file,value){fs.writeFileSync(file,JSON.stringify(value,null,2)+'\n',{flag:'wx'});}
function escapeHtml(value){return String(value).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));}
module.exports={BASE,fs,path,requireThat,hash,fileHash,frozenMatch,json,finite,text,digest,inside,binding,notVerified,stable,semanticHash,writeJson,escapeHtml};
