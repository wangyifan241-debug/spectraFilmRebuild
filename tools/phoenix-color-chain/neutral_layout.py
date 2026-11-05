"""Quantify calibration layout sensitivity without changing a color model."""
from pathlib import Path
import json,numpy as np,hashlib,shutil
p=Path('build/phoenix-color-chain-layout-run1');out=Path('validation/phoenix-color-chain/neutral-layout');out.mkdir(exist_ok=True)
r=json.loads((p/'report.json').read_text());result={'provenance':r['provenance'],'classification':'reference acquisition diagnostic only; no candidate','stocks':{}}
for stock in ['phoenix_i','phoenix_ii']:
 def data(prefix):return np.fromfile(p/(prefix+stock+'.f32'),dtype='<f4').reshape(97,3).astype(float)
 a=data('neutral_device_transmission_');b=data('permuted_neutral_restored_transmission_');u=data('uniform_neutral_transmission_');positive=data('uniform_neutral_through_frozen_positive_');expected=.184*2**(np.arange(97)*.125-6);w=np.array([.2880402,.7118741,.0000857])
 result['stocks'][stock]={'controls':r['neutral_layout_controls_'+stock],'max_permutation_relative_transmission_change':float(np.max(np.abs(b/a-1))),'max_ordered_vs_uniform_relative_transmission_change':float(np.max(np.abs(u/a-1))),'uniform_gray_through_frozen_positive_max_relative_spread':float(np.max(np.ptp(positive,axis=1)/positive.mean(1))),'uniform_gray_through_frozen_positive_max_relative_Y_error':float(np.max(np.abs((positive@w)/expected-1))),'ordered_EV0_transmission':a[48].tolist(),'uniform_EV0_transmission':u[48].tolist()}
 for prefix in ['neutral_device_transmission_','permuted_neutral_restored_transmission_','uniform_neutral_transmission_','uniform_neutral_through_frozen_positive_']:shutil.copy2(p/(prefix+stock+'.f32'),out/(prefix+stock+'.f32'))
 # The observer calls must not change the original color-chain probe's density data.
 prior=Path('build/phoenix-color-chain-run1/synthetic-local')/('negative_density_'+stock+'.f32');current=p/prior.name
 assert prior.read_bytes()==current.read_bytes()
 result['stocks'][stock]['negative_density_local_probe_unchanged']=True
(out/'metrics.json').write_text(json.dumps(result,indent=2),encoding='utf-8');shutil.copy2(p/'report.json',out/'report.json')
print(json.dumps(result['stocks'],indent=2))
