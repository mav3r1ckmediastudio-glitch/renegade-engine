"""Stage G7C lit foam over native WATER and accepted G3 normal advection. Never edit upstream."""
from pathlib import Path
import sys,json,hashlib
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]).resolve();out.mkdir(parents=True,exist_ok=True)
raw=(root/'WickedEngine/WickedEngine/shaders/objectHF.hlsli').read_bytes()
assert hashlib.sha256(raw).hexdigest()=='27d73178076aa7333374bef9a162e996629db01e749556881236d978daf41ce5'
base=raw.decode().replace('\r\n','\n')
native='\t\tbumpColor0 = 2 * texture_normalmap.Sample(sampler_objectshader, UV_normalMap - material.texMulAdd.ww).rg - 1;\n\t\tbumpColor1 = 2 * texture_normalmap.Sample(sampler_objectshader, UV_normalMap + material.texMulAdd.zw).rg - 1;'
normal=(root/'EngineBridge/shaders/RiverFlowNormalBlock.hlsli').read_text().replace('0x52464c33','0x52464c35')
normal=normal.replace('clock = clock >= 0 ? clock : GetTime() + flowSettings.z;','clock = asfloat(material.userdata.w);')
assert base.count(native)==1
base=base.replace(native,normal)
marker='#ifdef WATER\n\t//NORMALMAP';assert base.count(marker)==1
base=base.replace(marker,'#ifdef WATER\n    float4 riverFlowDebug=0;\n    uint riverFlowDebugMode=0;\n\t//NORMALMAP')
marker='\tApplyLighting(surface, lighting, color);';assert base.count(marker)==1
foam=(root/'EngineBridge/shaders/RiverJunctionFoamShadingBlock.hlsli').read_text()
base=base.replace(marker,marker+'\n'+foam)
(out/'objectHF.hlsli').write_text(base)
(out/'RiverContactFoamPS.hlsl').write_text('#define OBJECTSHADER_COMPILE_PS\n#define OBJECTSHADER_LAYOUT_COMMON\n#define TILEDFORWARD\n#define WATER\n#define TRANSPARENT\n#include "'+(out/'objectHF.hlsli').as_posix()+'"\n')
(out/'provenance.json').write_text(json.dumps({'nativeSHA256':hashlib.sha256(raw).hexdigest(),'foamBlockSHA256':hashlib.sha256(foam.encode()).hexdigest(),'stagedSHA256':hashlib.sha256(base.encode()).hexdigest(),'scope':'G7C native WATER plus separately lit rough bubble coverage; native lights/shadows; no upstream write'},indent=2))
print('G7C_LIT_FOAM_SHADER_STAGED')
