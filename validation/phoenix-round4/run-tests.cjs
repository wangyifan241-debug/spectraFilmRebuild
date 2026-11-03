'use strict';
const fs=require('node:fs'),path=require('node:path'),cp=require('node:child_process'),assert=require('node:assert/strict');
const {inspect}=require('./inspect.cjs'),{verify}=require('./verify-exports.cjs');
const base=fs.mkdtempSync(path.join(path.resolve(process.argv[4]),'round4-ctest-')),out=path.join(base,'synthetic');
const run=cp.spawnSync(path.resolve(process.argv[2]),['--render',path.resolve(process.argv[3]),'synthetic',out],{encoding:'utf8',timeout:180000});fs.writeFileSync(path.join(base,'render.log'),(run.stdout||'')+(run.stderr||''));assert.equal(run.status,0,run.stderr||run.stdout);
const metrics=inspect(out),exportsResult=verify(out);assert.throws(()=>inspect(out,true),/Beer-Lambert/);fs.writeFileSync(path.join(base,'verification.json'),JSON.stringify({metrics,exportsResult,counterfactual:'EXPECTED_FAILURE'},null,2));console.log('PASS independent spectral oracle, neutral reversal, original C ABI control, PNG/TIFF16 exact channels and ICC; artifacts '+base);
