"""Single-surface smooth drop: shared travel phase; native WATER optics retained."""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1])
subprocess.run([sys.executable,str(root/'Tools/prepare_river_graded_shading.py'),str(out)],check=True)
p=out/'objectHF.hlsli';s=p.read_text()
anchor='\t}\n\t[branch]\n\tif (camera.texture_waterriples_index >= 0)'
assert s.count(anchor)==1
normal="""        if(material.userdata.y==0x52464c35){
            ByteAddressBuffer smoothParams=bindless_buffers[descriptor_index(material.userdata.x)];
            float clock=asfloat(material.userdata.w)*asfloat(smoothParams.Load(24));
            // One continuous native 3D travel-time coordinate, including the toe.
            float2 travelUV=float2(uvsets.z*3.2,(uvsets.w-clock)*2);
            float2 dx=ddx_coarse(travelUV),dy=ddy_coarse(travelUV);
            dy.y=max(abs(dy.y),min(GetDeltaTime(),.05));
            bumpColor0=2*texture_normalmap.SampleGrad(sampler_objectshader,travelUV,dx,dy).rg-1;
            bumpColor1=2*texture_normalmap.SampleGrad(sampler_objectshader,travelUV*1.37+float2(.31,.71),dx*1.37,dy*1.37).rg-1;
        }
"""
s=s.replace(anchor,normal+anchor)
# Use the same artwork coordinates through the falling sheet and receiving water.
a='            float2 artUV0=(surface.P.xz-artVelocity*artPhase0*artPeriod)/tile;'
b='            float2 artUV1=(surface.P.xz-artVelocity*artPhase1*artPeriod)/tile;'
assert s.count(a)==1 and s.count(b)==1
s=s.replace(a,'            float2 artUV0=float2(uvsets.z*8,(uvsets.w-time)*1.5);')
s=s.replace(b,'            float2 artUV1=artUV0;')
s=s.replace('float density=state.r*options.x;','''float4 shape=asfloat(foamParams.Load4(128));
        float receive=smoothstep(shape.y-.15,shape.y+.6,surface.P.x);
        float density=state.r*options.x*receive;
        // Source-born entrainment on the steep sheet, blended into transported foam.
        float falling=smoothstep(shape.x,shape.x+.3,surface.P.x)*(1-receive)*saturate(shape.w);
        density+=options.x*falling*.9*(1-exp(-max(0,shape.z-surface.P.y)*.8));''')
# Outside the lower 2D domain retain only the 3D falling-sheet contribution.
s=s.replace('if(any(uv<0)||any(uv>1))density=0;',
 'if(any(uv<0)||any(uv>1))density=options.x*falling*.9*(1-exp(-max(0,shape.z-surface.P.y)*.8));')
s=s.replace('float artBlend=.5-.5*cos(artPhase0*6.2831853);','float artBlend=1;')
s=s.replace('falling*.9*','falling*8*')
s=s.replace('float hierarchy=smoothstep(.16,.62,tracer);','float hierarchy=max(smoothstep(.16,.62,tracer),falling*.8);')
s=s.replace('float clock=asfloat(material.userdata.w)*asfloat(smoothParams.Load(24));','float clock=fmod(asfloat(material.userdata.w)*asfloat(smoothParams.Load(24)),2000);')
s=s.replace('float time=asfloat(material.userdata.w),period=.9;','float time=fmod(asfloat(material.userdata.w)*asfloat(foamParams.Load(24)),2000),period=.9;')
s=s.replace('footprintUV=max(footprintUV,length(artVelocity)*min(GetDeltaTime(),.05)*.5/tile);','footprintUV=max(footprintUV,min(GetDeltaTime(),.05)*.75);')
p.write_text(s)
print('SMOOTH_CONTINUOUS_WATER_SHADING_STAGED')
