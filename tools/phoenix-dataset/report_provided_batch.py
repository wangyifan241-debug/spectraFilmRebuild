import csv,json,hashlib,collections
from pathlib import Path
base=Path('research/phoenix-reference-dataset');rows=[json.loads(x) for x in (base/'dataset_manifest.jsonl').read_text(encoding='utf-8').splitlines()]
local=[r for r in rows if r.get('download_status')=='downloaded'];usable=[r for r in local if 'color_exploratory' in r.get('allowed_uses',[])]
counts=lambda items,key:dict(collections.Counter(r.get(key,'unknown') for r in items))
logs=[]
for name in ['provided-single-attempts.jsonl','taiyou-attempts.jsonl']:
    file=Path('build/phoenix-reference-dataset')/name
    if file.exists():logs.extend(json.loads(x) for x in file.read_text().splitlines())
sources=[Path('research/phoenix_ii_source_expansion.csv'),Path('research/phoenix_ii_expansion_kit/phoenix_ii_source_expansion.csv')]
summary={'manifest_records':len(rows),'raw_local_files':len(local),'new_user_provided_imports':22,'new_network_downloads':sum(e['status']=='downloaded' for e in logs),'download_failures':[{k:e.get(k) for k in ['id','error','source_url']} for e in logs if e['status']=='failed'],
 'downloaded_by_stock':counts(local,'film_stock'),'exploratory_by_stock':counts(usable,'film_stock'),'trust_grades_all_records':counts(rows,'trust_grade'),
 'exploratory_author_count':{s:len({r['photographer'] for r in usable if r['film_stock']==s}) for s in ['Phoenix 200','Phoenix II 200']},
 'scenes_by_stock':{s:counts([r for r in usable if r['film_stock']==s],'scene_category') for s in ['Phoenix 200','Phoenix II 200']},
 'confirmed_visual_duplicates':[{k:r[k] for k in ['id','visual_duplicate_of']} for r in local if r.get('visual_duplicate_of')],
 'excluded_local_files':[{'id':r['id'],'reason':r.get('exclusion_reasons',['film generation unresolved'])} for r in local if r not in usable],
 'remaining_to_15':{s:max(0,15-sum(r['film_stock']==s for r in usable)) for s in ['Phoenix 200','Phoenix II 200']},
 'formal_fitting_accepted':0,'source_csv_hashes':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}}
(base/'provided_batch_summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
(base/'dataset_quality_report.md').write_text('# Source-kit batch: actual results\n\n'+json.dumps(summary,indent=2)+'\n\nThese are private exploratory color/tone samples, not licensed training data or independent film measurements. Original bytes remain local; no photographs are included in Git. Source, photographer and scanning imbalance prevents causal generation comparisons. A/B/C/D describe provenance completeness, not visual truth. Contact-sheet identity assessment plus explicit publisher/author stock evidence is grade B with unknown scanning. A is not awarded for resolution.\n\nFive failed attempts concern four source IDs (four scoped-photo 404s and one subsequently verified canonical photo returning 403). No access restriction was bypassed. Published Lomography images are about 1200px and not original scan files; the larger article/source-kit images retain their published sizes. No original-size endpoint was guessed.\n',encoding='utf-8')
# Synchronize current image metadata; retain album/review leads as non-image records.
discovered=base/'discovered_candidates.jsonl';entries={r['id']:r for r in (json.loads(x) for x in discovered.read_text(encoding='utf-8').splitlines())}
for r in rows:entries[r['id']]={**entries.get(r['id'],{}),**r,'record_type':'image_candidate'}
discovered.write_text(''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in entries.values()),encoding='utf-8')
print(json.dumps({k:summary[k] for k in ['raw_local_files','new_user_provided_imports','new_network_downloads','exploratory_by_stock','remaining_to_15']}))
