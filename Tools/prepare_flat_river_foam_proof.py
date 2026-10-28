"""Stage experimental flat-water foam; never deploy to Studio/Runtime."""
from pathlib import Path
import hashlib, shutil
r=Path(__file__).resolve().parents[1]
out=r/'BUILD/flat-river-foam-20261022'
out.mkdir(parents=True,exist_ok=True)
base=(r/'WickedEngine/WickedEngine/shaders/objectHF.hlsli').read_text()
marker='\tApplyLighting(surface, lighting, color);'
assert base.count(marker)==1
foam=(r/'EngineBridge/shaders/RiverFoamBlock.hlsli').read_text()
(out/'objectHF.hlsli').write_text(base.replace(marker,foam+'\n'+marker))
wrapper='#define OBJECTSHADER_COMPILE_PS\n#define OBJECTSHADER_LAYOUT_COMMON\n#define TILEDFORWARD\n#define WATER\n#define TRANSPARENT\n'
wrapper+='#include "'+(out/'objectHF.hlsli').as_posix()+'"\n'
(out/'FlatRiverFoamPS.hlsl').write_text(wrapper)
source=Path.home()/'Downloads/mossy_rocks_low_poly.glb'
assert hashlib.sha256(source.read_bytes()).hexdigest()=='e4d5b0fb924a3ffeca704cceb1ee04c58ee6034dc93cad14df7b8c1cefa21a5e'
shutil.copy2(source,out/source.name)
(out/'ATTRIBUTION.txt').write_text('Mossy Rocks Low Poly by mdsz13 (https://sketchfab.com/shaz13)\nCC BY 4.0 https://creativecommons.org/licenses/by/4.0/\nhttps://sketchfab.com/3d-models/mossy-rocks-low-poly-6823a52d39ec40be9a911384f662a466\nOriginal GLB unchanged; test placement and scale normalized at runtime.\n')
print('Prepared isolated flat-water foam shader and rock asset')
