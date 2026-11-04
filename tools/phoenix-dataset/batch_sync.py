"""Conservative resumable acquisition. Explicit CC license AND approved host required."""
import argparse, hashlib, io, json, os, re, time, urllib.request, urllib.error
from pathlib import Path
from PIL import Image, ImageOps
import numpy as np
BASE=Path('research/phoenix-reference-dataset')
CACHE=Path('build/phoenix-reference-dataset/images')
def canonical(url):
    match=re.search(r'flickr.com/photos/[^/]+/(\d+)',(url or '').replace('%40','@'))
    return 'flickr:'+match[1] if match else (url or '').rstrip('/')
def grade(row):
    if not row.get('source_url') or row.get('film_stock') in ('Phoenix 125','HARMAN RED'):
        return 'D'
    if not row.get('identity_verified'):
        return 'C'
    if row.get('scanner') not in (None,'unknown') and row.get('color_profile','').startswith('embedded'):
        return 'A'
    return 'B'
def save(rows):
    target=BASE/'dataset_manifest.jsonl'
    temp=target.with_suffix('.jsonl.tmp')
    temp.write_text(''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in rows),encoding='utf-8')
    for attempt in range(5):
        try:
            os.replace(temp,target)
            break
        except PermissionError:
            if attempt==4:raise
            time.sleep(.1*(attempt+1))
def run(download=False,retries=2):
    rows=[json.loads(x) for x in (BASE/'dataset_manifest.jsonl').read_text(encoding='utf-8').splitlines()]
    journal=CACHE.parent/'acquisition_journal.jsonl';CACHE.mkdir(parents=True,exist_ok=True)
    seen={};hashes=[]
    for r in rows:
        key=canonical(r.get('original_source_url') or r.get('source_url'))
        r['source_identity']=key;r['mirror_duplicate_of']=seen.get(key) if key else None
        if key:seen.setdefault(key,r['id'])
        r['trust_grade']=grade(r);r['accepted_for_fitting']=False
        local=CACHE/(r['id']+'.jpg')
        # Never redownload or rewrite an existing original.
        if not local.exists() and download and not r.get('mirror_duplicate_of') and r.get('license') in ('CC-BY-SA-4.0','CC BY-SA 4.0') and r.get('image_url','').startswith('https://upload.wikimedia.org/'):
            for attempt in range(1,retries+1):
                try:
                    req=urllib.request.Request(r['image_url'],headers={'User-Agent':'SpektraLabResearch/0.3 (noncommercial reference study)'})
                    with urllib.request.urlopen(req,timeout=15) as response:
                        data=response.read(30*1024*1024+1)
                    if len(data)>30*1024*1024:raise ValueError('size limit')
                    image=Image.open(io.BytesIO(data));image.load()
                    with local.open('xb') as file:file.write(data)
                    r['download_status']='downloaded';r.pop('download_error',None)
                    event={'id':r['id'],'attempt':attempt,'status':'downloaded'}
                except Exception as error:
                    r.update(download_status='failed',download_error=str(error))
                    event={'id':r['id'],'attempt':attempt,'status':'failed','error':str(error)}
                with journal.open('a',encoding='utf-8') as file:file.write(json.dumps({**event,'time_unix':time.time()})+'\n')
                save(rows)
                if r['download_status']=='downloaded':break
                if 'HTTP Error 403' in r.get('download_error','') or 'HTTP Error 429' in r.get('download_error',''):break
                time.sleep(attempt)
        if local.exists():
            data=local.read_bytes();digest=hashlib.sha256(data).hexdigest()
            with Image.open(local) as image:
                image.load();g=np.asarray(ImageOps.grayscale(image).resize((9,8)))
                dh=sum(int(v)<<i for i,v in enumerate((g[:,1:]>g[:,:-1]).ravel()))
                r.update(sha256=digest,dhash64=f'{dh:016x}',width=image.width,height=image.height,download_status='downloaded')
                r['color_profile']='embedded ICC' if image.info.get('icc_profile') else 'untagged; explicit assumption required'
                r['icc_sha256']=hashlib.sha256(image.info['icc_profile']).hexdigest() if image.info.get('icc_profile') else None
            r['near_duplicate_candidates']=[identifier for identifier,h in hashes if (h^dh).bit_count()<=6];hashes.append((r['id'],dh))
            r['quality_scores']={'resolution':min(4,min(r['width'],r['height'])//500),'film_identity':4 if r.get('identity_verified') else 1,'color_encoding':4 if r['icc_sha256'] else 1,'scanner_information':0 if r['scanner']=='unknown' else 2,'watermark':None,'motion_blur':None,'scene_applicability':2 if r.get('visual_review_date') else None,'compression':None}
            r['quality_score_scale']='0–4 per dimension; null=unassessed, not zero; not aggregated'
        save(rows)
    print(json.dumps({'rows':len(rows),'downloaded':sum(r.get('download_status')=='downloaded' for r in rows),'journal':str(journal)}))
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--download',action='store_true');p.add_argument('--retries',type=int,default=2);a=p.parse_args();run(a.download,a.retries)
