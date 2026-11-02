'use strict';
const fs=require('fs'),path=require('path');
const root=path.resolve(__dirname,'../..');
const json=process.env.PHOENIX_CONFIG?JSON.parse(fs.readFileSync(process.env.PHOENIX_CONFIG,'utf8')):{};
function setting(name,fallback){return process.env['PHOENIX_'+name]||json[name.toLowerCase()]||fallback;}
function absolute(name,fallback){const p=setting(name,fallback);if(!p)throw Error('Configure PHOENIX_'+name+' or '+name.toLowerCase()+' in PHOENIX_CONFIG JSON');return path.resolve(root,p);}
exports.root=root;
exports.paths={clean:absolute('CLEAN_BUILD','build/review-clean/engine'),trace:absolute('TRACE_BUILD','build/round3-trace'),fixed:absolute('FIXED_BUILD','build/round3-fixed-gain'),round2:absolute('ROUND2_BUILD','build/round2'),run:absolute('RUN_BUILD','build/round3-run'),ui:absolute('UI_BUILD','build/round3-ui'),result:absolute('RESULT_DIR','validation/phoenix-round3/review-hardening')};
exports.compiler=()=>absolute('CXX');exports.qtCore=()=>absolute('QT_CORE');exports.librawHeader=()=>absolute('LIBRAW_HEADER');
exports.toolEnvironment=function(){const bin=path.dirname(exports.compiler());return {...process.env,PATH:bin+path.delimiter+process.env.PATH};};
exports.setting=setting;
