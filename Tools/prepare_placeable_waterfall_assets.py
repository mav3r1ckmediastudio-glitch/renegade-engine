"""Original deterministic G13B granite and atomised-water sprites; no external art."""
from pathlib import Path
from PIL import Image
import random,math,sys
out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
def field(seed,size=256):
 rng=random.Random(seed);layers=[]
 for n in (4,9,21,48,110):
  im=Image.new('L',(n,n));im.putdata([rng.randrange(256) for _ in range(n*n)])
  layers.append(list(im.resize((size,size),Image.Resampling.BICUBIC).getdata()))
 return [sum(layer[i]*w for layer,w in zip(layers,(.38,.27,.18,.11,.06)))/255 for i in range(size*size)]
n=field(1302);fine=field(443);size=256
for name,mist in [('spray',False),('mist',True)]:
 im=Image.new('RGBA',(size,size));pixels=[]
 for y in range(size):
  for x in range(size):
   i=y*size+x;u=(x+.5)/size*2-1;v=(y+.5)/size*2-1
   envelope=max(0,1-u*u-v*v)**(1.7 if mist else 1.1)
   texture=max(0,min(1,(n[i]-.24)*2.6))
   if not mist:texture*=max(0,min(1,(fine[i]-.32)*4))
   alpha=envelope*texture*(.62 if mist else .95)
   pixels.append((232,242,246,int(alpha*255)))
 im.putdata(pixels);im.save(out/(name+'.png'))
rock=Image.new('RGB',(size,size));rock.putdata([(int(58+46*a+8*b),int(60+47*a+8*b),int(58+44*a+8*b)) for a,b in zip(n,fine)]);rock.save(out/'rock_original.png')
print('G13B_ORIGINAL_GRANITE_SPRAY_MIST_READY')

# Stage original project-bound artwork; never fetch from commercial packages.
import shutil
root=Path(__file__).resolve().parents[1]
for name in ('AeratedWater.png','ImpactSplash.png','SteamMist.png','WaterWisp.png'):
 source=root/'EngineBridge/assets/waterfall-v2'/name
 if source.resolve()!=(out/name).resolve():shutil.copy2(source,out/name)
shutil.copy2(root/'WickedEngine/Content/models/waterNormalMap.jpg',out/'waterNormalMap.jpg')
