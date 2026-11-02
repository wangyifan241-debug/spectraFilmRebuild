'use strict';
const fs=require('fs'),cp=require('child_process'),path=require('path'),assert=require('assert/strict'),c=require('./config.cjs');
// Reuse reviewed tracing, but generate/compile ONLY a research pipeline object.
const old=process.env.PHOENIX_TRACE_BUILD;process.env.PHOENIX_TRACE_BUILD=c.paths.fixed;
delete require.cache[require.resolve('./config.cjs')];require('./build-trace24.cjs');
if(old===undefined)delete process.env.PHOENIX_TRACE_BUILD;else process.env.PHOENIX_TRACE_BUILD=old;
const out=c.paths.fixed,b=c.paths.clean,p=path.join(out,'pipeline-trace.cpp');let s=fs.readFileSync(p,'utf8');
const needle='{ float cc[243],bb[81],ii[243];';assert.equal(s.split(needle).length,2,'instrumentation insertion must be unique');
const hook=`{ const char* frozen = std::getenv("SPK_RESEARCH_FIXED_PRINT_GAIN"); if(frozen) { double g[3]; char tail; if(std::sscanf(frozen,"%lf %lf %lf %c", &g[0],&g[1],&g[2],&tail)!=3) { error="invalid research frozen gain"; return false; } for(int c=0;c<3;++c) { if(!std::isfinite(g[c]) || !(g[c]>0)) { error="invalid research frozen gain"; return false; } print_gain_[c]=g[c]; } } } `;
s='#include <cstdlib>\n#include <cstdio>\n'+s.replace(needle,hook+needle);fs.writeFileSync(p,s);
function run(args){const r=cp.spawnSync(c.compiler(),args,{cwd:b,encoding:'utf8',env:c.toolEnvironment()});if(r.status!==0)throw Error(r.stderr||r.error?.message||'compiler failed');}
run(['-O2','-std=gnu++20','-I'+c.root+'/engine/src/pipeline','@'+b+'/CMakeFiles/spektralab_pipeline.dir/includes_CXX.rsp','-c',p,'-o',out+'/pipeline.o']);
const objects=fs.readFileSync(b+'/CMakeFiles/spektralab_engine_shared.dir/objects1','utf8').trim().split(/\s+/).map(x=>x.includes('/pipeline.cpp.obj')?out+'/pipeline.o':x);
run([c.root+'/engine/windows_exports.def','-shared',...objects,'@CMakeFiles/spektralab_engine_shared.dir/linkLibs.rsp','-o',out+'/spektrafilm_engine.dll']);console.log('isolated fixed-gain diagnostic built');
