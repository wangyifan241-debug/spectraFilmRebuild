"""Bounded single-page fetch. Honor robots, no login or endpoint guessing."""
import csv,hashlib,io,json,time,urllib.robotparser,sys
from pathlib import Path
from urllib.parse import urlparse
import requests
from bs4 import BeautifulSoup
from PIL import Image
BASE=Path('research/phoenix-reference-dataset')
AGENT='SpektraLabResearch/0.4'
def main():
    manifest=BASE/'dataset_manifest.jsonl'
    rows=[json.loads(x) for x in manifest.read_text(encoding='utf-8').splitlines()]
    entries=list(csv.DictReader(Path('research/phoenix_ii_expansion_kit/phoenix_ii_source_expansion.csv').open(encoding='utf-8-sig')))
    policy={};session=requests.Session();session.headers['User-Agent']=AGENT
    log=Path('build/phoenix-reference-dataset/provided-single-attempts.jsonl')
    def allowed(url):
        parsed=urlparse(url);root=parsed.scheme+'://'+parsed.netloc
        if parsed.scheme!='https':return False
        if root not in policy:
            response=session.get(root+'/robots.txt',timeout=12)
            if response.status_code==404:policy[root]=None
            elif response.status_code==200:
                rp=urllib.robotparser.RobotFileParser();rp.parse(response.text.splitlines());policy[root]=rp
            else:raise ValueError('robots unavailable HTTP '+str(response.status_code))
        return policy[root] is None or policy[root].can_fetch(AGENT,url)
    for entry in entries:
        if entry['type']!='single':continue
        if len(sys.argv)>1 and entry['id'] not in sys.argv[1:]:continue
        record=next((r for r in rows if r.get('id')==entry['id']),None)
        if record and record.get('download_status')=='downloaded':continue
        result={'id':entry['id'],'source_url':entry['source_page_url'],'started_unix':time.time()}
        try:
            url='https://www.lomography.com/photos/28956950' if entry['id']=='lomo-lovedlx' else entry['source_page_url']
            if not allowed(url):raise ValueError('robots disallows page')
            response=session.get(url,timeout=15);response.raise_for_status()
            soup=BeautifulSoup(response.text,'html.parser')
            # Require an actual Film label, not only a film-gallery URL.
            identity=any('Harman Phoenix II 200' in node.get_text(' ',strip=True) for node in soup.select('a'))
            if not identity:raise ValueError('exact Phoenix II identity missing from page')
            meta=soup.find('meta',property='og:image')
            if not meta or not meta.get('content'):raise ValueError('no published image')
            image_url=meta['content'];candidates=[(0,image_url)]
            # Prefer explicitly published same-photo srcset sizes over social preview.
            for image in soup.find_all('img'):
                if 'lomography' not in image.get('src',''):continue
                if image.get('src')==image_url:
                    for part in image.get('srcset','').split(','):
                        bits=part.strip().split()
                        if len(bits)==2 and bits[1].endswith('w'):candidates.append((int(bits[1][:-1]),bits[0]))
            image_url=max(candidates)[1]
            if not allowed(image_url):raise ValueError('robots disallows image')
            response=session.get(image_url,timeout=20);response.raise_for_status()
            data=response.content
            if len(data)>25_000_000:raise ValueError('file size limit')
            image=Image.open(io.BytesIO(data));image.load()
            ext={'JPEG':'.jpg','PNG':'.png','WEBP':'.webp'}.get(image.format)
            if not ext:raise ValueError('unsupported image format')
            destination=Path('build/phoenix-reference-dataset/images')/(entry['id']+ext)
            if destination.exists():
                if destination.read_bytes()!=data:raise ValueError('existing original differs; retained')
            else:
                with destination.open('xb') as file:file.write(data)
            record={'id':entry['id'],'source_url':url,'original_source_url':url,'image_url':image_url,'film_stock':'Phoenix II 200','photographer':entry['photographer_or_group'],'scene_category':'unreviewed','lighting':'unknown','scanner':'unknown','post_processing':entry['notes'],'license':'unknown; public personal-use page, redistribution not verified','color_profile':'embedded ICC' if image.info.get('icc_profile') else 'untagged; explicit assumption required','quality_score':None,'identity_verified':True,'identity_evidence':'explicit film link in individual page; platform assertion, not measured proof','trust_grade':'B','download_status':'downloaded','local_file':str(destination),'sha256':hashlib.sha256(data).hexdigest(),'width':image.width,'height':image.height,'allowed_uses':['quarantine'],'split':'unassigned','redistribute':False,'accepted_for_fitting':False,'image_variant':'published social image or matching published srcset; not guaranteed original scan'}
            rows=[r for r in rows if r['id']!=record['id']]+[record]
            result.update(status='downloaded',dimensions=list(image.size),sha256=record['sha256'])
            manifest.write_text(''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in rows),encoding='utf-8')
        except Exception as error:result.update(status='failed',error=str(error))
        with log.open('a',encoding='utf-8') as file:file.write(json.dumps(result)+'\n')
        print(entry['id'],result['status'],result.get('error',''));time.sleep(1.2)
if __name__=='__main__':main()
