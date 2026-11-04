import csv,json
from pathlib import Path
base=Path('research/phoenix-reference-dataset');manifest=base/'dataset_manifest.jsonl'
rows=[json.loads(x) for x in manifest.read_text(encoding='utf-8').splitlines()];byid={r['id']:r for r in rows}
entries={r['id']:r for r in csv.DictReader(Path('research/phoenix_ii_expansion_kit/phoenix_ii_source_expansion.csv').open(encoding='utf-8-sig'))}
log=Path('build/phoenix-reference-dataset/provided-single-attempts.jsonl')
for event in (json.loads(x) for x in log.read_text().splitlines()):
    if event['status']!='failed' or byid.get(event['id'],{}).get('download_status')=='downloaded':continue
    e=entries[event['id']]
    row=byid.get(event['id'],{'id':event['id'],'source_url':e['source_page_url'],'film_stock':'unverified','film_stock_claim':'Phoenix II 200','photographer':e['photographer_or_group'],'scene_category':'unreviewed','lighting':'unknown','scanner':'unknown','post_processing':'unknown','license':'unknown','color_profile':'unknown','quality_score':None,'trust_grade':'C','allowed_uses':['quarantine'],'split':'unassigned','redistribute':False,'accepted_for_fitting':False})
    row.update(download_status='failed',download_error=event['error'],last_attempt_source_url=event['source_url'])
    if event['id'] not in byid:rows.append(row)
    byid[event['id']]=row
manifest.write_text(''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in rows),encoding='utf-8')
