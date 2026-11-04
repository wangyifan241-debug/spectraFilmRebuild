import json,urllib.request,hashlib,io
from pathlib import Path
from PIL import Image,ImageOps,ImageStat
base=Path('research/phoenix-reference-dataset');cache=Path('build/phoenix-reference-dataset/images')
entries=json.loads(Path('validation/phoenix-round5/public-references/manifest.json').read_text())
rows=[]
for e in entries:
 r=dict(id=e['id'],source_url=e['page'],original_source_url=e['original'],image_url=e['url'],film_stock=e['film'],photographer=e['author'],scene_category='unreviewed',lighting='unknown',scanner='unknown',post_processing=e['processing'],license=e['license'],license_url=e['license_url'],color_profile='unknown',quality_score=None,confidence={'film_identity':'author-described, independently rechecked source page 2026-11-04','scan':'unknown'},status='candidate',download_status='pending',split='unassigned',usage='unpaired exploratory only; no grain or physical accuracy inference',redistribute=False)
 try:
  p=cache/(e['id']+'.jpg')
  if not p.exists():
   req=urllib.request.Request(e['url'],headers={'User-Agent':'SpektraLabResearch/0.1 (noncommercial image provenance audit)'})
   with urllib.request.urlopen(req,timeout=20) as response:data=response.read(30*1024*1024)
   with Image.open(io.BytesIO(data)) as im:im.verify()
   p.write_bytes(data)
  data=p.read_bytes();im=Image.open(p);r.update(width=im.width,height=im.height,sha256=hashlib.sha256(data).hexdigest(),color_profile='embedded ICC' if im.info.get('icc_profile') else 'untagged; do not silently assume verified sRGB',download_status='downloaded',status='needs_visual_review')
 except Exception as ex:r['download_status']='failed';r['download_error']=str(ex)
 rows.append(r)
base.joinpath('dataset_manifest.jsonl').write_text(''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in rows),encoding='utf-8');print(json.dumps(rows,indent=2,ensure_ascii=False))
