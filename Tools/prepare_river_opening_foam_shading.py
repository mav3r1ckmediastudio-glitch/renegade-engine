"""G10D: retain the smooth landing, recover accepted downstream foam openings."""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1])
subprocess.run([sys.executable,str(root/'Tools/prepare_river_smooth_cascade_shading.py'),str(out)],check=True)
p=out/'objectHF.hlsli';s=p.read_text()
# Blend optical coverage, never UV coordinates: this avoids a stretched blend band.
helper="""
#ifdef WATER
float3 RiverOpeningArtwork(Texture2D<float4> artwork,float2 uv0,float2 uv1,
    float blend,float lod,float hierarchy,float density)
{
    float mask=lerp(artwork.SampleLevel(sampler_linear_mirror,uv1,lod).r,
        artwork.SampleLevel(sampler_linear_mirror,uv0,lod).r,blend);
    const float2x2 turn=float2x2(.8,-.6,.6,.8);
    float2 fine0=mul(turn,uv0)*2.13+float2(7.31,2.67);
    float2 fine1=mul(turn,uv1)*2.13+float2(7.31,2.67);
    float small=lerp(artwork.SampleLevel(sampler_linear_mirror,fine1,lod+log2(2.13)).r,
        artwork.SampleLevel(sampler_linear_mirror,fine0,lod+log2(2.13)).r,blend);
    float fineWeight=lerp(.25,.65,hierarchy);
    mask=lerp(mask,small,fineWeight);
    float variance=sqrt((blend*blend+(1-blend)*(1-blend))*
        (fineWeight*fineWeight+(1-fineWeight)*(1-fineWeight)));
    mask=saturate((mask-.3844)/max(.58,variance)+.3844);
    float coverage=saturate((mask-.08)/.75);
    float foam=saturate(coverage*(.45+.55*hierarchy)*(1-exp(-density*1.3)));
    return float3(mask,coverage,foam);
}
#endif
"""
# Insert after includes so native sampler declarations are already available.
anchor='// DEFINITIONS'
assert s.count(anchor)==1;s=s.replace(anchor,helper+'\n'+anchor)
a=s.index('            float artBlend=1;')
z=s.index('            float2 dxP=ddx_coarse',a)
s=s[:a]+"""            // This bounded fixture runs +X. Shape.y is the actual rounded toe.
            // Production integration will use the corresponding native arc offset.
            float downstream=max(0,surface.P.x-shape.y);
            float opening=smoothstep(.75,6.0,downstream);
            float3 closeAppearance=0,openAppearance=0;
            float2 closeUV=float2(uvsets.z*8,(uvsets.w-time)*1.5);
            float closeFootprint=max(length(ddx_coarse(closeUV)),length(ddy_coarse(closeUV)));
            closeFootprint=max(closeFootprint,min(GetDeltaTime(),.05)*.75);
            float closeLOD=max(0,log2(max(.0001,closeFootprint*artWidth)));
            float2 openUV0=(surface.P.xz-artVelocity*artPhase0*artPeriod)/tile;
            float2 openUV1=(surface.P.xz-artVelocity*artPhase1*artPeriod)/tile;
            float openFootprint=max(max(length(ddx_coarse(openUV0)),length(ddy_coarse(openUV0))),
                max(length(ddx_coarse(openUV1)),length(ddy_coarse(openUV1))));
            openFootprint=max(openFootprint,length(artVelocity)*min(GetDeltaTime(),.05)*.5/tile);
            float openLOD=max(0,log2(max(.0001,openFootprint*artWidth)));
            float hierarchy=smoothstep(.16,.62,tracer);
            [branch] if(opening<1)
                closeAppearance=RiverOpeningArtwork(artworkTexture,closeUV,closeUV,1,closeLOD,max(hierarchy,falling*.8),density);
            [branch] if(opening>0)
                openAppearance=RiverOpeningArtwork(artworkTexture,openUV0,openUV1,
                    .5-.5*cos(artPhase0*6.2831853),openLOD,hierarchy,density);
            float3 appearance=lerp(closeAppearance,openAppearance,opening);
            float artMask=appearance.x,artCoverage=appearance.y,foam=appearance.z;
""" +s[z:]
# The phase period must close for BOTH continuous and world-space sampling.
s=s.replace('float time=fmod(asfloat(material.userdata.w)*asfloat(foamParams.Load(24)),2000),period=.9;',
 'float time=fmod(asfloat(material.userdata.w)*asfloat(foamParams.Load(24)),6000),period=.9;')
p.write_text(s)
print('OPENING_FOAM_SHADER_STAGED')
