"""Read-only diagnosis of the frozen v0.2 EXE. No model fitting or new rendering policy."""
from pathlib import Path
import json,hashlib,csv,html,shutil
import numpy as np
from PIL import Image,ImageDraw,ImageOps
import tifffile
from skimage.color import rgb2lab,xyz2lab,deltaE_ciede2000
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from scipy.ndimage import binary_propagation

OUT=Path('validation/phoenix-v02-diagnostics');OUT.mkdir(exist_ok=True)
W=np.array([.2880402,.7118741,.0000857]);FLOOR=.184*2**-6
SCENES={
 'DSC02443':{'path':'build/phoenix-phase3-v02-run1/raw','description':'original indoor portrait / uniform','roi':{'face':[.28,.25,.43,.36],'arm':[.28,.56,.34,.62],'uniform':[.72,.43,.86,.60],'dark_clothing':[.26,.85,.34,.94],'background':[.04,.1,.10,.30]}},
 'DSC02337':{'path':'build/phoenix-v02-diagnostics-run1/DSC02337','description':'backlit outdoor portrait / foliage / bright sky','roi':{'face':[.36,.46,.47,.54],'hair':[.23,.32,.38,.43],'sky':[.30,.06,.65,.17],'foliage':[.06,.61,.24,.78],'patterned_shirt':[.62,.53,.78,.67],'white_sleeve':[.78,.75,.91,.90]}},
 'DSC03870':{'path':'build/phoenix-v02-diagnostics-run1/DSC03870','description':'night street / lit signage / dark road','roi':{'dark_sky':[.10,.05,.30,.20],'red_sign':[.49,.23,.53,.265],'storefront':[.55,.48,.62,.57],'bicycles':[.43,.64,.59,.71],'road':[.55,.79,.80,.92],'dark_trees':[.82,.25,.94,.50]}}}

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def pct(a):
 a=np.asarray(a);return {'n':int(a.size),'p01_p50_p99':np.percentile(a,[1,50,99]).tolist(),'min':float(a.min()),'max':float(a.max()),'std':float(a.std())} if a.size else {'n':0}
def flo(p,n,r):
 q=r[n];return np.fromfile(p/(n+'.f32'),dtype='<f4').reshape(q['height'],q['width'],3).astype(float)
def s16(p,n):return tifffile.imread(p/(n+'.tif'))[...,:3].astype(float)/65535.
def crop(a,b):
 h,w=a.shape[:2];x0,y0,x1,y1=b;return a[int(y0*h):int(y1*h),int(x0*w):int(x1*w)]
def angle(a,b):return (a-b+180)%360-180
def lc(lab):return np.linalg.norm(lab[...,1:],axis=-1),np.degrees(np.arctan2(lab[...,2],lab[...,1]))

result={'mode':'diagnostics only; unchanged v0.2 EXE, coefficients, thresholds and display transform','floor_linear_Y':FLOOR,'scenes':{},'hue_quantization':{},'limitations':['software-domain contrasts are not film accuracy','only three available RAWs, same camera family; no broad camera/scene generalization','no exposure adjustments; a night input can be poorly placed under fixed EV 0']}
for scene,setup in SCENES.items():
 p=Path(setup['path']);r=json.loads((p/'report.json').read_text());dest=OUT/scene;dest.mkdir(exist_ok=True)
 inp=flo(p,'input-linear-prophoto',r);inputY=inp@W;zero=np.all(inp==0,axis=-1);seed=np.zeros(zero.shape,bool);seed[0]=zero[0];seed[-1]=zero[-1];seed[:,0]=zero[:,0];seed[:,-1]=zero[:,-1];border_zero=binary_propagation(seed,mask=zero);source=s16(p,'digital_baseline');sourceLab=rgb2lab(source)
 data={'description':setup['description'],'provenance':r['provenance'],'ROIs':setup['roi'],'decoded_zero_pixels':int(zero.sum()),'decoded_zero_fraction':float(zero.mean()),'border_connected_decoded_zero_pixels':int(border_zero.sum()),'stocks':{}}
 shutil.copy2(p/'report.json',dest/'report.json')
 annotated=Image.fromarray(np.uint8(np.round(source*255)));draw=ImageDraw.Draw(annotated);h,w=source.shape[:2]
 for name,b in setup['roi'].items():x0,y0,x1,y1=b;draw.rectangle((x0*w,y0*h,x1*w,y1*h),outline='yellow',width=2);draw.text((x0*w,y0*h),name,fill='yellow')
 annotated.save(dest/'ROI_locations.png')
 for n in ['digital_baseline','round5_phoenix_i','research_v01_phoenix_i','research_phoenix_i','round5_phoenix_ii','research_v01_phoenix_ii','research_phoenix_ii']:shutil.copy2(p/(n+'.png'),dest/(n+'.png'))
 for stock in ['phoenix_i','phoenix_ii']:
  output=flo(p,'research_'+stock,r);trans=flo(p,'device_transmission_'+stock,r);neutral=flo(p,'neutral_device_transmission_'+stock,r)[0];ty=trans@W;axis=neutral@W;mask=ty>=axis[0];Y=output@W
  image16=s16(p,'research_'+stock);lab=rgb2lab(image16);de=deltaE_ciede2000(sourceLab,lab);Ldiff=lab[...,0]-sourceLab[...,0];cdiff=np.linalg.norm(lab[...,1:]-sourceLab[...,1:],axis=-1)
  matrix=np.array(json.loads((p/('legacy_recipe_'+stock+'.json')).read_text())['linear_prophoto_to_XYZ_D50']).reshape(3,3)
  # Chromaticity coordinate only: normalize Y to 1 without producing a changed image.
  norm_input=inp/np.maximum(inputY[...,None],1e-12);norm_output=output/np.maximum(Y[...,None],1e-12)
  nlab_i=xyz2lab(norm_input@matrix.T,illuminant='D50');nlab_o=xyz2lab(norm_output@matrix.T,illuminant='D50');ci,hi=lc(nlab_i);co,ho=lc(nlab_o)
  d={'floor_pixels':int(mask.sum()),'pixels':int(mask.size),'floor_fraction':float(mask.mean()),'ceiling_pixels':int((ty<=axis[-1]).sum()),'floor_max_relative_Y_error':float(np.abs(Y[mask]/FLOOR-1).max()) if mask.any() else None,'stage_bounds':r['v02_constraints_'+stock],'regions':{}}
  d['floor_decomposition']={'decoded_zero_on_floor_pixels':int((mask&zero).sum()),'decoded_zero_on_floor_fraction_of_frame':float((mask&zero).mean()),'nonzero_input_on_floor_pixels':int((mask&~zero).sum()),'nonzero_input_on_floor_fraction_of_frame':float((mask&~zero).mean()),'nonzero_input_on_floor_fraction_of_nonzero_input':float((mask&~zero).sum()/max(1,(~zero).sum())),'interpretation':'Already-zero decoded pixels contain no input luminance variation to lose; lifting them to the reconstruction floor is different from flattening nonzero shadows.'}
  for name,m in [('floor',mask),('floor_with_nonzero_input',mask&~zero),('nonfloor',~mask)]:
   eligible=m&(ci>2)&(co>2)&(inputY>1e-8)
   d[name]={'input_linear_Y':pct(inputY[m]),'output_linear_Y':pct(Y[m]),'display16_deltaL_vs_digital':pct(Ldiff[m]),'display16_delta_ab_distance_vs_digital':pct(cdiff[m]),'display16_deltaE00_vs_digital':pct(de[m]),'equal_Y_chromaticity_abs_hue_shift':pct(np.abs(angle(ho[eligible],hi[eligible]))),'equal_Y_chroma_ratio':pct(co[eligible]/ci[eligible]),'color_note':'equal-Y Lab is an analysis coordinate only, not a re-render or reference target'}
  # Neighbor contrast loss: same-domain log Y steps within pairs entirely on floor.
  pair=mask[:,:-1]&mask[:,1:];di=np.abs(np.diff(np.log2(np.maximum(inputY,1e-12)),axis=1));do=np.abs(np.diff(np.log2(Y),axis=1))
  d['horizontal_floor_neighbor_contrast']={'pairs':int(pair.sum()),'input_abs_EV_step':pct(di[pair]),'output_abs_EV_step':pct(do[pair])}
  overlay=np.uint8(np.round(image16*255));overlay[mask]=(overlay[mask].astype(float)*.35+np.array([255,0,255])*.65).astype(np.uint8);overlay[mask&zero]=(np.uint8(np.round(image16[mask&zero]*255)).astype(float)*.35+np.array([0,255,255])*.65).astype(np.uint8);Image.fromarray(overlay).save(dest/(stock+'_floor_overlay.png'));Image.fromarray(np.uint8(mask)*255).save(dest/(stock+'_floor_mask.png'))
  # Fixed deterministic sparse plot; original floats stay untouched.
  fig,ax=plt.subplots(figsize=(6,4));indices=np.linspace(0,mask.size-1,min(mask.size,20000),dtype=int);ax.scatter(inputY.ravel()[indices],Y.ravel()[indices],s=1,c=np.where(mask.ravel()[indices],'magenta','steelblue'));ax.axhline(FLOOR,color='black',lw=.6);ax.set_xscale('symlog',linthresh=1e-5);ax.set_yscale('log');ax.set_xlabel('Decoded scene-linear ProPhoto Y');ax.set_ylabel('v0.2 linear output Y');ax.set_title(scene+' '+stock+' (magenta: floor)');fig.tight_layout();fig.savefig(dest/(stock+'_tone.png'));plt.close(fig)
  for region,b in setup['roi'].items():
   rm=crop(mask,b);original=crop(sourceLab,b);candidate=crop(lab,b)
   d['regions'][region]={'floor_fraction':float(rm.mean()),'input_linear_Y':pct(crop(inputY,b)),'output_linear_Y':pct(crop(Y,b)),'Digital_mean_Lab_D65':original.mean((0,1)).tolist(),'v02_mean_Lab_D65':candidate.mean((0,1)).tolist(),'display16_deltaL':pct(candidate[...,0]-original[...,0]),'display16_delta_ab':pct(np.linalg.norm(candidate[...,1:]-original[...,1:],axis=-1)),'equal_Y_abs_hue_shift':pct(np.abs(crop(angle(ho,hi),b))),'note':'ROI diagnostics, not true skin or film-color targets; hue unreliable near neutral'}
   for label,im in [('Digital',source),('v02',image16)]:ImageOps.pad(Image.fromarray(np.uint8(np.round(crop(im,b)*255))),(240,180),color='#202020').save(dest/(stock+'_'+region+'_'+label+'.png'))
  data['stocks'][stock]=d
 result['scenes'][scene]=data

# Original synthetic skin sweeps only: preserve eligibility C8>2 and hue step >15.
p=Path('build/phoenix-phase3-v02-run1/synthetic');r=json.loads((p/'report.json').read_text());rows=[]
for stock in ['phoenix_i','phoenix_ii']:
 n='research_'+stock;trans=flo(p,'device_transmission_'+stock,r)[4:];ny=flo(p,'neutral_device_transmission_'+stock,r)[0]@W;skin_floor=trans@W>=ny[0];codes16=tifffile.imread(p/(n+'.tif'))[4:,:,:3];a16=codes16.astype(float)/65535.;codes8=np.asarray(Image.open(p/(n+'.png')).convert('RGB'))[4:];a8=codes8.astype(float)/255.
 lab16=rgb2lab(a16);lab8=rgb2lab(a8);C16,H16=lc(lab16);C8,H8=lc(lab8);qDE=deltaE_ciede2000(lab16,lab8);qRGB=np.abs(a8-a16).max(-1);h16=np.abs(angle(H16[:,1:],H16[:,:-1]));h8=np.abs(angle(H8[:,1:],H8[:,:-1]));eligible=(C8[:,:-1]>2)&(C8[:,1:]>2);fail=eligible&(h8>15)
 Lpair=np.minimum(lab16[:,:-1,0],lab16[:,1:,0]);Cpair=np.minimum(C16[:,:-1],C16[:,1:]);qpair=np.maximum(qDE[:,:-1],qDE[:,1:]);pair_error=np.abs(h8-h16)
 d={'original_gate':{'threshold_degrees':15,'minimum_chroma_each_endpoint':2,'eligible_pairs':int(eligible.sum()),'failed_pairs':int(fail.sum()),'max_hue_step_RGB8':float(h8[eligible].max())},'quantizer':{'original_PNG_IHDR_bit_depth':(p/(n+'.png')).read_bytes()[24],'PNG_Pillow_readback_equals_RGB16_high_byte':bool(np.array_equal(codes8,(codes16>>8).astype(np.uint8))),'PNG_Pillow_readback_equals_nearest_8bit':bool(np.array_equal(codes8,np.uint8(np.round(a16*255)))),'max_error_normalized_sRGB':float(qRGB.max()),'RGB8_vs_RGB16_deltaE00':pct(qDE),'note':'tests the actual historical PIL RGB8 readback, not an assumed rounding rule'},'stratified_pair_reports':{}}
 for dimension,values,bins in [('Lstar16',Lpair,[0,5,10,25,50,101]),('Cstar16',Cpair,[0,2,5,10,20,1e6]),('quantization_deltaE00',qpair,[0,.25,.5,1,1e6])]:
  table=[]
  for lo,hi in zip(bins[:-1],bins[1:]):
   m=(values>=lo)&(values<hi);em=m&eligible;fm=m&fail
   table.append({'range':[lo,hi],'all_pairs':int(m.sum()),'eligible_pairs_original_gate':int(em.sum()),'failed_pairs_original_gate':int(fm.sum()),'maximum_RGB8_hue_step_eligible':float(h8[em].max()) if em.any() else None,'maximum_RGB16_hue_step_same_pairs':float(h16[em].max()) if em.any() else None,'hue_step_quantization_error':pct(pair_error[em])})
  d['stratified_pair_reports'][dimension]=table
 d['C8_eligibility_vs_C16']={'RGB8_only_pairs':int((eligible&~((C16[:,:-1]>2)&(C16[:,1:]>2))).sum()),'RGB16_only_pairs':int((~eligible&((C16[:,:-1]>2)&(C16[:,1:]>2))).sum())}
 for row,col in zip(*np.where(fail)):
  rows.append({'stock':stock,'skin_row':int(row+4),'EV_from':-6+col*.125,'EV_to':-6+(col+1)*.125,'hue_step_RGB8':float(h8[row,col]),'hue_step_RGB16_same_pair':float(h16[row,col]),'L16_left':float(lab16[row,col,0]),'L16_right':float(lab16[row,col+1,0]),'C16_left':float(C16[row,col]),'C16_right':float(C16[row,col+1]),'C8_left':float(C8[row,col]),'C8_right':float(C8[row,col+1]),'max_quantization_deltaE00':float(qpair[row,col]),'either_endpoint_on_minus6_EV_floor':bool(skin_floor[row,col] or skin_floor[row,col+1]),'RGB16_left':codes16[row,col].tolist(),'RGB16_right':codes16[row,col+1].tolist(),'RGB8_left':codes8[row,col].tolist(),'RGB8_right':codes8[row,col+1].tolist()})
 result['hue_quantization'][stock]=d
(OUT/'RGB8_failed_pairs.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
with (OUT/'RGB8_failed_pairs.csv').open('w',newline='',encoding='utf-8') as f:
 writer=csv.DictWriter(f,fieldnames=list(rows[0]) if rows else ['stock']);writer.writeheader();writer.writerows(rows)
(OUT/'diagnostics.json').write_text(json.dumps(result,indent=2,allow_nan=False),encoding='utf-8')

page='<!doctype html><meta charset="utf-8"><title>Frozen v0.2 diagnostics</title><style>body{background:#202020;color:#eee;font:16px system-ui;margin:20px}main{display:grid;grid-template-columns:repeat(4,1fr);gap:10px}img{width:100%}figure{margin:0}pre{white-space:pre-wrap}a{color:#acd9ff}</style><h1>Frozen v0.2: endpoint / quantization / scene diagnostics</h1><p>No new model, tuning, exposure edits or relaxed gate. Overlay magenta: floor with nonzero decoded input; cyan: floor with already-zero decoded input. White binary mask: all floor pixels. These annotations are not reconstructed outputs.</p>'
page+='<p><a href="RESULT.md">Diagnostic attribution report</a> · <a href="#DSC02443">Original portrait</a> · <a href="#DSC02337">Backlit portrait</a> · <a href="#DSC03870">Night street</a></p>'
for scene,data in result['scenes'].items():
 page+='<h2 id="'+scene+'">'+scene+' — '+data['description']+'</h2><img style="max-width:360px" src="'+scene+'/ROI_locations.png">'
 for stock in ['phoenix_i','phoenix_ii']:
  page+='<h3>'+stock+'</h3><main>'
  for label,n in [('Digital','digital_baseline'),('Round5','round5_'+stock),('v01','research_v01_'+stock),('v02','research_'+stock)]:page+='<figure><figcaption>'+label+'</figcaption><img src="'+scene+'/'+n+'.png"></figure>'
  page+='</main><main><img src="'+scene+'/'+stock+'_floor_overlay.png"><img src="'+scene+'/'+stock+'_floor_mask.png"><img src="'+scene+'/'+stock+'_tone.png"></main>'
  for region in data['ROIs']:
   page+='<h4>'+region+'</h4><main>'
   for label in ['Digital','v02']:page+='<figure><figcaption>'+label+'</figcaption><img src="'+scene+'/'+stock+'_'+region+'_'+label+'.png"></figure>'
   page+='</main>'
  page+='<details><summary>Endpoint and ROI numbers</summary><pre>'+html.escape(json.dumps(data['stocks'][stock],indent=2))+'</pre></details>'
page+='<h2>RGB8 stratification: original gate retained</h2><pre>'+html.escape(json.dumps(result['hue_quantization'],indent=2))+'</pre><p><a href="RGB8_failed_pairs.csv">Every failed pair CSV</a> · <a href="RGB8_failed_pairs.json">Exact code values</a> · <a href="diagnostics.json">All diagnostics and provenance</a></p>'
(OUT/'index.html').write_text(page,encoding='utf-8')
print('Scenes:',len(result['scenes']),'RGB8 failed pairs:',len(rows))
for scene,data in result['scenes'].items():print(scene,{s:round(d['floor_fraction']*100,3) for s,d in data['stocks'].items()})
for stock,d in result['hue_quantization'].items():print(stock,d['original_gate'],d['quantizer'])
