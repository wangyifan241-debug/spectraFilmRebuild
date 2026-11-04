import json,collections
from pathlib import Path
base=Path('research/phoenix-reference-dataset')
rows=[json.loads(x) for x in (base/'dataset_manifest.jsonl').read_text(encoding='utf-8').splitlines()]
downloaded=[r for r in rows if r.get('download_status')=='downloaded']
verified=[r for r in downloaded if r.get('identity_verified')]
stats=[r for r in verified if 'color_exploratory' in r.get('allowed_uses',[])]
report={'manifest_rows':len(rows),'trust_grades':dict(collections.Counter(r.get('trust_grade','ungraded') for r in rows)),
 'new_downloads_this_batch':1,'downloaded_total':len(downloaded),
 'verified_downloads_by_stock':dict(collections.Counter(r['film_stock'] for r in verified)),
 'exploratory_usable_by_stock':dict(collections.Counter(r['film_stock'] for r in stats)),
 'downloaded_distinct_authors':len({r['photographer'] for r in downloaded}),
 'verified_distinct_authors':len({r['photographer'] for r in verified}),
 'scenes':dict(collections.Counter(r['scene_category'] for r in stats)),
 'mirror_aliases':[{'id':r['id'],'duplicate_of':r['mirror_duplicate_of']} for r in rows if r.get('mirror_duplicate_of')],
 'excluded_missing_source':[r['id'] for r in rows if not r.get('source_url')],
 'formal_fitting_accepted':0,'target_met':False}
(base/'batch_summary.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
(base/'dataset_quality_report.md').write_text('# Actual acquisition result\n\n'+json.dumps(report,indent=2)+'\n\nTrust: A=verified stock with scanner and ICC evidence; B=verified author stock statement with acquisition/scan uncertainty; C=tag-only or unresolved identity; D=missing source/wrong product. A grade is provenance completeness, never film accuracy. The one missing-source record is retained for history but excluded from candidate use. The Flickr/Commons sunset alias is one photograph, not two.\n\nThis batch gained one downloadable forest image, generation unresolved. Verified exploration remains Phoenix 200 two images and Phoenix II one image; the per-generation 15 target is NOT met. Three newly license-checked Flickr leads returned image HTTP 403 and were not fetched through another route. HARMAN terms prohibit automated scripts; no bulk official-gallery download. Most existing Flickr rows are rights-reserved metadata.\n\nScanner unknown is allowed for exploratory color/tone with declared uncertainty. Quality is per-dimension, never aggregated; null dimensions remain unassessed. No training/validation assignment, grain or halation recovery, formal model fitting, or real-film accuracy claim.\n',encoding='utf-8')
print(json.dumps(report))
