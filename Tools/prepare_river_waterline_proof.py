"""Stage the G4 WATER-only classification diagnostic. No native source writes."""
from pathlib import Path
import sys,hashlib,json
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]).resolve();out.mkdir(parents=True,exist_ok=True)
raw=(root/'WickedEngine/WickedEngine/shaders/objectHF.hlsli').read_bytes()
assert hashlib.sha256(raw).hexdigest()=='27d73178076aa7333374bef9a162e996629db01e749556881236d978daf41ce5'
base=raw.decode().replace('\r\n','\n');marker='\tApplyLighting(surface, lighting, color);';assert base.count(marker)==1
block=(root/'EngineBridge/shaders/RiverWaterlineDebugBlock.hlsli').read_text()
stage=base.replace(marker,marker+'\n'+block)
(out/'objectHF.hlsli').write_text(stage)
(out/'RiverWaterlinePS.hlsl').write_text('#define OBJECTSHADER_COMPILE_PS\n#define OBJECTSHADER_LAYOUT_COMMON\n#define TILEDFORWARD\n#define WATER\n#define TRANSPARENT\n#include "'+(out/'objectHF.hlsli').as_posix()+'"\n')
(out/'provenance.json').write_text(json.dumps({'nativeSHA256':hashlib.sha256(raw).hexdigest(),'blockSHA256':hashlib.sha256(block.encode()).hexdigest(),'scope':'WATER-only explicit category display after lighting; all native normal sampling untouched'},indent=2))
print('G4_SHADER_STAGED')
