import urllib.request,urllib.parse,json,time
from pathlib import Path
out=Path('build/phoenix-reference-dataset/discovery');out.mkdir(exist_ok=True)
for name,q in [('commons-I','"Harman Phoenix"'),('commons-II','"Harman Phoenix II"')]:
 url='https://commons.wikimedia.org/w/api.php?'+urllib.parse.urlencode(dict(action='query',format='json',generator='search',gsrsearch=q,gsrnamespace=6,gsrlimit=50,prop='imageinfo',iiprop='url|extmetadata|size'))
 try:
  req=urllib.request.Request(url,headers={'User-Agent':'SpektraLabResearch/0.2 (noncommercial provenance study)'})
  with urllib.request.urlopen(req,timeout=20) as r:data=r.read()
  (out/(name+'.json')).write_bytes(data);print(name,len(json.loads(data).get('query',{}).get('pages',{})))
 except Exception as e:(out/(name+'-error.txt')).write_text(str(e));print(name,str(e))
 time.sleep(1)
