"""Stage native WATER with original foam artwork; reference-package pixels never consumed."""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]).resolve()
subprocess.run([sys.executable,str(root/'Tools/prepare_river_strands_shading.py'),str(out)],check=True)
p=out/'objectHF.hlsli';s=p.read_text()
a=s.index('            uint4 detailControls=foamParams.Load4(96);')
z=s.index('            // Native rough dielectric material lighting',a)
s=s[:a]+'''            uint4 detailControls=foamParams.Load4(96);
            uint detailIndex=(material.userdata.z&2)==0?detailControls.x:detailControls.y;
            Texture2D<float> detailField=bindless_textures_float[descriptor_index(detailIndex)];
            float tracer=detailField.SampleLevel(sampler_linear_clamp,uv,0);
            uint4 artworkControls=foamParams.Load4(112);
            Texture2D<float4> artworkTexture=bindless_textures[descriptor_index(artworkControls.x)];
            uint artWidth,artHeight;artworkTexture.GetDimensions(artWidth,artHeight);
            float tile=asfloat(artworkControls.y),artPeriod=1.5;
            float2 artVelocity=velocity*asfloat(foamParams.Load(24));
            float artPhase0=frac(time/artPeriod),artPhase1=frac(time/artPeriod+.5);
            float artBlend=.5-.5*cos(artPhase0*6.2831853);
            float2 artUV0=(surface.P.xz-artVelocity*artPhase0*artPeriod)/tile;
            float2 artUV1=(surface.P.xz-artVelocity*artPhase1*artPeriod)/tile;
            float footprintUV=max(max(length(ddx_coarse(artUV0)),length(ddy_coarse(artUV0))),
                max(length(ddx_coarse(artUV1)),length(ddy_coarse(artUV1))));
            // Integrate unresolved fast detail instead of temporal sparkle.
            footprintUV=max(footprintUV,length(artVelocity)*min(GetDeltaTime(),.05)*.5/tile);
            float lod=max(0,log2(max(.0001,footprintUV*artWidth)));
            float art0=artworkTexture.SampleLevel(sampler_linear_mirror,artUV0,lod).r;
            float art1=artworkTexture.SampleLevel(sampler_linear_mirror,artUV1,lod).r;
            float artMask=lerp(art1,art0,artBlend);
            float varianceWeight=sqrt(artBlend*artBlend+(1-artBlend)*(1-artBlend));
            artMask=saturate((artMask-.3844)/max(.7,varianceWeight)+.3844);
            float artCoverage=saturate((artMask-.08)/.75);
            float artPacking=.55+.45*saturate(tracer*2);
            float foam=saturate(artCoverage*artPacking*(1-exp(-density*1.8)));
            float2 dxP=ddx_coarse(surface.P.xz),dyP=ddy_coarse(surface.P.xz);
            float dxH=ddx_coarse(artMask),dyH=ddy_coarse(artMask);
            float determinant=dxP.x*dyP.y-dxP.y*dyP.x;
            float2 foamSlope=abs(determinant)>1e-10?float2(dxH*dyP.y-dyH*dxP.y,dyH*dxP.x-dxH*dyP.x)/determinant*.0015:0;
            foamSlope/=max(1,length(foamSlope)/.25);
''' + s[z:]
s=s.replace('float3(bubbleSlope.x,0,bubbleSlope.y)*.25','-float3(foamSlope.x,0,foamSlope.y)')
s=s.replace('lerp(.97,1.03,cap)','lerp(.92,1.04,artCoverage)')
p.write_text(s)
s=(root/'EngineBridge/shaders/RiverFoamStrandsCS.hlsl').read_text()
a=s.index('float Birth(float2 world,float time)');z=s.index('[numthreads',a)
s=s[:a]+(root/'EngineBridge/shaders/RiverFoamArtworkBirth.hlsli').read_text()+'\n'+s[z:]
s=s.replace('corrected=lerp(corrected,Birth(world,strands.settings.y),weight);','[branch]if(weight>0)corrected=lerp(corrected,Birth(world,strands.settings.y,wet.xy*wet.z*strands.settings.w),weight);')
s=s.replace('emission)*dt*8*strands.settings.w','emission)*dt*128*strands.settings.w')
(out/'RiverFoamArtworkCS.hlsl').write_text(s)
print('G8G_ORIGINAL_FOAM_SHADERS_STAGED')
