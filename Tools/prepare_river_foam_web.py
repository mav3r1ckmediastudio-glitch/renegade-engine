"""Optional connected foam-web staging. Source occupancy changes; bulk WATER immutable."""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]).resolve()
subprocess.run([sys.executable,str(root/'Tools/prepare_river_strands_shading.py'),str(out)],check=True)
p=out/'objectHF.hlsli';s=p.read_text()
a='float ribbons=smoothstep(.30,.72,tracer)*smoothstep(.12,.34,bankDistance);'
assert s.count(a)==1
s=s.replace(a,'float threshold=.24+.02*(.5-cap);\n            float ribbons=smoothstep(threshold,.68,tracer)*smoothstep(.12,.34,bankDistance);')
p.write_text(s)
s=(root/'EngineBridge/shaders/RiverFoamStrandsCS.hlsl').read_text()
a=s.index('float Birth(float2 world,float time)');z=s.index('[numthreads',a)
s=s[:a]+(root/'EngineBridge/shaders/RiverFoamWebBirth.hlsli').read_text()+'\n'+s[z:]
s=s.replace('corrected=lerp(corrected,Birth(world,strands.settings.y),weight);','[branch]if(weight>0)corrected=lerp(corrected,Birth(world,strands.settings.y,wet.xy*wet.z*strands.settings.w),weight);')
s=s.replace('emission)*dt*8*strands.settings.w','emission)*dt*128*strands.settings.w')
(out/'RiverFoamWebCS.hlsl').write_text(s)
print('G8F_CONNECTED_FOAM_WEB_STAGED')
