"""Fetch explicitly captioned Phoenix II images, not guessed full-size endpoints."""
import hashlib,io,json,re,time,urllib.robotparser
from pathlib import Path
from urllib.parse import urlparse
import requests
from bs4 import BeautifulSoup
from PIL import Image
def main():
    source='https://www.taiyous.com/blog/harman-phoenix-ii-film-impressions'
    agent='SpektraLabResearch/0.4';session=requests.Session();session.headers['User-Agent']=agent
    policies={}
    def permitted(url):
        parsed=urlparse(url);root=parsed.scheme+'://'+parsed.netloc
        if parsed.scheme!='https':return False
        if root not in policies:
            response=session.get(root+'/robots.txt',timeout=12)
            if response.status_code==404:policies[root]=None
            elif response.status_code==200:
                rp=urllib.robotparser.RobotFileParser();rp.parse(response.text.splitlines());policies[root]=rp
            else:raise ValueError('robots unavailable')
        return policies[root] is None or policies[root].can_fetch(agent,url)
    if not permitted(source):raise ValueError('robots disallows article')
    response=session.get(source,timeout=15);response.raise_for_status();soup=BeautifulSoup(response.text,'html.parser')
    Path('build/phoenix-reference-dataset/taiyou-page.html').write_text(response.text,encoding='utf-8')
    manifest=Path('research/phoenix-reference-dataset/dataset_manifest.jsonl');rows=[json.loads(x) for x in manifest.read_text(encoding='utf-8').splitlines()]
    log=Path('build/phoenix-reference-dataset/taiyou-attempts.jsonl')
    for image in soup.find_all('img'):
        figure=image.find_parent('figure')
        alt=image.get('alt','')+' '+(figure.get_text(' ',strip=True) if figure else '')
        if not re.search(r'ph(?:oe|eo)nix.*(?:200\s*ii|ii)',alt,re.I):continue
        url=image.get('data-src') or image.get('src')
        if not url or 'images.squarespace-cdn.com' not in url:continue
        # URL is published by the page; do not synthesize original-size variants.
        ident='taiyou-'+hashlib.sha256(url.encode()).hexdigest()[:12]
        if any(r.get('id')==ident or r.get('image_url')==url for r in rows):continue
        event={'id':ident,'source_url':source,'image_url':url}
        try:
            if not permitted(url):raise ValueError('robots disallows image')
            response=session.get(url,timeout=20);response.raise_for_status();data=response.content
            if len(data)>25_000_000:raise ValueError('size limit')
            im=Image.open(io.BytesIO(data));im.load();digest=hashlib.sha256(data).hexdigest()
            duplicate=next((r for r in rows if r.get('sha256')==digest),None)
            if duplicate:event.update(status='duplicate',duplicate_of=duplicate['id'])
            else:
                ext='.png' if im.format=='PNG' else '.jpg';path=Path('build/phoenix-reference-dataset/images')/(ident+ext)
                with path.open('xb') as file:file.write(data)
                rows.append({'id':ident,'source_url':source,'original_source_url':source,'image_url':url,'film_stock':'Phoenix II 200','photographer':'Taiyou Vt','scene_category':'unreviewed','lighting':'unknown','scanner':'unknown; Brooktree Film Lab credited','post_processing':'unknown','license':'not verified; private reference only; no redistribution','color_profile':'embedded ICC' if im.info.get('icc_profile') else 'untagged; explicit assumption required','identity_verified':True,'identity_evidence':'article individual figure caption or alt identifies Phoenix II; explicit author attribution','trust_grade':'B','download_status':'downloaded','local_file':str(path),'sha256':digest,'width':im.width,'height':im.height,'quality_score':None,'allowed_uses':['quarantine'],'split':'unassigned','redistribute':False,'accepted_for_fitting':False,'image_variant':'published article image URL; not guaranteed original scan'})
                event.update(status='downloaded',dimensions=list(im.size))
                manifest.write_text(''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in rows),encoding='utf-8')
        except Exception as error:event.update(status='failed',error=str(error))
        with log.open('a',encoding='utf-8') as file:file.write(json.dumps(event)+'\n')
        print(ident,event['status']);time.sleep(1.2)
if __name__=='__main__':main()
