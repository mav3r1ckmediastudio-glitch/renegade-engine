"""Stage a normal-sampling-only extension of pinned native WATER. No upstream writes."""
from pathlib import Path
import hashlib,sys,json
root=Path(__file__).resolve().parents[1]
out=Path(sys.argv[1]).resolve()
out.mkdir(parents=True,exist_ok=True)
source=root/'WickedEngine/WickedEngine/shaders/objectHF.hlsli'
raw=source.read_bytes()
assert hashlib.sha256(raw).hexdigest()=='27d73178076aa7333374bef9a162e996629db01e749556881236d978daf41ce5'
base=raw.decode().replace('\r\n','\n')
native='\t\tbumpColor0 = 2 * texture_normalmap.Sample(sampler_objectshader, UV_normalMap - material.texMulAdd.ww).rg - 1;\n\t\tbumpColor1 = 2 * texture_normalmap.Sample(sampler_objectshader, UV_normalMap + material.texMulAdd.zw).rg - 1;'
assert base.count(native)==1
block=(root/'EngineBridge/shaders/RiverFlowNormalBlock.hlsli').read_text()
stage=base.replace(native,block)
marker='#ifdef WATER\n\t//NORMALMAP'
assert stage.count(marker)==1
stage=stage.replace(marker,'#ifdef WATER\n    float4 riverFlowDebug = 0;\n    uint riverFlowDebugMode = 0;\n\t//NORMALMAP')
lighting='\tApplyLighting(surface, lighting, color);'
assert stage.count(lighting)==1
debug='\n#ifdef WATER\n    if (riverFlowDebugMode == 1) color.rgb = riverFlowDebug.a > 0 ? float3(riverFlowDebug.x * 0.5 + 0.5, riverFlowDebug.y * 0.5 + 0.5, saturate(riverFlowDebug.z / 2)) : float3(0,0,0);\n#endif\n'
stage=stage.replace(lighting,lighting+debug)
(out/'objectHF.hlsli').write_text(stage)
wrapper='#define OBJECTSHADER_COMPILE_PS\n#define OBJECTSHADER_LAYOUT_COMMON\n#define TILEDFORWARD\n#define WATER\n#define TRANSPARENT\n#include "'+(out/'objectHF.hlsli').as_posix()+'"\n'
(out/'RiverFlowNormalPS.hlsl').write_text(wrapper)
(out/'shader-provenance.json').write_text(json.dumps({'nativeSHA256':hashlib.sha256(raw).hexdigest(),'extensionSHA256':hashlib.sha256(block.encode()).hexdigest(),'stagedSHA256':hashlib.sha256(stage.encode()).hexdigest(),'scope':'two WATER normal samples plus explicit velocity debug mode; original optics retained'},indent=2))
print('STAGED_NATIVE_NORMAL_EXTENSION',out)
