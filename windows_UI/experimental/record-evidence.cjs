'use strict';
const fs=require('fs'),path=require('path'),crypto=require('crypto'),cp=require('child_process');
const [build,resources,normal,fixed,normalOut,fixedOut,output,raw]=process.argv.slice(2);
if(!output)throw Error('build resources normal-exe fixed-exe normal-output fixed-output report-json [raw]');
const sha=file=>crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const walk=dir=>fs.readdirSync(dir,{withFileTypes:true}).sort((a,b)=>a.name.localeCompare(b.name)).flatMap(e=>e.isDirectory()?walk(path.join(dir,e.name)):[path.join(dir,e.name)]);
const inventory=dir=>walk(dir).map(file=>({path:path.relative(dir,file).replaceAll('\\','/'),bytes:fs.statSync(file).size,sha256:sha(file)}));
const binaries=[path.join(build,'app/SpektraLabPhoenixPreview.exe'),path.join(build,'ui/spk_qt_phoenix_preview_test.exe'),normal,fixed];
for(const exe of [normal,fixed])binaries.push(...fs.readdirSync(path.dirname(exe)).filter(n=>n.endsWith('.dll')).map(n=>path.join(path.dirname(exe),n)));
const source=cp.execFileSync('git',['ls-files','--cached','--others','--exclude-standard'],{encoding:'utf8'}).trim().split('\n').filter(n=>/^(engine\/|windows_UI\/|\.gitattributes$)/.test(n)&&fs.existsSync(n)).sort();
const report={version:1,created_utc:new Date().toISOString(),git_head:cp.execFileSync('git',['rev-parse','HEAD'],{encoding:'utf8'}).trim(),
 tools:{node:process.version,qt:'6.11.3',compiler:'MinGW GCC 13.1.0',cmake:'3.25.0',libraw:'0.22.2'},
 binaries:binaries.map(file=>({name:path.basename(file),context:file===normal?'normal reference':file===fixed?'fixed reference':path.dirname(file).replaceAll('\\','/').split('/').slice(-2).join('/'),sha256:sha(file)})),
 resources:inventory(resources),source:source.map(file=>({path:file,sha256:sha(file)})),
 outputs:{normal:inventory(normalOut),fixed:inventory(fixedOut)},
 raw:raw?{bytes:fs.statSync(raw).size,sha256:sha(raw),publicly_distributed:false}:null,
 photographic_fidelity_validated:false,photographic_gate:'Not Verified'};
fs.writeFileSync(output,JSON.stringify(report,null,2)+'\n');
console.log('Recorded source, binaries, all resources, inputs and outputs; no photograph bytes copied');
