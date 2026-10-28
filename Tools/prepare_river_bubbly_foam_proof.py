"""Stage isolated G8C bubbling appearance; accepted shader blocks are immutable inputs."""
from pathlib import Path
import subprocess,sys,hashlib,json
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]).resolve()
subprocess.run([sys.executable,str(root/'Tools/prepare_river_rapids_shading.py'),str(out)],check=True)
p=out/'objectHF.hlsli';s=p.read_text()
a=(root/'EngineBridge/shaders/RiverJunctionFoamShadingBlock.hlsli').read_text()
z=a
start=z.index('            float scale=lerp(55,32,saturate(age/5));')
end=z.index('            float mass=1-exp(-density*2);')
z=z[:start]+'''            uint4 flowControl=foamParams.Load4(16);
            Texture2D<float4> baseFlow=bindless_textures[descriptor_index(flowControl.x)];
            float4 base=baseFlow.SampleLevel(sampler_linear_clamp,uv,0);
            float2 velocity=base.rg*base.b;
            float time=asfloat(material.userdata.w),period=.9;
            float phase=frac(time/period),phase2=frac(time/period+.5);
            // C1 phase blend removes slope cusps while retaining accepted travel.
            float blend=.5-.5*cos(phase*6.2831853);
            float2 detail0=surface.P.xz-velocity*phase*period;
            float2 detail1=surface.P.xz-velocity*phase2*period;
            float footprint=max(length(ddx_coarse(surface.P.xz)),length(ddy_coarse(surface.P.xz)));
            footprint=max(footprint,max(length(ddx_coarse(detail0)),length(ddy_coarse(detail0))));
            footprint=max(footprint,max(length(ddx_coarse(detail1)),length(ddy_coarse(detail1))));
            float midLOD=1-smoothstep(.2,.65,footprint*19.7);
            float fineLOD=1-smoothstep(.2,.65,footprint*43.1);
            float clump0=noise_simplex_2D(detail0*7.3)*.29+
                noise_simplex_2D(detail0*19.7)*.16*midLOD+noise_simplex_2D(detail0*43.1)*.08*fineLOD;
            float clump1=noise_simplex_2D(detail1*7.3)*.29+
                noise_simplex_2D(detail1*19.7)*.16*midLOD+noise_simplex_2D(detail1*43.1)*.08*fineLOD;
            float clusters=.5+lerp(clump1,clump0,blend)+noise_simplex_2D(coordinate*3.1)*.12;
            float4 bubbles0=RiverBubbleCell(detail0*18,time);
            float4 bubbles1=RiverBubbleCell(detail1*18,time);
            float4 small0=RiverBubbleCell(detail0*43+17.3,time);
            float4 small1=RiverBubbleCell(detail1*43+17.3,time);
            float mediumResolved=1-smoothstep(.3,1.15,footprint*18);
            float smallResolved=1-smoothstep(.3,1.15,footprint*43);
            float cap=lerp(.45,lerp(bubbles1.z,bubbles0.z,blend),mediumResolved);
            float2 bubbleSlope=lerp(bubbles1.xy,bubbles0.xy,blend)*mediumResolved;
            bubbleSlope+=lerp(small1.xy,small0.xy,blend)*smallResolved*.3;
            float bubbles=.9+.1*cap;
''' +z[end:]
z=z.replace('float coverage=smoothstep(.78-.40*mass+options.y-.06,.78-.40*mass+options.y+.06,clusters);','float edgeWidth=max(.06,fwidth(clusters)*.7);\n            float coverage=smoothstep(.78-.40*mass+options.y-edgeWidth,.78-.40*mass+options.y+edgeWidth,clusters);')
z=z.replace('coverage*(.36+.64*bubbles)','coverage*bubbles')
z=z.replace('bubbleSurface.albedo=float3(.62,.645,.66);','bubbleSurface.albedo=float3(.62,.645,.66)*lerp(.8,1.08,cap);')
z=z.replace('bubbleSurface.N=normalize(lerp(surface.N,float3(0,1,0),.85));','float3 foamBase=normalize(lerp(surface.N,float3(0,1,0),.75));\n            bubbleSurface.N=normalize(foamBase+float3(bubbleSlope.x,0,bubbleSlope.y));')
assert s.count(a)==1
s=s.replace(a,z)
helper=(root/'EngineBridge/shaders/RiverBubblyFoamFunctions.hlsli').read_text()
s=helper+'\n'+s;p.write_text(s)
(out/'bubbly-provenance.json').write_text(json.dumps({'nativeShaderSHA256':hashlib.sha256((root/'WickedEngine/WickedEngine/shaders/objectHF.hlsli').read_bytes()).hexdigest(),'acceptedFoamBlockSHA256':hashlib.sha256(a.encode()).hexdigest(),'stagedSHA256':hashlib.sha256(s.encode()).hexdigest(),'scope':'G8C shader appearance only; density velocity sources geometry and accepted shaders unchanged'},indent=2))
print('G8C_BUBBLY_FOAM_STAGED')
