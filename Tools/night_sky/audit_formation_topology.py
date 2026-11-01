"""Inspect connected 3D bodies in the engine's own GPU noise readback."""
from pathlib import Path
from collections import deque
import json,sys
source=Path(sys.argv[1]);raw=source.read_bytes();size=64;count=size**3
assert len(raw)==count*4
field=[]
for i in range(0,len(raw),4):
    fbm=(raw[i+1]*.625+raw[i+2]*.25+raw[i+3]*.125)/255
    native=(raw[i]/255+1-fbm)/(2-fbm)
    field.append(max(0,min(1,(native-.602444)/(.858194-.602444))))
results=[]
for threshold in (.25,.40,.55,.70):
    mask=bytearray(v>threshold for v in field);occupied=sum(mask);areas=[]
    for start in range(count):
        if not mask[start]:continue
        mask[start]=0;todo=deque([start]);area=0
        while todo:
            i=todo.popleft();area+=1
            x=i%size;y=(i//size)%size;z=i//(size*size)
            neighbors=(i+(1 if x<size-1 else 1-size),i+(-1 if x>0 else size-1),
                i+(size if y<size-1 else size*(1-size)),i+(-size if y>0 else size*(size-1)),
                i+(size*size if z<size-1 else size*size*(1-size)),
                i+(-size*size if z>0 else size*size*(size-1)))
            for j in neighbors:
                if mask[j]:mask[j]=0;todo.append(j)
        areas.append(area)
    areas.sort(reverse=True)
    results.append(dict(threshold=threshold,occupied_fraction=occupied/count,
        components=len(areas),largest_component_fraction=areas[0]/max(1,occupied),
        tiny_component_volume=sum(a for a in areas if a<8)/max(1,occupied),
        largest_component_voxels=areas[:8]))
source.with_name("body-topology.json").write_text(json.dumps(results,indent=2),encoding="utf-8")
print(json.dumps(results,indent=2))
