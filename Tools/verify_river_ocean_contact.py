from pathlib import Path
from PIL import Image,ImageChops,ImageStat
import json
r=Path(__file__).resolve().parents[1]
out=r/'BUILD/ocean-contact-foam-20261023'
def diff(a,b,box=None):
    x=Image.open(out/a).convert('RGB');y=Image.open(out/b).convert('RGB')
    if box:x=x.crop(box);y=y.crop(box)
    d=ImageChops.difference(x,y)
    return {'mean_rgb':ImageStat.Stat(d).mean,'bbox':d.getbbox()}
result={}
for view in ['player','reverse','high','low']:
    result[view]=diff('native-'+view+'.png','ocean-narrow-'+view+'.png')
result['open_water']=diff('native-player.png','ocean-narrow-player.png',(80,330,350,550))
result['animation']=diff('animation-start.png','animation-end.png')
assert sum(result['player']['mean_rgb'])>.0001,'No visible contact effect'
assert sum(result['animation']['mean_rgb'])>.00001,'No animation'
assert max(result['open_water']['mean_rgb'])<.1,'Unexpected open-water change'
(out/'pixel-evidence.json').write_text(json.dumps(result,indent=2))
print(json.dumps(result,indent=2))
