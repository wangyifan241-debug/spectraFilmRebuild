"""Analyze actual research-engine outputs, not film accuracy."""
import sys,json,hashlib
from pathlib import Path
import numpy as np
from PIL import Image
from skimage.color import deltaE_ciede2000
root=Path(sys.argv[1]); out=root/'analysis';out.mkdir(exist_ok=True)
M=np.array([[.4124564,.3575761,.1804375],[.2126729,.7151522,.072175],[.0193339,.119192,.9503041]])
CAT=np.array([[1.0478112,.0228866,-.050127],[.0295424,.9904844,-.0170491],[-.0092345,.0150436,.7521316]])
def lab(a):
 x=np.where(a<=.04045,a/12.92,((a+.055)/1.055)**2.4)@M.T@CAT.T/np.array([.96422,1,.82521]); f=np.where(x>216/24389,np.cbrt(x),(24389/27*x+16)/116)
 return np.stack([116*f[...,1]-16,500*(f[...,0]-f[...,1]),200*(f[...,1]-f[...,2])],axis=-1)
def read(p):return np.asarray(Image.open(p).convert('RGB'),dtype=float)/255
modes={'A':'round5-scan','B':'B-affine-matrix','C':'C-inverse-identity','D':'D-affine-identity'}
if (root/'harman_phoenix_200_exp_v1-E-device.png').exists():modes['E']='E-device'
report={'classification':'factorial software diagnostic; NOT real-film accuracy; identity matrix cells NOT proposed improvements','comparisons':{},'stages':{}}
images={}
for stock in ['harman_phoenix_200_exp_v1','harman_phoenix_ii_200_exp_v1']:
 for mode,suffix in modes.items():images[stock+'-'+mode]=read(root/(stock+'-'+suffix+'.png'))
 for p in root.glob(stock+'*.f32'):
  v=np.fromfile(p,dtype='<f4').reshape(-1,3); report['stages'][p.stem]={'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'nonfinite':int((~np.isfinite(v)).sum()),'outside_0_1_fraction':float(((v<0)|(v>1)).mean()),'channel_min':v.min(0).tolist(),'channel_max':v.max(0).tolist()}
  if v.shape[0]==256*96:
   ramp=v.reshape(96,256,3)[0];np.savetxt(out/(p.stem+'-neutral-response.csv'),ramp,delimiter=',',header='R,G,B')
 for mode in modes:
  name=stock+'-'+mode;a=images[name];b=read(root/'digital-input.png');de=deltaE_ciede2000(lab(a),lab(b)); diff=np.abs(a-b)
  report['comparisons'][name+'-vs-digital']={'RGB_MAE_255':float(diff.mean()*255),'DE00_mean':float(de.mean()),'DE00_p95':float(np.percentile(de,95)),'display_endpoint_fraction':float(((a==0)|(a==1)).mean())}
  Image.fromarray(np.uint8(np.clip(diff*10*255,0,255))).save(out/(name+'-diff10x.png'))
 for mode in ['B','C','D']:
  a=images[stock+'-'+mode];b=images[stock+'-A'];de=deltaE_ciede2000(lab(a),lab(b));report['comparisons'][stock+'-'+mode+'-vs-A']={'RGB_MAE_255':float(np.abs(a-b).mean()*255),'DE00_mean':float(de.mean()),'DE00_p95':float(np.percentile(de,95))}
for mode in modes:
 a=images['harman_phoenix_200_exp_v1-'+mode];b=images['harman_phoenix_ii_200_exp_v1-'+mode];de=deltaE_ciede2000(lab(a),lab(b));report['comparisons']['II-vs-I-'+mode]={'RGB_MAE_255':float(np.abs(a-b).mean()*255),'DE00_mean':float(de.mean()),'DE00_p95':float(np.percentile(de,95))}
 Image.fromarray(np.uint8(np.clip(np.abs(a-b)*2550,0,255))).save(out/('II-vs-I-'+mode+'-diff10x.png'))
(out/'metrics.json').write_text(json.dumps(report,indent=2))
(out/'index.html').write_text('<meta charset="utf-8"><h1>Scan ablation — diagnostic only</h1><p>A complete; B affine neutral + fitted matrix; C full inverse + identity; D affine + identity. Identity is not device calibration.</p>'+''.join('<figure><figcaption>'+n+'</figcaption><img width="600" src="../'+n.replace('-A','-round5-scan').replace('-B','-B-affine-matrix').replace('-C','-C-inverse-identity').replace('-D','-D-affine-identity')+'.png"></figure>' for n in images)+'<pre>'+json.dumps(report['comparisons'],indent=2)+'</pre>')
print(json.dumps(report['comparisons'],indent=2))
