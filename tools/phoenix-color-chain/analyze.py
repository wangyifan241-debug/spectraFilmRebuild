"""Stage localization with the same proxy negative; no fitting a positive look."""
from pathlib import Path
import json,hashlib,shutil,html
import numpy as np
import tifffile
from PIL import Image,ImageOps
from skimage.color import rgb2lab,xyz2lab,deltaE_ciede2000

ROOT=Path('build/phoenix-color-chain-run1');OUT=Path('validation/phoenix-color-chain');OUT.mkdir(exist_ok=True)
W=np.array([.2880402,.7118741,.0000857]);EV=np.arange(97)*.125-6
checks=[];result={'classification':'diagnostic oracle for unchanged proxy spectra; not a new film model','runs':{},'local_derivatives':{},'checks':checks}
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def stats(v):
 a=np.asarray(v).ravel();return {'n':int(a.size),'min':float(a.min()),'p50':float(np.median(a)),'p95':float(np.percentile(a,95)),'max':float(a.max())} if a.size else {'n':0}
def test(n,b,d):checks.append({'name':n,'passed':bool(b),'details':d})
def data(p,n,r):
 q=r[n];return np.fromfile(p/(n+'.f32'),dtype='<f4').reshape(q['height'],q['width'],3).astype(float)
def ref_at(T,N):
 axis=N@W;position=np.interp(T@W,axis[::-1],np.arange(97)[::-1]);k=np.minimum(np.floor(position).astype(int),95);f=position-k
 slope=np.diff(N.astype(np.float32),axis=0).astype(float)
 return N[k]+f[...,None]*slope[k],.184*2**(-6+position*.125)
def positive(T,N):
 ref,e=ref_at(T,N);u=np.log(ref/T);u-=(u@W)[...,None];norm=np.linalg.norm(u,axis=-1);scale=np.ones_like(norm);np.divide(np.tanh(norm/np.log(4)),norm/np.log(4),out=scale,where=norm>1e-12);shape=np.exp(u*scale[...,None]);return e[...,None]*shape/(shape@W)[...,None]
def hue(lab):return np.linalg.norm(lab[...,1:],axis=-1),np.degrees(np.arctan2(lab[...,2],lab[...,1]))
def hue_eval(lab):
 c,h=hue(lab);d=np.abs((np.diff(h,axis=1)+180)%360-180);ok=(c[:,:-1]>2)&(c[:,1:]>2);return {'eligible_pairs':int(ok.sum()),'failed_pairs':int((ok&(d>15)).sum()),'max_step':float(d[ok].max()) if ok.any() else 0,'threshold':15,'minimum_chroma':2}
ROI=json.loads(Path('validation/phoenix-v02-diagnostics/diagnostics.json').read_text(encoding='utf-8'))['scenes']

for scene in ['synthetic','synthetic-local','DSC02443','DSC02337']:
 p=ROOT/scene;r=json.loads((p/'report.json').read_text());dest=OUT/scene;dest.mkdir(exist_ok=True)
 entry={'provenance':r['provenance'],'stocks':{}}
 for stock in ['phoenix_i','phoenix_ii']:
  rec=r['direct_receipt_'+stock];dyes=np.array(rec['sanitized_dye_density_GPU_f32']).reshape(81,3);weights=np.array(rec['XYZ_integral_weights_D50']).reshape(81,3).astype(np.float32).astype(float);rgbM=np.array(rec['XYZ_to_ProPhoto_row_major']).reshape(3,3);m=np.array(r['device']['detector_to_XYZ_row_major']).reshape(3,3)
  base=json.loads((p/('legacy_recipe_'+stock+'.json')).read_text());toXYZ=np.array(base['linear_prophoto_to_XYZ_D50']).reshape(3,3)
  test(scene+' '+stock+' baked XYZ/ProPhoto roundtrip precision',np.max(np.abs(rgbM@toXYZ-np.eye(3)))<2e-5,{'roundtrip_max_error':float(np.max(np.abs(rgbM@toXYZ-np.eye(3)))),'white_XYZ_from_fit':m.sum(1).tolist(),'flat_transmission_ProPhoto':(rgbM@m.sum(1)).tolist(),'tone_Y_vs_baked_Y_row_max_error':float(np.max(np.abs(W-toXYZ[1])))})
  neg=data(p,'negative_density_'+stock,r);xyz=data(p,'direct_XYZ_'+stock+'_pixels',r)-1e-10
  indices=np.linspace(0,neg.size//3-1,min(1024,neg.size//3),dtype=int);trans=np.power(10.,-(neg.reshape(-1,3)[indices]@dyes.T));cpu=trans@weights;gpu=xyz.reshape(-1,3)[indices]
  error=np.max(np.abs(cpu-gpu));test(scene+' '+stock+' CPU/GPU 81-band integral',error<2e-5,{'max_abs_XYZ_error':float(error),'samples':len(indices)})
  logT=data(p,'research_logT_'+stock,r);measured=np.power(10.,logT)-1e-10;approxXYZ=measured@m.T
  T=data(p,'device_transmission_'+stock,r);N=data(p,'neutral_device_transmission_'+stock,r)[0];TE=data(p,'direct_transmission_'+stock,r);NE=data(p,'neutral_direct_transmission_'+stock,r)[0];P=data(p,'research_'+stock,r);PE=data(p,'direct_integration_diagnostic_'+stock,r)
  test(scene+' '+stock+' fitted device replay',np.max(np.abs(T-approxXYZ@rgbM.T))<2e-5,{'max_abs_error':float(np.max(np.abs(T-approxXYZ@rgbM.T)))})
  test(scene+' '+stock+' bounded positive replay',np.max(np.abs(positive(T,N)-P))<2e-5,{'max_abs_error':float(np.max(np.abs(positive(T,N)-P)))})
  test(scene+' '+stock+' direct positive replay',np.max(np.abs(positive(TE,NE)-PE))<2e-5,{'max_abs_error':float(np.max(np.abs(positive(TE,NE)-PE)))})
  interior=((T@W)<(N[0]@W))&((T@W)>(N[-1]@W))&((TE@W)<(NE[0]@W))&((TE@W)>(NE[-1]@W))
  oldref,e=ref_at(T,N);exactref,_=ref_at(T,NE);nr=np.log(exactref/oldref);pr=np.log(TE/T);delta=nr-pr
  for a in [nr,pr,delta]:a-=(a@W)[...,None]
  linear_error=np.linalg.norm(approxXYZ-xyz,axis=-1)/np.maximum(xyz[...,1],1e-8)
  im=tifffile.imread(p/('research_'+stock+'.tif'))[...,:3]/65535.;ie=tifffile.imread(p/('direct_integration_diagnostic_'+stock+'.tif'))[...,:3]/65535.;lab=rgb2lab(im);le=rgb2lab(ie);de=deltaE_ciede2000(lab,le)
  out={'virtual_negative_device_relative_XYZ_error':stats(linear_error[interior]),'virtual_negative_transmission_DE00_D50':stats(deltaE_ciede2000(xyz2lab(xyz,illuminant='D50'),xyz2lab(approxXYZ,illuminant='D50'))[interior]),'same_positive_formula_direct_vs_fit_display16_DE00_common_interior':stats(de[interior]),'neutral_transmission_at_EV0_fit':N[48].tolist(),'neutral_transmission_at_EV0_direct':NE[48].tolist(),'direct_engine_constraints':r['direct_constraints_'+stock],'fitted_toXYZ_vs_inverse_baked_matrix_coefficient_error':float(np.max(np.abs(rgbM-np.linalg.inv(toXYZ)))),'ROIs':{}}
  if scene.startswith('DSC'):
   h,w=interior.shape
   for name,b in ROI[scene]['ROIs'].items():
    x0,y0,x1,y1=b;sl=(slice(int(y0*h),int(y1*h)),slice(int(x0*w),int(x1*w)));mask=interior[sl]
    out['ROIs'][name]={'common_interior_fraction':float(mask.mean()),'fit_mean_Lab_D65':lab[sl].mean((0,1)).tolist(),'direct_mean_Lab_D65':le[sl].mean((0,1)).tolist(),'direct_vs_fit_DE00':stats(de[sl][mask]),'relative_XYZ_error':stats(linear_error[sl][mask]),'fixed_exposure_log_reference_term_norm':stats(np.linalg.norm(nr[sl],axis=-1)[mask]),'fixed_exposure_log_pixel_term_norm':stats(np.linalg.norm(pr[sl],axis=-1)[mask]),'fixed_exposure_log_net_change_norm':stats(np.linalg.norm(delta[sl],axis=-1)[mask])}
    for label,image in [('fit',im),('direct',ie)]:ImageOps.pad(Image.fromarray(np.uint8(np.round(image[sl]*255))),(240,180),color='#202020').save(dest/(stock+'_'+name+'_'+label+'.png'))
   prior=Path('build/phoenix-phase3-v02-run1/raw') if scene=='DSC02443' else Path('build/phoenix-v02-diagnostics-run1/DSC02337')
   for n in ['round5_'+stock,'research_v01_'+stock,'research_'+stock]:test(scene+' '+n+' exact historical float',sha(p/(n+'.f32'))==sha(prior/(n+'.f32')),{'current':sha(p/(n+'.f32')),'historical':sha(prior/(n+'.f32'))})
   Image.fromarray(np.uint8(np.clip(np.abs(im-ie)*10,0,1)*255)).save(dest/(stock+'_difference10x.png'))
  if scene=='synthetic':
   yy=PE@W;dy=np.diff(yy,axis=1);neutral_spread=np.ptp(PE[0],axis=-1)/np.maximum(PE[0].mean(-1),1e-9);curve=np.abs(np.diff(dy,axis=1))/np.maximum(.184,np.abs(yy[:,1:-1]))
   test(stock+' direct integral neutral',neutral_spread.max()<.005,{'max_relative_spread':float(neutral_spread.max())})
   test(stock+' direct integral monotonic ramps',dy.min()>=-1e-5,{'minimum_Y_step':float(dy.min())})
   test(stock+' direct integral continuity',curve.max()<.25,{'maximum_normalized_second_difference':float(curve.max())})
   for label,n in [('fit','research_'+stock),('direct','direct_integration_diagnostic_'+stock)]:
    a=tifffile.imread(p/(n+'.tif'))[4:,:,:3]/65535.;high=np.asarray(Image.open(p/(n+'.png')).convert('RGB'))[4:]/255.;rounded=np.round(a*255)/255.;linear=data(p,n,r)[4:];out[label+'_hue_domains']={'linear_Lab_D50':hue_eval(xyz2lab(linear@toXYZ.T,illuminant='D50')),'native_RGB16_Lab_D65':hue_eval(rgb2lab(a)),'historical_high_byte_RGB8_Lab_D65':hue_eval(rgb2lab(high)),'nearest_rounded_RGB8_Lab_D65':hue_eval(rgb2lab(rounded)),'interpretation':'separate evaluations; historical failure stays failed, rounded8 does not replace it'}
  if scene=='synthetic-local':
   derivatives={}
   for stage in ['negative_density_','research_logT_','device_transmission_','direct_transmission_','research_','direct_integration_diagnostic_']:
    values=data(p,stage+stock,r).reshape(5,7,3);rows=[]
    for j in range(5):
     J=np.column_stack([(values[j,2+2*c]-values[j,1+2*c])/(.02*.184*2**(j-2)) for c in range(3)]);sv=np.linalg.svd(J,compute_uv=False);rows.append({'EV':j-2,'Jacobian_rows_output_cols_input':J.tolist(),'singular_values':sv.tolist(),'off_diagonal_absolute_fraction':float((np.abs(J).sum()-np.abs(np.diag(J)).sum())/max(np.abs(J).sum(),1e-12))})
    derivatives[stage]=rows
   result['local_derivatives'][stock]=derivatives
  entry['stocks'][stock]=out
  for n in ['research_'+stock,'direct_integration_diagnostic_'+stock,'round5_'+stock]:shutil.copy2(p/(n+'.png'),dest/(n+'.png'))
 shutil.copy2(p/'digital_baseline.png',dest/'digital_baseline.png')
 for f in p.glob('*.json'):shutil.copy2(f,dest/f.name)
 result['runs'][scene]=entry

pa=json.loads(Path('engine/resources_experimental/profiles/harman_phoenix_200_round5.json').read_text(encoding='utf-8'));pb=json.loads(Path('engine/resources_experimental/profiles/harman_phoenix_ii_200_round5.json').read_text(encoding='utf-8'));da=np.array(pa['data']['channel_density'],float);db=np.array(pb['data']['channel_density'],float)
result['profile_evidence']={'dye_arrays_identical_including_missing':bool(np.array_equal(da,db,equal_nan=True)),'Phoenix_I_provenance':pa['metadata']['datasource'],'Phoenix_II_provenance':pb['metadata']['datasource'],'note':'same proxy dye basis does not prove it is physically correct for either stock; sensitivity and characteristic response differ'}
(OUT/'metrics.json').write_text(json.dumps(result,indent=2,allow_nan=False),encoding='utf-8')
page='<!doctype html><meta charset="utf-8"><title>Color chain localization</title><style>body{background:#202020;color:#eee;font:16px system-ui;margin:20px}main{display:grid;grid-template-columns:repeat(4,1fr);gap:10px}img{width:100%}figure{margin:0}pre{white-space:pre-wrap}a{color:#acd9ff}</style><h1>Same proxy negative: fitted device vs direct spectral integral</h1><p>Diagnostic only, not v0.3 or validated Phoenix color. No endpoint extension. Same positive formula and display. <a href="RESULT.md">Attribution report</a> · <a href="metrics.json">All stage evidence</a></p>'
for scene in ['DSC02443','DSC02337']:
 page+='<h2>'+scene+'</h2>'
 for stock in ['phoenix_i','phoenix_ii']:
  page+='<h3>'+stock+'</h3><main>'
  for label,n in [('Digital','digital_baseline'),('Round5','round5_'+stock),('frozen v0.2','research_'+stock),('direct integration: diagnostic only','direct_integration_diagnostic_'+stock)]:page+='<figure><figcaption>'+label+'</figcaption><img src="'+scene+'/'+n+'.png"></figure>'
  page+='</main><img style="max-width:400px" src="'+scene+'/'+stock+'_difference10x.png">'
  for region in result['runs'][scene]['stocks'][stock]['ROIs']:
   page+='<h4>'+region+'</h4><main>'
   for label in ['fit','direct']:page+='<figure><figcaption>'+label+'</figcaption><img src="'+scene+'/'+stock+'_'+region+'_'+label+'.png"></figure>'
   page+='</main>'
  page+='<details><summary>Stage and region numbers</summary><pre>'+html.escape(json.dumps(result['runs'][scene]['stocks'][stock],indent=2))+'</pre></details>'
(OUT/'index.html').write_text(page,encoding='utf-8')
print('Numerical checks',len(checks),'passed',sum(c['passed'] for c in checks))
print(json.dumps([c for c in checks if not c['passed']],indent=2))
for scene in ['DSC02443','DSC02337']:
 for s,d in result['runs'][scene]['stocks'].items():print(scene,s,d['virtual_negative_device_relative_XYZ_error'],d['same_positive_formula_direct_vs_fit_display16_DE00_common_interior'])
