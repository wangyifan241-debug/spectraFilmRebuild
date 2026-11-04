"""Screen local licensed candidates; no automatic acceptance or random photo split."""
import json,hashlib,collections
from pathlib import Path
from PIL import Image,ImageOps
import numpy as np
base=Path('research/phoenix-reference-dataset');rows=[json.loads(x) for x in (base/'dataset_manifest.jsonl').read_text(encoding='utf-8').splitlines()];seen={};hashes=[]
for r in rows:
 p=Path('build/phoenix-reference-dataset/images')/(r['id']+'.jpg')
 if not p.exists():continue
 with Image.open(p) as im:
  g=np.asarray(ImageOps.grayscale(im).resize((9,8)),dtype=float);bits=(g[:,1:]>g[:,:-1]).ravel();dh=sum(int(v)<<i for i,v in enumerate(bits));r['dhash64']=f'{dh:016x}';r['near_duplicate_candidates']=[i for i,h in hashes if (h^dh).bit_count()<=6];hashes.append((r['id'],dh));r['exact_duplicate_of']=seen.get(r['sha256']);seen[r['sha256']]=r['id'];r['quality_score']=round(min(1,min(im.size)/1200)*100,1);r['quality_score_method']='resolution proxy only; not photographic merit';r['quality_flags']=r.get('quality_flags',[])
  if min(im.size)<800:r['quality_flags'].append('low_resolution')
  if r['color_profile'].startswith('untagged'):r['quality_flags'].append('unknown_color_encoding')
 r['split']='quarantine';r['split_reason']='only two authors from one platform: insufficient independent source groups for train/validation';r['source_group']='flickr';r['status']='reviewed_quarantine' if r.get('visual_review_date') else 'metadata_eligible_pending_visual_review'
base.joinpath('dataset_manifest.jsonl').write_text(''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in rows),encoding='utf-8')
counts=collections.Counter((r['film_stock'],r['download_status']) for r in rows)
base.joinpath('dataset_quality_report.md').write_text('# Dataset quality — first executed batch\n\n'+str(dict(counts))+'\n\nCandidates: '+str(len(rows))+'. Accepted for fitting: 0. Validation samples: 0. No image count padding. Downloaded images remain private build artifacts. Resolution scoring is automated; visual-review fields are retained where present. Both scanner identities unknown. Exact hashes and near-duplicate candidates are recorded; dHash is a screening flag, not an automatic duplicate verdict. Scene balance and photographer/source split are not satisfied by this seed batch. All rows quarantined until more independent groups and visual review.\n',encoding='utf-8')
print(dict(counts))
