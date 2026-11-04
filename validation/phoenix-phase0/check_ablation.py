import hashlib,json
from pathlib import Path
checks=[]
for baseline,candidate in [('phoenix-phase0-synth-02','phoenix-phase0-ablation-synth'),('phoenix-phase0-raw-02','phoenix-phase0-ablation-raw')]:
 for stock in ['harman_phoenix_200_exp_v1','harman_phoenix_ii_200_exp_v1']:
  name=stock+'-scan-linear-prophoto.f32'; a=(Path('build')/baseline/name).read_bytes(); b=(Path('build')/candidate/name).read_bytes(); assert a==b,(baseline,stock,'baseline changed')
  for suffix in ['B-affine-matrix','C-inverse-identity','D-affine-identity']:
   v=(Path('build')/candidate/(stock+'-'+suffix+'.f32')).read_bytes();assert v!=b,(candidate,stock,suffix,'insensitive ablation')
  checks.append({'input':baseline,'film':stock,'baseline_exact':True,'ablation_sensitivity':True,'sha256':hashlib.sha256(b).hexdigest()})
Path('validation/phoenix-phase0/ablation-regression.json').write_text(json.dumps({'status':'PASS','scope':'exact A baseline preservation and branch sensitivity only; not UI race or real-film accuracy','checks':checks},indent=2))
print('PASS four baseline/film cases; twelve ablation sensitivity checks')
