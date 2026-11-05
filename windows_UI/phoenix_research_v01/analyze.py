"""Inspect actual diagnostic buffers. Never changes candidate coefficients."""
import hashlib,json,sys,shutil,html
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
from skimage.color import rgb2lab,deltaE_ciede2000
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

root=Path(sys.argv[1]);syn=root/'synthetic';raw=root/'raw'
checks=[]
def check(name,passed,details):checks.append(dict(name=name,passed=bool(passed),details=details))
def readf(folder,name):
    r=json.loads((folder/'report.json').read_text());q=r[name]
    return np.fromfile(folder/(name+'.f32'),dtype='<f4').reshape(q['height'],q['width'],3)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def image(folder,name):return np.asarray(Image.open(folder/(name+'.png')).convert('RGB'))/255.
def enc(x):return np.where(x<=.0031308,12.92*x,1.055*np.maximum(x,0)**(1/2.4)-.055)
reports={k:json.loads((f/'report.json').read_text()) for k,f in [('raw',raw),('synthetic',syn)]}
for kind,r in reports.items():
    check(kind+' shader domain',r['shader_domain_probe']['pass'],r['shader_domain_probe'])
    d=r['device'];check(kind+' heldout device',d['heldout_pass'],{k:d[k] for k in ['heldout_XYZ_MAE','heldout_XYZ_max_error']})
for stock,old in [('phoenix_i','harman_phoenix_200_exp_v1'),('phoenix_ii','harman_phoenix_ii_200_exp_v1')]:
    a=readf(syn,'research_'+stock).astype(float);b=readf(syn,'round5_'+stock).astype(float)
    check(stock+' neutral reconstruction',np.max(np.ptp(a[0],axis=-1)/np.maximum(np.mean(a[0],axis=-1),1e-9))<.005,{'max_relative_channel_spread':float(np.max(np.ptp(a[0],axis=-1)/np.maximum(np.mean(a[0],axis=-1),1e-9)))})
    Y=a@np.array([.2880402,.7118741,.0000857]);diff=np.diff(Y,axis=1)
    check(stock+' monotonic neutral RGB biased ramps',np.min(diff[:4])>=-1e-5,{'minimum_Y_step':float(diff[:4].min())})
    curvature=np.abs(np.diff(diff,axis=1))/np.maximum(.184,np.abs(Y[:,1:-1]))
    check(stock+' continuity',curvature.max()<.25,{'max_normalized_second_difference':float(curvature.max())})
    check(stock+' tone axis',reports['synthetic']['tone_'+stock]['neutral_Y_strictly_decreasing'],reports['synthetic']['tone_'+stock])
    for kind,folder in [('synthetic',syn),('raw',raw)]:
        v=readf(folder,'research_'+stock);check(kind+' '+stock+' finite',np.isfinite(v).all(),{'nonfinite':int(np.count_nonzero(~np.isfinite(v))),'negative_channels':int((v<0).sum()),'above_one_channels':int((v>1).sum())})
        check(kind+' '+stock+' legacy repeat',reports[kind]['legacy_repeat_'+stock]['exact'],reports[kind]['legacy_repeat_'+stock])
        # Capture the historical baseline independently, comparing exact float buffers.
        if kind=='raw':
            prior=Path('build/phoenix-phase0-raw-02')/(old+'-scan-linear-prophoto.f32')
            check(stock+' historical legacy float preservation',sha(prior)==sha(folder/('round5_'+stock+'.f32')),{'prior':sha(prior),'current':sha(folder/('round5_'+stock+'.f32'))})
        # Match the exporter's ProPhoto quantization and baked display matrix.
        # Final PNG channel endpoints are separately reported as quantized display clipping.
        aa=image(folder,'research_'+stock);bb=image(folder,'round5_'+stock)
        ca=np.mean(np.any((aa<=0)|(aa>=1),axis=-1));cb=np.mean(np.any((bb<=0)|(bb>=1),axis=-1))
        check(kind+' '+stock+' display clipping',ca<=cb+.001,{'research_RGB8_endpoint_fraction':float(ca),'legacy_RGB8_endpoint_fraction':float(cb),'limitation':'quantized endpoint measure; linear negative and headroom counts are reported separately'})
    lab=rgb2lab(image(syn,'research_'+stock));ch=np.linalg.norm(lab[...,1:],axis=-1);hue=np.degrees(np.arctan2(lab[...,2],lab[...,1]));dh=np.abs((np.diff(hue[4:],axis=1)+180)%360-180);mask=(ch[4:,:-1]>2)&(ch[4:,1:]>2)
    maxh=float(dh[mask].max()) if mask.any() else 0
    check(stock+' synthetic skin hue continuity',maxh<15,{'max_adjacent_hue_degrees':maxh,'target':'consistency only; no true skin fit'})
    x=np.arange(97)*.125-6;fig,ax=plt.subplots(1,2,figsize=(11,4));ax[0].plot(x,a[0]);ax[0].set_title(stock+' neutral RGB');ax[0].set_yscale('log');ax[1].plot(x,Y.T);ax[1].set_title('Neutral / biased / skin-like Y');ax[1].set_yscale('log');fig.tight_layout();fig.savefig(root/(stock+'-ramps.png'));plt.close(fig)
    aa=image(raw,'research_'+stock);bb=image(raw,'round5_'+stock);delta=deltaE_ciede2000(rgb2lab(aa),rgb2lab(bb));Image.fromarray(np.uint8(np.clip(np.abs(aa-bb)*10,0,1)*255)).save(raw/('difference10x_'+stock+'.png'))
    roi=(slice(int(aa.shape[0]*.56),int(aa.shape[0]*.62)),slice(int(aa.shape[1]*.28),int(aa.shape[1]*.34)))
    for name,im in [('research',aa),('round5',bb)]:Image.fromarray(np.uint8(im[roi]*255)).resize((240,180)).save(raw/(name+'_skin_'+stock+'.png'))
    reports['raw']['comparison_'+stock]={'mean_deltaE00_D65':float(delta.mean()),'p95_deltaE00_D65':float(np.percentile(delta,95)),'skin_ROI_research_Lab_D65':rgb2lab(aa[roi]).mean((0,1)).tolist(),'skin_ROI_round5_Lab_D65':rgb2lab(bb[roi]).mean((0,1)).tolist(),'interpretation':'software difference, not film accuracy; same manually inspected Sony ROI as Phase 2'}

passed=all(c['passed'] for c in checks)
result={'status':'PASS_ENGINEERING' if passed else 'REJECTED_ENGINEERING','checks':checks,'reports':reports,'policy':'one candidate then stop; no subjective tuning','gui_promotion':passed}
(root/'metrics_and_settings.json').write_text(json.dumps(result,indent=2,allow_nan=False),encoding='utf-8')
title='Research v0.1 — '+result['status']
page='<!doctype html><meta charset="utf-8"><title>'+title+'</title><style>body{background:#202020;color:#eee;font:16px system-ui;margin:24px}main{display:grid;grid-template-columns:repeat(3,1fr);gap:12px}img{width:100%}figure{margin:0}pre{white-space:pre-wrap}a{color:#adcfff}</style><h1>'+title+'</h1><p>Actual Sony RAW / NVIDIA Vulkan · scan-only · EV 0 · same shoulder / sRGB. Experimental hypothesis; no real-film accuracy claim. One candidate, no look tuning.</p><main>'
for name in ['digital_baseline','round5_phoenix_i','research_phoenix_i','digital_baseline','round5_phoenix_ii','research_phoenix_ii','difference10x_phoenix_i','difference10x_phoenix_ii','research_skin_phoenix_i','round5_skin_phoenix_i','research_skin_phoenix_ii','round5_skin_phoenix_ii']:
    page+='<figure><figcaption>'+name+'</figcaption><img src="raw/'+name+'.png"></figure>'
page+='</main><h2>Exposure diagnostics</h2><main>'
for stock in ['phoenix_i','phoenix_ii']:page+='<img src="'+stock+'-ramps.png">'
page+='</main><h2>Engineering gates</h2><pre>'+html.escape(json.dumps(checks,indent=2))+'</pre><p><a href="metrics_and_settings.json">Full settings, source/output hashes and float diagnostics</a></p>'
(root/'delta_and_side_by_side.html').write_text(page,encoding='utf-8')
inventory={str(p.relative_to(root)).replace('\\','/'):sha(p) for p in root.rglob('*') if p.is_file() and p.name!='sha256.json'}
(root/'sha256.json').write_text(json.dumps(inventory,indent=2),encoding='utf-8')
print(result['status']);print(json.dumps([c for c in checks if not c['passed']],indent=2))
