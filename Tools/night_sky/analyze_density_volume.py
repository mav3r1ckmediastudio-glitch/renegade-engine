"""Analyze the actual unlit GPU density volume. No visual acceptance claim."""
from pathlib import Path
from array import array
import json,math,sys
from PIL import Image,ImageDraw
root=Path(sys.argv[1]); reports=[]
n=128;plane=n*n;count=n**3;threshold=.005
for folder in (root/"c0",root/"c80",root/"c100"):
    meta=json.loads((folder/"volume.json").read_text(encoding="utf-8"))
    # Version-zero audit fixtures explicitly used 2280/3190.
    y_extent=meta.get("y_metres",[2280,5470])
    depth=y_extent[1]-y_extent[0]
    data=array("f")
    data.frombytes((folder/"density-r32f-128x128x128.bin").read_bytes())
    if sys.byteorder!="little":data.byteswap()
    assert len(data)==count and all(math.isfinite(v) and v>=0 for v in data)
    mask=bytearray(v>=threshold for v in data)
    occupied=sum(mask);seen=bytearray(count);components=[]
    for start in range(count):
        if not mask[start] or seen[start]:continue
        queue=array("I",[start]);seen[start]=1;head=0;mass=0;anchored=False
        while head<len(queue):
            i=queue[head];head+=1
            x=i%n;y=(i//n)%n;z=i//plane
            mass+=data[i]
            # Bottom 150m is the condensation-root neighbourhood. This is an
            # empirical topology diagnostic, not proof of dynamic convection.
            if (y+.5)*depth/n<150:anchored=True
            neighbours=[]
            if x:neighbours.append(i-1)
            if x<n-1:neighbours.append(i+1)
            if y:neighbours.append(i-n)
            if y<n-1:neighbours.append(i+n)
            if z:neighbours.append(i-plane)
            if z<n-1:neighbours.append(i+plane)
            for j in neighbours:
                if mask[j] and not seen[j]:seen[j]=1;queue.append(j)
        components.append({"voxels":len(queue),"density_sum":mass,"base_connected":anchored})
    components.sort(key=lambda c:c["voxels"],reverse=True)
    total_mass=sum(c["density_sum"] for c in components)
    report={"case":folder.name,"threshold":threshold,"grid":[n,n,n],
        "x_z_extent_metres":[-6000,6000],"y_extent_metres":y_extent,
        "density_min":min(data),"density_max":max(data),"occupied_fraction":occupied/count,
        "components":len(components),"largest_component_fraction":
            components[0]["voxels"]/occupied if occupied else 0,
        "unsupported_density_mass_fraction":sum(c["density_sum"] for c in components
            if not c["base_connected"])/total_mass if total_mass else 0,
        "largest_components":components[:12],
        "limit":"6-neighbour voxel connectivity; fixed density LOD 0, 93.75m horizontal and 24.92m vertical voxels; subvoxel links can be missed."}
    reports.append(report)
    sheet=Image.new("RGB",(768,286),"#101820");draw=ImageDraw.Draw(sheet)
    for axis in range(3):
        im=Image.new("L",(n,n))
        for b in range(n):
            for a in range(n):
                if axis==0:idx=(n//2)*plane+(n-1-b)*n+a
                elif axis==1:idx=(n-1-b)*plane+8*n+a
                else:idx=a*plane+(n-1-b)*n+n//2
                im.putpixel((a,b),round(min(1,data[idx]/.1)*255))
        physical_height=256 if axis==1 else max(1,round(256*depth/12000))
        sheet.paste(im.resize((256,physical_height)),
            (axis*256,30+(256-physical_height)//2))
        draw.text((axis*256+6,8),
            ("XY at Z=+47m","XZ at base+212m","ZY at X=+47m")[axis],fill="white")
    sheet.save(folder/"density-slices.png")
(root/"density-topology.json").write_text(json.dumps(reports,indent=2),encoding="utf-8")
for r in reports:print(json.dumps({k:v for k,v in r.items() if k!="largest_components"},indent=2))
