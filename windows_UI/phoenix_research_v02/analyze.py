"""One v0.2 experiment. Retain original float data and separate hue domains."""
from pathlib import Path
import json,hashlib,sys,html,shutil
import numpy as np
from PIL import Image,ImageDraw,ImageOps
from skimage.color import rgb2lab,xyz2lab,deltaE_ciede2000
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import tifffile

root=Path(sys.argv[1]);old=Path('build/phoenix-phase3-v01-run1')
reports={n:json.loads((root/n/'report.json').read_text()) for n in ['synthetic','raw']}
checks=[];metrics={};regions={}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def check(n,p,d):checks.append(dict(name=n,passed=bool(p),details=d))
def floats(folder,n):
 r=json.loads((folder/'report.json').read_text());q=r[n]
 return np.fromfile(folder/(n+'.f32'),dtype='<f4').reshape(q['height'],q['width'],3).astype(float)
def rgb(folder,n):return np.asarray(Image.open(folder/(n+'.png')).convert('RGB'))/255.
def hue_stats(lab):
 c=np.linalg.norm(lab[...,1:],axis=-1);h=np.degrees(np.arctan2(lab[...,2],lab[...,1]));d=np.abs((np.diff(h,axis=1)+180)%360-180);mask=(c[:,:-1]>2)&(c[:,1:]>2)
 return {'max_adjacent_hue_degrees':float(d[mask].max()) if mask.any() else 0,'eligible_pairs':int(mask.sum()),'minimum_chroma':2}
ROIS={'face':[.28,.25,.43,.36],'arm':[.28,.56,.34,.62],'white_uniform':[.72,.43,.86,.60],'dark_clothing':[.26,.85,.34,.94],'background':[.04,.10,.10,.30]}
def patch(a,box):
 h,w=a.shape[:2];x0,y0,x1,y1=box;return a[int(h*y0):int(h*y1),int(w*x0):int(w*x1)]

for n,r in reports.items():
 check(n+' shader domains',r['shader_domain_probe']['pass'],r['shader_domain_probe'])
 check(n+' device heldout',r['device']['heldout_pass'],{k:r['device'][k] for k in ['heldout_XYZ_MAE','heldout_XYZ_max_error']})
 prior=json.loads((old/n/'report.json').read_text())
 check(n+' unchanged device calibration',r['device']==prior['device'],{'exact_device_recipe':r['device']==prior['device']})

for stock in ['phoenix_i','phoenix_ii']:
 name='research_'+stock
 matrix=np.array(json.loads((root/'raw'/('legacy_recipe_'+stock+'.json')).read_text())['linear_prophoto_to_XYZ_D50']).reshape(3,3)
 a=floats(root/'synthetic',name);y=a@np.array([.2880402,.7118741,.0000857]);dy=np.diff(y,axis=1)
 neutral=np.ptp(a[0],axis=-1)/np.maximum(a[0].mean(-1),1e-9)
 check(stock+' neutral',neutral.max()<.005,{'max_relative_channel_spread':float(neutral.max())})
 check(stock+' all eight exposure ramps monotonic',dy.min()>=-1e-5,{'min_Y_step':float(dy.min()),'per_row_minimum_Y_step':dy.min(1).tolist()})
 curvature=np.abs(np.diff(dy,axis=1))/np.maximum(.184,np.abs(y[:,1:-1]))
 check(stock+' continuity',curvature.max()<.25,{'max_normalized_second_difference':float(curvature.max())})
 for domain,lab in [('linear_float32_ProPhoto_to_Lab_D50',xyz2lab(a[4:]@matrix.T,illuminant='D50')),('RGB8_sRGB_to_Lab_D65',rgb2lab(rgb(root/'synthetic',name)[4:]))]:
  stats=hue_stats(lab);check(stock+' skin hue '+domain,stats['max_adjacent_hue_degrees']<15,stats)
  old_array=floats(old/'synthetic',name) if domain.startswith('linear') else rgb(old/'synthetic',name)
  old_lab=xyz2lab(old_array[4:]@matrix.T,illuminant='D50') if domain.startswith('linear') else rgb2lab(old_array[4:])
  metrics[stock+' v01 hue '+domain]=hue_stats(old_lab)
 # Same exported display samples at 16 bits: diagnostic only; never replaces RGB8 gate.
 display16=tifffile.imread(root/'synthetic'/('research_'+stock+'.tif'))[4:,:,:3]/65535.
 metrics[stock+' v02 hue RGB16_sRGB_to_Lab_D65']=hue_stats(rgb2lab(display16))
 for kind in ['synthetic','raw']:
  folder=root/kind;r=reports[kind];v=floats(folder,name)
  check(kind+' '+stock+' finite',np.isfinite(v).all(),{'nonfinite':int((~np.isfinite(v)).sum()),'negative_channels':int((v<0).sum()),'headroom_channels_above_one':int((v>1).sum())})
  bounds=r['v02_constraints_'+stock];check(kind+' '+stock+' Y normalization',bounds['Y_constraint_pass'],bounds);check(kind+' '+stock+' chromatic bound',bounds['ratio_bound_pass'],bounds)
  trans=floats(folder,'device_transmission_'+stock);neutral_trans=floats(folder,'neutral_device_transmission_'+stock)[0];weights=np.array([.2880402,.7118741,.0000857]);ty=trans@weights;ny=neutral_trans@weights
  metrics[kind+' '+stock+' tone endpoints']={'below_neutral_exposure_floor_pixels':int((ty>=ny[0]).sum()),'above_neutral_exposure_ceiling_pixels':int((ty<=ny[-1]).sum()),'pixels':int(ty.size),'note':'inherited scalar neutral range -6..6 EV; continuous endpoint plateaus retained, not fixed in this candidate'}
  check(kind+' '+stock+' legacy repeat',r['legacy_repeat_'+stock]['exact'],r['legacy_repeat_'+stock])
  for control,current,previous in [('Round5','round5_'+stock,'round5_'+stock),('v01','research_v01_'+stock,'research_'+stock)]:
   check(kind+' '+stock+' '+control+' exact historical float',sha(folder/(current+'.f32'))==sha(old/kind/(previous+'.f32')),{'old_sha256':sha(old/kind/(previous+'.f32')),'current_sha256':sha(folder/(current+'.f32'))})
   check(kind+' '+stock+' '+control+' exact historical PNG',sha(folder/(current+'.png'))==sha(old/kind/(previous+'.png')),{'old_sha256':sha(old/kind/(previous+'.png')),'current_sha256':sha(folder/(current+'.png'))})
  aa=rgb(folder,name);bb=rgb(folder,'round5_'+stock);ca=np.any((aa<=0)|(aa>=1),axis=-1).mean();cb=np.any((bb<=0)|(bb>=1),axis=-1).mean()
  check(kind+' '+stock+' display clipping',ca<=cb+.001,{'v02_endpoint_fraction':float(ca),'Round5_endpoint_fraction':float(cb),'domain':'RGB8 endpoint counts, not raw decode or linear >1 clipping'})
 # Actual RAW region consistency, not a skin target or a film accuracy score.
 imgs={k:rgb(root/'raw',n) for k,n in [('Digital','digital_baseline'),('Round5','round5_'+stock),('v01','research_v01_'+stock),('v02',name)]}
 regions[stock]={}
 for region,box in ROIS.items():
  regions[stock][region]={}
  for label,im in imgs.items():
   crop=patch(im,box);lab=rgb2lab(crop);regions[stock][region][label]={'mean_Lab_D65':lab.mean((0,1)).tolist(),'mean_chroma':float(np.linalg.norm(lab[...,1:],axis=-1).mean())}
   ImageOps.pad(Image.fromarray(np.uint8(np.round(crop*255))),(240,180),color='#202020').save(root/'raw'/(stock+'_'+region+'_'+label+'.png'))
  regions[stock][region]['mean_DE00_v02_vs_v01']=float(deltaE_ciede2000(rgb2lab(patch(imgs['v02'],box)),rgb2lab(patch(imgs['v01'],box))).mean())
 for control,im in [('v01',imgs['v01']),('Round5',imgs['Round5'])]:Image.fromarray(np.uint8(np.clip(np.abs(imgs['v02']-im)*10,0,1)*255)).save(root/'raw'/('difference10x_'+stock+'_vs_'+control+'.png'))
 fig,ax=plt.subplots(1,2,figsize=(11,4));x=np.arange(97)*.125-6
 for row in range(8):ax[0].plot(x,y[row]);ax[1].plot(x[:-1],dy[row])
 ax[0].set_yscale('log');ax[0].set_title(stock+' v0.2 Y: eight ramps');ax[1].set_title('Adjacent Y difference (negative fails)');fig.tight_layout();fig.savefig(root/(stock+'-ramps.png'));plt.close(fig)

for kind in ['raw','synthetic']:
 check(kind+' exact historical input',sha(root/kind/'input-linear-prophoto.f32')==sha(old/kind/'input-linear-prophoto.f32'),{'same_input_bytes':sha(root/kind/'input-linear-prophoto.f32')==sha(old/kind/'input-linear-prophoto.f32')})
passed=all(c['passed'] for c in checks)
result={'status':'PASS_ENGINEERING' if passed else 'REJECTED_ENGINEERING','checks':checks,'metrics':metrics,'regions':regions,'ROIs':ROIS,'reports':reports,'gui_promotion':passed,'note':'one candidate; no subjective tuning; no film accuracy claim; separate Lab whitepoints'}
(root/'metrics_and_settings.json').write_text(json.dumps(result,indent=2,allow_nan=False),encoding='utf-8')
page='<!doctype html><meta charset="utf-8"><title>Phoenix Research v0.2</title><style>body{background:#202020;color:#eee;font:16px system-ui;margin:20px}main{display:grid;grid-template-columns:repeat(4,1fr);gap:10px}img{width:100%}figure{margin:0}pre{white-space:pre-wrap}a{color:#acd9ff}</style><h1>Research v0.2: '+result['status']+'</h1><p>Digital / Round 5 / rejected v0.1 / v0.2. Same Sony RAW, EV 0, preview, scan only, shoulder and display transform. Not verified film accuracy. No further tuning.</p>'
for stock in ['phoenix_i','phoenix_ii']:
 page+='<h2>'+stock+'</h2><main>'
 for label,n in [('Digital','digital_baseline'),('Round5','round5_'+stock),('v01','research_v01_'+stock),('v02','research_'+stock)]:page+='<figure><figcaption>'+label+'</figcaption><img src="raw/'+n+'.png"></figure>'
 page+='</main>'
 for region in ROIS:
  page+='<h3>'+region+'</h3><main>'
  for label in ['Digital','Round5','v01','v02']:page+='<figure><figcaption>'+label+'</figcaption><img src="raw/'+stock+'_'+region+'_'+label+'.png"></figure>'
  page+='</main>'
 page+='<h3>Absolute difference ×10</h3><main>'
 for control in ['v01','Round5']:page+='<img src="raw/difference10x_'+stock+'_vs_'+control+'.png">'
 page+='</main><img style="max-width:1100px" src="'+stock+'-ramps.png">'
page+='<h2>Engineering checks</h2><pre>'+html.escape(json.dumps(checks,indent=2))+'</pre><a href="metrics_and_settings.json">All metrics, separate linear/display hue, regions, settings and provenance</a>'
(root/'delta_and_side_by_side.html').write_text(page,encoding='utf-8')
ims=[Image.open(root/'raw'/n).convert('RGB') for n in ['digital_baseline.png','round5_phoenix_i.png','research_v01_phoenix_i.png','research_phoenix_i.png','digital_baseline.png','round5_phoenix_ii.png','research_v01_phoenix_ii.png','research_phoenix_ii.png']];w,h=ims[0].size;canvas=Image.new('RGB',(w*4,(h+30)*2),'#202020');draw=ImageDraw.Draw(canvas)
for i,im in enumerate(ims):x=i%4*w;y=i//4*(h+30);canvas.paste(im,(x,y+30));draw.text((x+8,y+8),['Digital','Round 5','Research v0.1','Research v0.2'][i%4]+(' - Phoenix I' if i<4 else ' - Phoenix II'),fill='white')
canvas.save(root/'comparison.png')
(root/'sha256.json').write_text(json.dumps({str(p.relative_to(root)).replace('\\','/'):sha(p) for p in root.rglob('*') if p.is_file() and p.name!='sha256.json'},indent=2),encoding='utf-8')
print(result['status']);print('Checks:',len(checks),'passed:',sum(c['passed'] for c in checks));print(json.dumps([c for c in checks if not c['passed']],indent=2))
sys.exit(0 if passed else 2)
