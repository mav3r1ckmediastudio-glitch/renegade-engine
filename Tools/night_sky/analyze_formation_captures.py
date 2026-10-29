"""Cloud-only opacity metrics; no RGB brightness thresholds."""
from pathlib import Path
from PIL import Image, ImageDraw
import csv, math, json, sys
from collections import deque
root=Path(sys.argv[1])
rows=list(csv.DictReader((root/'captures.csv').open()))
metrics=[]
for row in rows:
    name=row['name']
    png=root/(name+'-opacity.png')
    if png.exists():
        alpha=Image.open(png).convert('RGBA').getchannel('A')
    else:
        alpha=Image.open(root/(name+'-opacity.pgm')).convert('L')
    alpha=alpha.resize((320,180),Image.Resampling.NEAREST)
    w,h=alpha.size;data=list(alpha.get_flattened_data())
    p=math.radians(float(row['pitch']))
    sky=[math.sin(p)+(1-2*(y+.5)/h)*math.tan(math.radians(35))*math.cos(p)>.02 for y in range(h)]
    eligible=sum(sky)*w
    mask=[data[i]>=13 and sky[i//w] for i in range(w*h)]
    opaque=sum(data[i]>=128 and sky[i//w] for i in range(w*h))
    seen=set();areas=[]
    for i,active in enumerate(mask):
        if not active or i in seen:continue
        seen.add(i);todo=deque([i]);size=0
        while todo:
            a=todo.popleft();size+=1;x=a%w;y=a//w
            for b in (a-1 if x else -1,a+1 if x<w-1 else -1,a-w if y else -1,a+w if y<h-1 else -1):
                if b>=0 and mask[b] and b not in seen:seen.add(b);todo.append(b)
        areas.append(size)
    edges=sum(mask[y*w+x]!=mask[y*w+x+1] for y in range(h) if sky[y] for x in range(w-1))
    edges+=sum(mask[y*w+x]!=mask[(y+1)*w+x] for y in range(h-1) if sky[y] and sky[y+1] for x in range(w))
    result=dict(row)
    result.update(sky_fraction_alpha05=sum(mask)/max(1,eligible),
        sky_fraction_alpha50=opaque/max(1,eligible),components=len(areas),
        tiny_components=sum(a<9 for a in areas),
        tiny_area_fraction=sum(a for a in areas if a<9)/max(1,sum(areas)),
        largest_cloud_fraction=max(areas,default=0)/max(1,eligible),
        edge_transitions_per_sky_pixel=edges/max(1,eligible))
    metrics.append(result)
(root/'opacity-metrics.json').write_text(json.dumps(metrics,indent=2))
with (root/'opacity-metrics.csv').open('w',newline='') as file:
    writer=csv.DictWriter(file,fieldnames=metrics[0].keys());writer.writeheader();writer.writerows(metrics)
sheet=Image.new('RGB',(1600,6*249),(20,20,24));draw=ImageDraw.Draw(sheet)
labels=['LEGACY','CUMULUS','TOWERING CUMULUS','STRATOCUMULUS','OVERCAST','CIRRUS']
for mode in range(6):
    for col,(coverage,hour) in enumerate(((80,12),(100,12),(80,0),(100,0))):
        name=f'type{mode}-c{coverage}-h{hour}-p35'
        if not (root/(name+'.png')).exists(): continue
        image=Image.open(root/(name+'.png')).convert('RGB').resize((400,225))
        sheet.paste(image,(col*400,mode*249+24))
        draw.text((col*400+6,mode*249+6),f'{labels[mode]} {coverage}% '+('DAY' if hour else 'NIGHT'),fill='white')
sheet.save(root/'formations-contact-sheet.jpg',quality=90)
summary=[{k:m[k] for k in ['name','sky_fraction_alpha05','sky_fraction_alpha50','tiny_components','tiny_area_fraction','gpu_frame_ms']}
    for m in metrics if '-matrix' not in m['name'] and '-move' not in m['name']]
print(json.dumps(summary,indent=2))
