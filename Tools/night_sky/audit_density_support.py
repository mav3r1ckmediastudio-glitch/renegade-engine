"""Inspect stock base-noise support before detail erosion and lighting.

Diagnostic, not a renderer or visual acceptance test. The input is the actual
GPU-generated RGBA8 64-cubed shape texture from --density-audit. Reports native
weather carving algebra and an unlit vertical slice of the independent field.
Usage: python audit_density_support.py RAW OUTPUT_DIRECTORY
"""
import json
import math
import sys
from pathlib import Path
from PIL import Image, ImageDraw

raw=Path(sys.argv[1]).read_bytes()
assert len(raw)==64**3*4
out=Path(sys.argv[2]); out.mkdir(parents=True,exist_ok=True)
def clamp(v): return max(0,min(1,v))
def smooth(a,b,v):
    t=clamp((v-a)/(b-a))
    return t*t*(3-2*t)
def body_at(x,y,z):
    # D3D linear-wrap texture convention: centre of first texel at .5/64.
    q=[v*64-.5 for v in (x,y,z)]
    cell=[math.floor(v) for v in q]; frac=[v-c for v,c in zip(q,cell)]
    rgba=[0.0]*4
    for dz in range(2):
        for dy in range(2):
            for dx in range(2):
                weight=math.prod(frac[i] if d else 1-frac[i]
                    for i,d in enumerate((dx,dy,dz)))
                offset=((((cell[2]+dz)%64)*64+(cell[1]+dy)%64)*64+(cell[0]+dx)%64)*4
                for i in range(4): rgba[i]+=raw[offset+i]/255*weight
    fbm=sum(a*b for a,b in zip(rgba[1:],(.625,.25,.125)))
    return clamp((rgba[0]+1-fbm)/(2-fbm))
values=[]
for i in range(0,len(raw),4):
    n=[v/255 for v in raw[i:i+4]]
    f=sum(a*b for a,b in zip(n[1:],(.625,.25,.125)))
    values.append(clamp((n[0]+1-f)/(2-f)))
# Native Remap(body,1-weather,1,0,1)*weather reduces to body+weather-1.
report={
    "scope":"base shape only; excludes height, anvil, detail, curl, light and temporal reconstruction",
    "samples":len(values),
    "native_local_weather":[{"weather":w,
        "positive_base_fraction":sum(v>1-w for v in values)/len(values),
        "mean_positive_base_density":sum(max(0,v+w-1) for v in values)/len(values)}
        for w in (.25,.4,.5,.6,.8,1.0)]
}
width,height=640,256
sheet=Image.new("RGB",(width,height*3+90),"#101820")
draw=ImageDraw.Draw(sheet)
labels=["Native base / local weather .8 (height applied)",
        "Native base / local weather .4 (height applied)",
        "Independent field / measured contrast + vertical threshold"]
counts=[0,0,0]
for y in range(height):
    u=1-(y+.5)/height
    # Slice: x -5km..5km; height within one 2km cloud profile; z=0.
    for x in range(width):
        metres=-5000+(x+.5)/width*10000
        native=body_at(metres*.0006,(2280+u*2000+metres/3)*.0006,0)
        gradient=smooth(0,.06,u)*(1-smooth(.60,1,u))
        contrast=clamp((body_at(metres/4000,(2280+u*2000)/4000,0)-.602444)/(.858194-.602444))
        independent=smooth(0,.06,u)*smooth(0,.15,contrast-(.25+.65*smooth(.05,.95,u)))*.1
        densities=[max(0,native*gradient-.2),max(0,native*gradient-.6),independent]
        for row,v in enumerate(densities):
            if v>0: counts[row]+=1
            colour=round(clamp(v/(.6 if row==0 else .15 if row==1 else .1))*255)
            sheet.putpixel((x,row*(height+30)+30+y),(colour,colour,colour))
for row,label in enumerate(labels): draw.text((8,row*(height+30)+8),label,fill="white")
report["slice"]={"x_metres":[-5000,5000],"profile_depth_metres":2000,
    "z_metres":0,"dimensions":[width,height],
    "nonzero_fraction":[v/(width*height) for v in counts],
    "note":"Algebraic diagnostic; native per-column profile variation and erosion excluded."}
sheet.save(out/"base-support-slices.png")
(out/"base-support.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
print(json.dumps(report,indent=2))
