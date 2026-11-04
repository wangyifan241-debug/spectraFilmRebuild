"""Apply documented human contact-sheet reviews to this source-kit batch."""
import json,csv
from pathlib import Path
base=Path('research/phoenix-reference-dataset');path=base/'dataset_manifest.jsonl'
rows=[json.loads(x) for x in path.read_text(encoding='utf-8').splitlines()]
scenes={'lomo-jaeming':'water_vegetation','lomo-fabio':'animal_portrait','lomo-ekmonks':'flowers','lomo-jason':'indoor_still_life','lomo-earley':'portrait_urban',
 'taiyou-c421cadf6004':'vegetation_shade','taiyou-ac1d77eafc48':'vegetation_shade','taiyou-55520dc648a1':'flowers','taiyou-8502f7cf96e7':'flowers','taiyou-d8f77915c993':'street_portrait','taiyou-8a11edcf50cb':'street_portrait','taiyou-572648635ba9':'flowers'}
duplicates={'P2_gab_cliffs_review':'P2_marcus_cliffs','P2_parasol_retail':'P2_naeem_parasol'}
for r in rows:
    if r.get('download_status')!='downloaded':continue
    new=r['id'].startswith(('P1_','P2_','lomo-','taiyou-'))
    if not new:continue
    r.update(identity_verified=True,trust_grade='B',identity_checked_date='2026-11-04',visual_review_date='2026-11-04',accepted_for_fitting=False,redistribute=False)
    if r['id'].startswith(('P1_','P2_')):
        r['identity_evidence']='provided source/author + individual published sample/credit + contact-sheet review; publisher attribution, not physical measurement'
    if r['id'] in scenes:r['scene_category']=scenes[r['id']]
    r['visual_review']='Actual contact sheet checked; unsuitable grain/halation recovery from these web images. Source/scan confounds retained.'
    masks=r.get('statistics_mask',[])
    if r['id'].startswith(('P1_','P2_')):
        masks=[{'rect_normalized':[0,0,1,.08],'reason':'conservative presentation-border exclusion'},
               {'rect_normalized':[0,.84,1,1],'reason':'conservative credit/logo/watermark exclusion'},
               {'rect_normalized':[0,0,.08,1],'reason':'presentation-border exclusion'},
               {'rect_normalized':[.92,0,1,1],'reason':'presentation-border exclusion'}]
    if r['id']=='lomo-jaeming':
        masks=[{'rect_normalized':[0,0,1,.06],'reason':'film edge'},{'rect_normalized':[0,.94,1,1],'reason':'film edge'},{'rect_normalized':[0,0,.06,1],'reason':'film edge'},{'rect_normalized':[.94,0,1,1],'reason':'film edge'}]
    r['statistics_mask']=masks
    r['private_exploration_only']=True
    r['rights_limitation']='publication/redistribution and formal fitting not licensed by public accessibility; original remains private'
    r['excluded_uses']=['grain','halation','validation','formal_fitting']
    r['exclusion_reasons']=[]
    if r['id'] in duplicates:
        r['visual_duplicate_of']=duplicates[r['id']];r['exclusion_reasons'].append('visually confirmed resized/cropped mirror; prefer higher-resolution copy')
    if min(r['width'],r['height'])<700 or max(r['width'],r['height'])<1000:
        r['exclusion_reasons'].append('small web preview; insufficient for this batch threshold (short edge 700, long edge 1000)')
    r['allowed_uses']=['quarantine'] if r['exclusion_reasons'] else ['color_exploratory','tone']
    r['quality_scores']={'resolution':min(4,min(r['width'],r['height'])//500),'film_identity':3,'color_encoding':4 if r.get('icc_sha256') or r['color_profile'].startswith('embedded') else 1,'scanner_information':0 if r['scanner']=='unknown' else 2,'watermark':1 if masks else 3,'motion_blur':2,'scene_applicability':2,'compression':1}
    r['quality_score']=None
    r['quality_score_scale']='0–4 independent ordinal review dimensions; not an overall film-quality measure'
    if r['id']=='P2_taiyou_flowers':r['photographer']='Taiyou Vt';r['scanner']='unknown; Brooktree Film Lab credited'
path.write_text(''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in rows),encoding='utf-8')
# Preserve album/review lead type: these are not individual image counts.
discovered=base/'discovered_candidates.jsonl';existing=[json.loads(x) for x in discovered.read_text(encoding='utf-8').splitlines()];ids={r['id'] for r in existing}
for r in rows:
    if r['id'] not in ids:existing.append(r);ids.add(r['id'])
for entry in csv.DictReader(Path('research/phoenix_ii_expansion_kit/phoenix_ii_source_expansion.csv').open(encoding='utf-8-sig')):
    if entry['id'] in ids:continue
    existing.append({'id':entry['id'],'source_url':entry['source_page_url'],'record_type':entry['type'],'photographer':entry['photographer_or_group'],'film_stock_claim':'Phoenix II 200','identity_status':'source lead only; no individual photo verified','download_status':'not_downloaded','claimed_count_not_usable_count':entry['page_claimed_count']});ids.add(entry['id'])
discovered.write_text(''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in existing),encoding='utf-8')
print('Exploratory per stock:',{stock:sum(r['film_stock']==stock and 'color_exploratory' in r.get('allowed_uses',[]) for r in rows) for stock in ['Phoenix 200','Phoenix II 200']})
