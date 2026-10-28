"""General course WATER: native arc lookup replaces fixed-X proof assumptions."""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]).resolve()
subprocess.run([sys.executable,str(root/'Tools/prepare_river_opening_foam_shading.py'),str(out)],check=True)
p=out/'objectHF.hlsli';s=p.read_text()
lookup="""
#ifdef WATER
float4 RiverCoursePath(ByteAddressBuffer parameters,float travel)
{
    uint4 controls=parameters.Load4(128);
    Texture2D<float4> course=bindless_textures[descriptor_index(controls.x)];
    return course.SampleLevel(sampler_linear_clamp,float2(.5,saturate(travel/max(.001,asfloat(controls.y)))),0);
}
#endif
"""
s=s.replace('// DEFINITIONS',lookup+'\n// DEFINITIONS',1)
a=s.index('        if(material.userdata.y==0x52464c35){\n            ByteAddressBuffer smoothParams')
z=s.index('\t}\n\t[branch]\n\tif (camera.texture_waterriples_index',a)
normal=s[a:z]
normal=normal.replace('float clock=', 'if(smoothParams.Load(140)!=0){\n            float4 pathSample=RiverCoursePath(smoothParams,uvsets.w);\n            float clock=',1)
normal=normal.replace('uvsets.z*3.2','uvsets.z*pathSample.z*.8')
last=normal.rfind('        }');normal=normal[:last]+'            }\n'+normal[last:]
s=s[:a]+normal+s[z:]
a=s.index('        float4 shape=asfloat(foamParams.Load4(128));')
z=s.index('        if(options.z>0)',a)
s=s[:a]+"""        float4 coursePath=RiverCoursePath(foamParams,uvsets.w);
        float density=state.r*options.x;
        if(any(uv<0)||any(uv>1))density=0;
        float falling=coursePath.y;
        density+=options.x*falling*8;
""" +s[z:]
s=s.replace('float downstream=max(0,surface.P.x-shape.y);','float downstream=max(0,coursePath.x);')
s=s.replace('uvsets.z*8,(uvsets.w-time)*1.5','uvsets.z*coursePath.z*2,(uvsets.w-time)*1.5')
s=s.replace('// This bounded fixture runs +X. Shape.y is the actual rounded toe.\n            // Production integration will use the corresponding native arc offset.','// Native accumulated arc offset follows the latest rounded toe through bends.')
s=s.replace('float coverage=saturate((mask-.08)/.75);','float coverage=saturate((mask-.08)/.75);')
# The existing threshold control must remain functional.
s=s.replace('float artMask=appearance.x,artCoverage=appearance.y,foam=appearance.z;',
 'float artMask=appearance.x,artCoverage=appearance.y,foam=appearance.z;foam*=saturate(1-options.y);')
# Existing turbulence affects transported normal perturbation without resetting bulk history.
s=s.replace('bumpColor0=2*texture_normalmap.SampleGrad(sampler_objectshader,travelUV,dx,dy).rg-1;',
 'bumpColor0=(2*texture_normalmap.SampleGrad(sampler_objectshader,travelUV,dx,dy).rg-1)*sqrt(max(0,asfloat(smoothParams.Load(136))));')
s=s.replace('bumpColor1=2*texture_normalmap.SampleGrad(sampler_objectshader,travelUV*1.37+float2(.31,.71),dx*1.37,dy*1.37).rg-1;',
 'bumpColor1=(2*texture_normalmap.SampleGrad(sampler_objectshader,travelUV*1.37+float2(.31,.71),dx*1.37,dy*1.37).rg-1)*sqrt(max(0,asfloat(smoothParams.Load(136))));')
s=s.replace('density+=options.x*falling*8;', 'density+=options.x*falling*8*options.w;')
# Temporal footprint forcing erased water detail beyond close range.
# Texture filtering must use the actual spatial derivatives, not frame duration.
s=s.replace('dy.y=max(abs(dy.y),min(GetDeltaTime(),.05));','')
# Retain the established native/directed normal detail on level reaches.
# Travel-space sampling is needed only on the falling sheet.
s=s.replace('bumpColor0=(2*texture_normalmap.SampleGrad(sampler_objectshader,travelUV,dx,dy).rg-1)*sqrt(max(0,asfloat(smoothParams.Load(136))));',
 'bumpColor0=lerp(bumpColor0,(2*texture_normalmap.SampleGrad(sampler_objectshader,travelUV,dx,dy).rg-1)*sqrt(max(0,asfloat(smoothParams.Load(136)))),saturate(pathSample.y*4));')
s=s.replace('bumpColor1=(2*texture_normalmap.SampleGrad(sampler_objectshader,travelUV*1.37+float2(.31,.71),dx*1.37,dy*1.37).rg-1)*sqrt(max(0,asfloat(smoothParams.Load(136))));',
 'bumpColor1=lerp(bumpColor1,(2*texture_normalmap.SampleGrad(sampler_objectshader,travelUV*1.37+float2(.31,.71),dx*1.37,dy*1.37).rg-1)*sqrt(max(0,asfloat(smoothParams.Load(136)))),saturate(pathSample.y*4));')
# Derive world-to-texture advection from the rendered UV Jacobian.
# Derived course tangents need not share the native ribbon tangent convention.
s=s.replace('float2 uvVelocity = float2(-dot(worldVelocity, normalize(TBN[1].xz)),\n                    dot(worldVelocity, normalize(TBN[0].xz))) / tileMetres;',
'''float2 wx=ddx_coarse(surface.P.xz),wy=ddy_coarse(surface.P.xz);
                float determinant=wx.x*wy.y-wx.y*wy.x;
                float2 uvVelocity=0;
                if(abs(determinant)>1e-12){
                    float a=(worldVelocity.x*wy.y-worldVelocity.y*wy.x)/determinant;
                    float b=(wx.x*worldVelocity.y-wx.y*worldVelocity.x)/determinant;
                    uvVelocity=a*ddx_coarse(UV_normalMap)+b*ddy_coarse(UV_normalMap);
                }''')
# The hydraulic domain ends inside the concealed terrain cap. Continue the
# SAME directed normal field through its fringe and the buried mesh extension,
# instead of switching to the native scrolling normal samples there.
s=s.replace('            riverFlowDebug = float4(direction * asfloat(controlBits.z), speed, mask);',
'''            float2 travelDx=ddx_coarse(uvsets.zw),travelDy=ddy_coarse(uvsets.zw);
            float travelDet=travelDx.x*travelDy.y-travelDx.y*travelDy.x;
            float2 continuation=0;
            if(abs(travelDet)>1e-12)
                continuation=(-travelDy.x*ddx_coarse(surface.P.xz)+travelDx.x*ddy_coarse(surface.P.xz))/travelDet;
            worldVelocity=lerp(continuation*asfloat(controlBits.z),worldVelocity,mask);
            mask=flowSettings.w;
            riverFlowDebug = float4(direction * asfloat(controlBits.z), speed, mask);''')
p.write_text(s)
print('GENERAL_NATIVE_COURSE_SHADERS_STAGED')
