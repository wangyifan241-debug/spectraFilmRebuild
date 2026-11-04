"""Phase 2: reuse real reference summaries and actual fixed-RAW engine output."""
import hashlib,io,json,shutil
from pathlib import Path
import numpy as np
from PIL import Image,ImageCms,ImageDraw
from skimage.color import rgb2lab,deltaE_ciede2000
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
BASE=Path('research/phoenix-phase2');OUT=Path('build/phoenix-phase2');BASE.mkdir(exist_ok=True);OUT.mkdir(exist_ok=True)
reference=json.loads(Path('research/phoenix-reference-dataset/exploratory_statistics.json').read_text())
manifest={r['id']:r for r in map(json.loads,Path('research/phoenix-reference-dataset/dataset_manifest.jsonl').read_text(encoding='utf-8').splitlines())}
stocks=['Phoenix 200','Phoenix II 200'];rng=np.random.default_rng(20261104)
def category(scene):
    if scene in ['portrait_greenery','portrait_urban','street_portrait']:return 'human_portrait'
    if 'flower' in scene:return 'flowers'
    if any(k in scene for k in ['foliage','vegetation']):return 'vegetation'
    if scene in ['beach_sky','landscape_sunset','mountains_water']:return 'landscape'
    if scene in ['architecture','urban_architecture_sky','urban_daylight']:return 'architecture'
    if 'indoor' in scene:return 'indoor'
    return 'other'
def features(row):
    y=row['linear_Y_percentiles']
    return np.array(row['Lab_D65_mean']+row['OKLab_mean']+[y[3],y[5]-y[1]])
names=['Lab_L','Lab_a','Lab_b','OKLab_L','OKLab_a','OKLab_b','Y_median','Y_p95_minus_p05']
authors={s:{a:np.mean([features(r) for r in reference if r['film_stock']==s and r['photographer']==a],axis=0) for a in sorted({r['photographer'] for r in reference if r['film_stock']==s})} for s in stocks}
arrays=[np.array(list(authors[s].values())) for s in stocks]
observed=arrays[1].mean(0)-arrays[0].mean(0)
boot=np.array([arrays[1][rng.integers(len(arrays[1]),size=len(arrays[1]))].mean(0)-arrays[0][rng.integers(len(arrays[0]),size=len(arrays[0]))].mean(0) for _ in range(2000)])
leave=[]
for group in range(2):
    for i in range(len(arrays[group])):
        temp=[a.copy() for a in arrays];temp[group]=np.delete(temp[group],i,axis=0);leave.append(temp[1].mean(0)-temp[0].mean(0))
leave=np.array(leave)
stability={n:{'II_minus_I_author_balanced':float(observed[i]),'author_bootstrap_95_interval':np.percentile(boot[:,i],[2.5,97.5]).tolist(),'leave_one_author_out_range':[float(leave[:,i].min()),float(leave[:,i].max())],'caution':'descriptive resampling of these authors, not independent film causality'} for i,n in enumerate(names)}
groups={}
for scene in sorted({category(r['scene_category']) for r in reference}):
    groups[scene]={}
    for stock in stocks:
        subset=[r for r in reference if r['film_stock']==stock and category(r['scene_category'])==scene]
        byauthor={a:np.mean([features(r) for r in subset if r['photographer']==a],axis=0) for a in {r['photographer'] for r in subset}}
        groups[scene][stock]={'n':len(subset),'authors':len(byauthor),'ids':[r['id'] for r in subset],'author_balanced_mean':np.mean(list(byauthor.values()),axis=0).tolist() if byauthor else None}
def read(path):
    image=Image.open(path);icc=image.info.get('icc_profile')
    if icc:image=ImageCms.profileToProfile(image,ImageCms.ImageCmsProfile(io.BytesIO(icc)),ImageCms.createProfile('sRGB'),outputMode='RGB')
    else:image=image.convert('RGB')
    return np.asarray(image,dtype=float)/255
source=Path('build/phoenix-phase0-raw-02')
files={'digital':'digital-input.png','round5-I':'harman_phoenix_200_exp_v1-round5-scan.png','round5-II':'harman_phoenix_ii_200_exp_v1-round5-scan.png'}
images={};receipts={}
for name,filename in files.items():
    path=source/filename;destination=OUT/(name+'.png');shutil.copyfile(path,destination);images[name]=read(destination)
    receipts[name]={'source':str(path),'PNG_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'canonical_RGB8_sha256':hashlib.sha256(np.uint8(np.round(images[name]*255)).tobytes()).hexdigest(),'execution':'reused actual Phase 0 Sony RAW engine output, not a new render or a Research output'}
provenance=json.loads((source/'report.json').read_text())['provenance']
metrics={}
for first,second in [('round5-I','digital'),('round5-II','digital'),('round5-II','round5-I')]:
    a,b=images[first],images[second];difference=np.abs(a-b);de=deltaE_ciede2000(rgb2lab(a),rgb2lab(b))
    metrics[first+'-vs-'+second]={'RGB_MAE_255':float(difference.mean()*255),'DE00_D65_mean':float(de.mean()),'DE00_D65_p95':float(np.percentile(de,95))}
    Image.fromarray(np.uint8(np.clip(difference*2550,0,255))).save(OUT/(first+'-vs-'+second+'-diff10x.png'))
roi={'sony':{'rect':[.28,.56,.34,.62],'description':'manually inspected exposed skin patch; not a calibrated skin target'},'reference':{'id':'P2_naeem_parasol','rect':[.525,.345,.555,.36],'description':'manually inspected cheek patch, outdoor parasol illumination; not paired'}}
def patch(array,rectangle):
    h,w=array.shape[:2];x0,y0,x1,y1=rectangle;return array[int(y0*h):int(y1*h),int(x0*w):int(x1*w)]
regions={}
for name,array in images.items():
    p=patch(array,roi['sony']['rect']);regions[name]={'Lab_D65_mean':rgb2lab(p).mean((0,1)).tolist(),'pixel_count':p.shape[0]*p.shape[1]};Image.fromarray(np.uint8(np.round(p*255))).resize((240,180)).save(OUT/(name+'-skin-patch.png'))
r=manifest['P2_naeem_parasol'];reference_image=read(Path(r['local_file']));p=patch(reference_image,roi['reference']['rect']);regions['reference-II-Naeem']={'Lab_D65_mean':rgb2lab(p).mean((0,1)).tolist(),'pixel_count':p.shape[0]*p.shape[1],'color_policy':r['color_profile']+'; provisional sRGB if untagged','comparison':'different person, lighting and scanner; no unpaired DE accuracy metric'}
Image.fromarray(np.uint8(np.round(p*255))).resize((240,180)).save(OUT/'reference-II-skin-patch.png')
# Compare output shape to the available portrait stratum, not the unrelated global mix.
portrait=[r for r in reference if category(r['scene_category'])=='human_portrait' and r['film_stock']=='Phoenix II 200']
fig,axes=plt.subplots(1,3,figsize=(12,4))
for name,array in images.items():
    lab=rgb2lab(array);lin=np.where(array<=.04045,array/12.92,((array+.055)/1.055)**2.4);y=lin@np.array([.2126,.7152,.0722])
    for axis,values,bounds in [(axes[0],lab[:,:,0],(0,100)),(axes[1],lab[:,:,1],(-128,128)),(axes[2],y,(0,1))]:
        hist,edges=np.histogram(values,bins=40,range=bounds);axis.plot((edges[:-1]+edges[1:])/2,hist/max(1,hist.sum()),label=name)
for c,axis in enumerate(axes):
    key='Y_histogram' if c==2 else 'Lab_channel_histograms';vectors=[]
    for row in portrait:
        h=np.array(row[key] if c==2 else row[key][c],float);vectors.append(h/h.sum())
    axis.plot(np.linspace(.0125,.9875,40) if c==2 else np.linspace(1.25,98.75,40) if c==0 else np.linspace(-124.8,124.8,40),np.mean(vectors,axis=0),linestyle='--',label='II reference portraits: unmatched scans')
    axis.set_title(['Lab D65 L*','Lab D65 a*','Linear Y'][c]);axis.grid(alpha=.3)
handles,labels=axes[0].get_legend_handles_labels();fig.legend(handles,labels,loc='lower center',ncol=2);fig.suptitle('Portrait stratum is incomplete: no red-stock human portraits; no causal film fit');fig.tight_layout(rect=(0,.2,1,.9));fig.savefig(BASE/'portrait-comparison.png',dpi=150);shutil.copyfile(BASE/'portrait-comparison.png',OUT/'portrait-comparison.png')
result={'feature_order':names,'reference_count':len(reference),'author_counts':{s:len(authors[s]) for s in stocks},'author_stability':stability,'scene_groups':groups,'paired_software_metrics':metrics,'ROIs':roi,'region_statistics':regions,'render_receipts':receipts,'RAW_provenance':{k:provenance[k] for k in ['input_absolute','input_sha256','source_width','source_height','preview_stride','executable_sha256']},'Research_v0_1':'not implemented; no sufficient evidence for a defensible material/color fit'}
(BASE/'comparison.json').write_text(json.dumps(result,indent=2),encoding='utf-8');(OUT/'comparison.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
html='''<!doctype html><meta charset="utf-8"><title>Phoenix Phase 2 comparison</title><style>body{background:#202124;color:#eee;font:16px system-ui;margin:24px}main{display:flex;gap:12px;flex-wrap:wrap}figure{margin:0;max-width:32%}img{width:100%}pre{white-space:pre-wrap}.wide{max-width:100%;width:1000px}</style><h1>Phoenix Phase 2 — actual RAW output</h1><p>DSC02443.ARW; actual saved Round 5 engine renders reused with fingerprints. Digital baseline includes the same display shoulder. Research v0.1 is not implemented: no replacement image or EXE is claimed.</p><main>'''
for name in files:html+=f'<figure><figcaption>{name}</figcaption><img src="{name}.png"></figure>'
html+='</main><h2>Absolute encoded RGB difference x10</h2><img style="max-width:600px" src="round5-II-vs-round5-I-diff10x.png"><h2>Portrait distributions (unpaired)</h2><img class="wide" src="portrait-comparison.png"><h2>Skin patches: lighting and scanner NOT controlled</h2><main>'
for name in ['digital','round5-I','round5-II','reference-II']:html+=f'<figure><figcaption>{name}</figcaption><img src="{name}-skin-patch.png"></figure>'
html+='</main><p>No red reference skin target. A visual or statistical mismatch is not a physical film error.</p><h2>Software-only paired metrics</h2><pre>'+json.dumps(metrics,indent=2)+'</pre><h2>Author leave-one-out / bootstrap</h2><pre>'+json.dumps(stability,indent=2)+'</pre>'
(OUT/'index.html').write_text(html,encoding='utf-8')
print(json.dumps({'paired_metrics':metrics,'author_stability':stability,'portrait_reference_count':len(portrait)},indent=2))
