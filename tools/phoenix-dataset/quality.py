"""Offline multidimensional quality and duplicate screening; no auto-acceptance."""
import json, hashlib
from pathlib import Path
from PIL import Image, ImageOps
import numpy as np
base=Path('research/phoenix-reference-dataset')
path=base/'dataset_manifest.jsonl'
rows=[json.loads(line) for line in path.read_text(encoding='utf-8').splitlines()]
seen={}
hashes=[]
for row in rows:
    row['quality_score']=None
    local=Path(row.get('local_file',str(Path('build/phoenix-reference-dataset/images')/(row['id']+'.jpg'))))
    if not local.exists():
        continue
    data=local.read_bytes()
    digest=hashlib.sha256(data).hexdigest()
    with Image.open(local) as image:
        gray=np.asarray(ImageOps.grayscale(image).resize((9,8)))
        dh=sum(int(value)<<i for i,value in enumerate((gray[:,1:]>gray[:,:-1]).ravel()))
        row['quality_dimensions']={**row.get('quality_dimensions',{}),
            'resolution':{'width':image.width,'height':image.height},
            'film_identity':row.get('confidence'), 'scanner':row['scanner'],
            'color_encoding':row['color_profile'],
            'scene_applicability':row['scene_category'],
            'compression':'JPEG; upstream compression unknown',
            'watermark':row.get('quality_dimensions',{}).get('watermark','not yet reviewed'),
            'motion_blur':row.get('quality_dimensions',{}).get('motion_blur','not yet reviewed')}
    row['sha256']=digest
    row['dhash64']=f'{dh:016x}'
    row['exact_duplicate_of']=seen.get(digest)
    row['near_duplicate_candidates']=[identifier for identifier,h in hashes if (h^dh).bit_count()<=6]
    seen[digest]=row['id']
    hashes.append((row['id'],dh))
    row['allowed_uses']=row.get('allowed_uses',['quarantine'])
path.write_text(''.join(json.dumps(row,ensure_ascii=False)+'\n' for row in rows),encoding='utf-8')
print('screened local files:',len(hashes),'metadata rows:',len(rows))
