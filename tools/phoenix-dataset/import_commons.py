import json,urllib.request,re,hashlib,io
from pathlib import Path
from PIL import Image
base=Path('research/phoenix-reference-dataset');p=base/'dataset_manifest.jsonl';rows=[json.loads(x) for x in p.read_text(encoding='utf-8').splitlines()];pages=json.loads(Path('build/phoenix-reference-dataset/discovery/commons-I.json').read_text())['query']['pages']
for page in pages.values():
 info=page['imageinfo'][0];title=page['title'];meta=info['extmetadata'];desc=re.sub('<[^>]*>',' ',meta.get('ImageDescription',{}).get('value',''));url=info['descriptionurl']
 if any(r['source_url'].replace('%3A',':')==url.replace('%3A',':') or r.get('image_url')==info['url'] for r in rows):continue
 r=dict(id='commons-'+str(page['pageid']),source_url=url,image_url=info['url'],film_stock='Phoenix 200' if 'Phoenix 200' in desc and 'Phoenix II' not in desc else 'unverified',photographer=re.sub('<[^>]*>',' ',meta.get('Artist',{}).get('value','unknown')).strip(),scene_category='unreviewed',lighting='unknown',scanner='DSLR author-described' if 'DsLr' in desc else 'unknown',post_processing='LRC+PhotoLabPro author-described' if 'LRC' in desc else 'unknown',license=meta.get('LicenseShortName',{}).get('value','unknown'),color_profile='unknown',quality_score=None,confidence='Commons description; needs visual and identity review',download_status='pending',allowed_uses=['quarantine'],split='unassigned',redistribute=False)
 try:
  if r['license']!='CC BY-SA 4.0':raise ValueError('license not allowlisted')
  req=urllib.request.Request(info['url'],headers={'User-Agent':'SpektraLabResearch/0.2'})
  with urllib.request.urlopen(req,timeout=20) as response:data=response.read(30*1024*1024)
  im=Image.open(io.BytesIO(data));im.load();Path('build/phoenix-reference-dataset/images',r['id']+'.jpg').write_bytes(data);r.update(download_status='downloaded',sha256=hashlib.sha256(data).hexdigest(),width=im.width,height=im.height,color_profile='embedded ICC' if im.info.get('icc_profile') else 'untagged')
 except Exception as e:r.update(download_status='failed',download_error=str(e))
 rows.append(r)
p.write_text(''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in rows),encoding='utf-8');print([(r['id'],r['film_stock'],r['download_status']) for r in rows])
