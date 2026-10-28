"""Build-only copy of native WATER with pinned ocean shore foam. No upstream edits."""
from pathlib import Path
import hashlib
r=Path(__file__).resolve().parents[1]
out=r/'BUILD/ocean-contact-foam-20261023'
out.mkdir(parents=True,exist_ok=True)
source=r/'WickedEngine/WickedEngine/shaders/objectHF.hlsli'
base=source.read_text()
marker='\tApplyLighting(surface, lighting, color);'
assert base.count(marker)==1
block=(r/'EngineBridge/shaders/RiverOceanContactFoam.hlsli').read_text()
(out/'objectHF.hlsli').write_text(base.replace(marker,block+'\n'+marker))
wrapper='#define OBJECTSHADER_COMPILE_PS\n#define OBJECTSHADER_LAYOUT_COMMON\n#define TILEDFORWARD\n#define WATER\n#define TRANSPARENT\n'
wrapper+='#include "'+(out/'objectHF.hlsli').as_posix()+'"\n'
(out/'RiverOceanContactPS.hlsl').write_text(wrapper)
print('NATIVE_WATER_SHA256',hashlib.sha256(source.read_bytes()).hexdigest())
print('STAGED',out)
