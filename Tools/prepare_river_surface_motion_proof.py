"""Stage optional G8D shaders; never modify Wicked or production deployed shaders."""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]).resolve()
subprocess.run([sys.executable,str(root/'Tools/prepare_river_bubbly_foam_proof.py'),str(out)],check=True)
p=out/'objectHF.hlsli';s=p.read_text()
# Dense transported foam forms cohesive bodies; noise modulates tone, not cutout islands.
start=s.index('            float edgeWidth=max(.06,fwidth(clusters)*.7);')
end=s.index('            // Native rough dielectric material lighting',start)
s=s[:start]+'''            // Finite density must retain room between translucent trails and dense froth.
            // Broad flow-aligned modulation changes packing, never creates foam.
            float2 direction=normalize(velocity+float2(.00001,0));
            float2 across=float2(-direction.y,direction.x);
            float2 ribbon0=float2(dot(detail0,across)*1.6,dot(detail0,direction)*.45);
            float2 ribbon1=float2(dot(detail1,across)*1.6,dot(detail1,direction)*.45);
            float packing=.72+.28*saturate(.5+.5*lerp(noise_simplex_2D(ribbon1),noise_simplex_2D(ribbon0),blend));
            float coverage=smoothstep(.10,.70,mass);
            float foam=saturate(coverage*mass*packing*(.97+.03*cap));
'''+s[end:]
s=s.replace('float mass=1-exp(-density*2);','float mass=1-exp(-density*.65*lerp(1,.55,smoothstep(1,4,age)));')
s=s.replace('lerp(surface.N,float3(0,1,0),.75)','lerp(surface.N,normalize(input.nor),.75)').replace('float3(bubbleSlope.x,0,bubbleSlope.y)','float3(bubbleSlope.x,0,bubbleSlope.y)*.25')
s=s.replace('detail0*18','detail0*50').replace('detail1*18','detail1*50').replace('footprint*18','footprint*50')
s=s.replace('detail0*43','detail0*120').replace('detail1*43','detail1*120').replace('footprint*43','footprint*120')
s=s.replace('lerp(.8,1.08,cap)','lerp(.97,1.03,cap)')
p.write_text(s)
helper=(root/'EngineBridge/shaders/RiverSurfaceMotionFunctions.hlsli').read_text()
# Dedicated VS and shadow VS share exactly the same world-space deformation.
native=(root/'WickedEngine/WickedEngine/shaders/objectHF.hlsli').read_text()
anchor='\tsurface.create(material, input);'
assert native.count(anchor)==1
block='''\n    if(material.userdata.y==0x52464c35)
    {
        ByteAddressBuffer motionParams=bindless_buffers[descriptor_index(material.userdata.x)];
        uint4 motion=motionParams.Load4(80);
        if(motion.y!=0)
        {
            float4 bounds=asfloat(motionParams.Load4(0));
            Texture2D<float4> waveField=bindless_textures[descriptor_index(motion.x)];
            float2 uv=(surface.position.xz-bounds.xy)/bounds.zw;
            float time=fmod(asfloat(material.userdata.w),256);
            float2 stepUV=.08/bounds.zw;
            float height=RiverSurfaceHeight(waveField,uv,time);
            float hx=(RiverSurfaceHeight(waveField,uv+float2(stepUV.x,0),time)
                -RiverSurfaceHeight(waveField,uv-float2(stepUV.x,0),time))/.16;
            float hz=(RiverSurfaceHeight(waveField,uv+float2(0,stepUV.y),time)
                -RiverSurfaceHeight(waveField,uv-float2(0,stepUV.y),time))/.16;
            surface.position.y+=height;
            surface.normal=normalize(float3(-hx,1,-hz))*(surface.normal.y<0?-1:1);
            surface.tangent.y=hx*surface.tangent.x+hz*surface.tangent.z;
            surface.tangent.xyz=normalize(surface.tangent.xyz);
        }
    }
'''
# Helpers need globals' Texture2D and sampler declarations already included.
native=native.replace(anchor,anchor+block)
native=native.replace('PixelInput vertex_to_pixel_export(VertexInput input)',helper+'\nPixelInput vertex_to_pixel_export(VertexInput input)')
(out/'SurfaceObjectHF.hlsli').write_text(native)
(out/'RiverSurfaceVS.hlsl').write_text('#define OBJECTSHADER_COMPILE_VS\n#define OBJECTSHADER_LAYOUT_COMMON\n#include "SurfaceObjectHF.hlsli"\n'.replace('SurfaceObjectHF.hlsli',(out/'SurfaceObjectHF.hlsli').as_posix()))
(out/'RiverSurfaceShadowVS.hlsl').write_text('#define OBJECTSHADER_COMPILE_VS\n#define OBJECTSHADER_LAYOUT_SHADOW_TEX\n#define OBJECTSHADER_USE_COLOR\n#define OBJECTSHADER_USE_VIEWPORTARRAYINDEX\n#include "SurfaceObjectHF.hlsli"\n'.replace('SurfaceObjectHF.hlsli',(out/'SurfaceObjectHF.hlsli').as_posix()))
print('G8D_SURFACE_SHADERS_STAGED')
