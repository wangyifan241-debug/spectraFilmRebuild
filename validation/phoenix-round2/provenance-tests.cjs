'use strict';
const t=require('node:test'),a=require('node:assert/strict'),fs=require('fs'),crypto=require('crypto');
t('inventory excludes itself and matches every current byte',()=>{const d=__dirname+'/',m=JSON.parse(fs.readFileSync(d+'execution-inventory.json'));a.ok(!m.files.some(x=>x.path==='execution-inventory.json'));for(const x of m.files)a.equal(crypto.createHash('sha256').update(fs.readFileSync(d+x.path)).digest('hex'),x.sha256,x.path);});
t('installed header identity matches regenerated provenance',()=>{const p=JSON.parse(fs.readFileSync(__dirname+'/clean-build-provenance.json'));a.deepEqual(p.vulkan_headers,require('./toolchain-provenance.cjs').vulkanHeaders());a.ok(!('vulkan_headers_commit' in p));});
