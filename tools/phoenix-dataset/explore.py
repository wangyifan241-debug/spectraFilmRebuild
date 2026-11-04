"""Per-image exploration only. Never estimates a film response from unrelated scenes."""
import io,json,hashlib
from pathlib import Path
import numpy as np
from PIL import Image,ImageCms
from skimage.color import rgb2lab
base=Path('research/phoenix-reference-dataset');rows=[json.loads(x) for x in (base/'dataset_manifest.jsonl').read_text(encoding='utf-8').splitlines()];out=[]
for r in rows:
 p=Path('build/phoenix-reference-dataset/images')/(r['id']+'.jpg')
 if not p.exists() or not r.get('visual_review_date') or 'color_exploratory' not in r.get('allowed_uses',[]):continue
 original=p.read_bytes();im=Image.open(io.BytesIO(original));icc=im.info.get('icc_profile');policy='embedded ICC -> sRGB via LittleCMS' if icc else 'EXPLICIT exploratory assumption: untagged interpreted as sRGB; not verified'
 if icc:im=ImageCms.profileToProfile(im,ImageCms.ImageCmsProfile(io.BytesIO(icc)),ImageCms.createProfile('sRGB'),outputMode='RGB')
 else:im=im.convert('RGB')
 a=np.asarray(im,dtype=float)/255;h,w=a.shape[:2];keep=np.ones((h,w),dtype=bool);mask=r.get('statistics_mask',[])
 if r['id']=='flight':
  # Entire bottom strip excluded conservatively; source file is never changed.
  mask=[{'rect_normalized':[0,.92,1,1],'reason':'copyright watermark exclusion; conservative bottom strip'}]
 for item in mask:
  x0,y0,x1,y1=item['rect_normalized'];keep[int(y0*h):int(y1*h),int(x0*w):int(x1*w)]=False
 pix=a[keep];step=max(1,len(pix)//150000);pix=pix[::step];lin=np.where(pix<=.04045,pix/12.92,((pix+.055)/1.055)**2.4);Y=lin@np.array([.2126,.7152,.0722]);lab=rgb2lab(pix.reshape(1,-1,3)).reshape(-1,3)
 lms=lin@np.array([[.4122214708,.5363325363,.0514459929],[.2119034982,.6806995451,.1073969566],[.0883024619,.2817188376,.6299787005]]).T;ok=np.cbrt(lms)@np.array([[.2104542553,.7936177850,-.0040720468],[1.9779984951,-2.4285922050,.4505937099],[.0259040371,.7827717662,-.8086757660]]).T
 r['quality_score']=None;r['quality_dimensions']={'resolution':{'width':w,'height':h},'film_identity':'author statement','scanner':r['scanner'],'color_encoding':policy,'scene_applicability':r['scene_category'],'compression':'JPEG; previous compression history unknown','watermark':'present, masked' if mask else 'not observed in visual review','motion_blur':'extensive train motion' if r['id']=='train' else 'not assessed quantitatively'};r['allowed_uses']=['color_exploratory','tone'];r['excluded_uses']=['grain','halation','validation'];r['statistics_mask']=mask
 out.append({'id':r['id'],'film_stock':r['film_stock'],'source_sha256':hashlib.sha256(original).hexdigest(),'color_policy':policy,'mask':mask,'retained_pixel_fraction':float(keep.mean()),'sampled_pixels':len(pix),'linear_Y_percentiles':np.percentile(Y,[1,5,25,50,75,95,99]).tolist(),'Lab_D65_mean':lab.mean(0).tolist(),'OKLab_mean':ok.mean(0).tolist(),'Lab_L_histogram':np.histogram(lab[:,0],bins=20,range=(0,100))[0].tolist(),'OKLab_L_histogram':np.histogram(ok[:,0],bins=20,range=(0,1))[0].tolist(),'limitation':'single scene/author, not film causality or model calibration; no semantic segmentation, grain or halation estimate'})
 assert p.read_bytes()==original
(base/'dataset_manifest.jsonl').write_text(''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in rows),encoding='utf-8');(base/'exploratory_statistics.json').write_text(json.dumps(out,indent=2),encoding='utf-8');print('Exploratory images:',len(out))
