import json,csv,hashlib,shutil,re
from pathlib import Path
from PIL import Image
base=Path('research/phoenix-reference-dataset');target=base/'dataset_manifest.jsonl';rows=[json.loads(x) for x in target.read_text(encoding='utf-8').splitlines()];kit=Path('research/phoenix_reference_fetch_kit/phoenix_reference_fetch');candidates=list(csv.DictReader((kit/'candidate_urls.csv').open(encoding='utf-8-sig')));results=[]
for entry in candidates:
 matches=list((kit/'downloaded/images').rglob(entry['id']+'.*'))
 if not matches:results.append({'id':entry['id'],'status':'missing'});continue
 source=matches[0];data=source.read_bytes();image=Image.open(source);image.load();digest=hashlib.sha256(data).hexdigest()
 existing=next((r for r in rows if r.get('sha256')==digest),None)
 if existing:results.append({'id':entry['id'],'status':'existing_hash','existing':existing['id']});continue
 destination=Path('build/phoenix-reference-dataset/images')/(entry['id']+source.suffix.lower())
 if destination.exists():assert destination.read_bytes()==data
 else:
  with destination.open('xb') as file:file.write(data)
 row={'id':entry['id'],'source_url':entry['source_url'],'image_url':entry['download_url'],'original_source_url':entry['source_url'],'film_stock':entry['film_stock'],'photographer':entry['photographer'],'scene_category':entry['scene_category'],'lighting':'unknown','scanner':'unknown','post_processing':'unknown','license':entry['license_status'],'color_profile':'embedded ICC' if image.info.get('icc_profile') else 'untagged; explicit assumption required','icc_sha256':hashlib.sha256(image.info['icc_profile']).hexdigest() if image.info.get('icc_profile') else None,'quality_score':None,'confidence':'source kit claim; per-image identity review pending','identity_verified':False,'trust_grade':'C','download_status':'downloaded','acquisition_method':'user-provided original copied byte-for-byte; no network download','local_file':str(destination),'sha256':digest,'width':image.width,'height':image.height,'allowed_uses':['quarantine'],'split':'unassigned','redistribute':False,'accepted_for_fitting':False,'notes':entry['notes'],'provided_duplicate_group':entry['duplicate_group'],'provided_file_sha256':digest}
 rows.append(row);results.append({'id':entry['id'],'status':'imported','sha256':digest,'dimensions':list(image.size)})
target.write_text(''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in rows),encoding='utf-8');Path('build/phoenix-reference-dataset/provided-import-log.json').write_text(json.dumps(results,indent=2));print('Imported',sum(x['status']=='imported' for x in results),'of',len(results))
