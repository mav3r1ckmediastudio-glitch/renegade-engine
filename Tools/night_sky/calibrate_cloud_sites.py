"""CPU calibration of independent stratified 2D cloud-footprint support.

Each cell has one jittered site. Coverage expands complete footprints; it
never changes vertical morphology, fine noise, optical density or lighting.
"""
from pathlib import Path
import random,math,json,sys
def mix(v):
    v&=0xffffffff;v^=v>>16;v=v*0x7feb352d&0xffffffff
    v^=v>>15;v=v*0x846ca68b&0xffffffff;v^=v>>16
    return v
def corner(x,z):return (mix(((x&0xffffffff)*0x9e3779b9)^((z&0xffffffff)*0x85ebca6b))&0xffffff)/16777216
def nearest(x,z):
    ix=math.floor(x);iz=math.floor(z);best=100
    for dz in (-1,0,1):
        for dx in (-1,0,1):
            cx=ix+dx;cz=iz+dz
            sx=cx+.5+(corner(cx,cz)-.5)*.4
            sz=cz+.5+(corner(cx+37,cz-71)-.5)*.4
            best=min(best,(x-sx)**2+(z-sz)**2)
    return math.sqrt(best)
rng=random.Random(29)
values=sorted(nearest(rng.uniform(-500,500),rng.uniform(-500,500)) for _ in range(60000))
quantiles=[0]+[values[int(i/16*(len(values)-1))] for i in range(1,16)]+[1.2]
result={"samples":len(values),"seed":29,"sites_per_cell":1,"cell_metres":2000,
    "jitter_cell_fraction":.4,"distance_quantile17":quantiles,
    "note":"Horizontal footprint occupancy only; projected sky occupancy requires separate GPU calibration."}
folder=Path(sys.argv[1]);folder.mkdir(parents=True,exist_ok=True)
(folder/"site-distance-calibration.json").write_text(json.dumps(result,indent=2),encoding="utf-8")
print(json.dumps(result,indent=2))
