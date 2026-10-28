"""Bounded G8H appearance refinement; accepted transport and G8G remain unchanged."""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]).resolve()
subprocess.run([sys.executable,str(root/'Tools/prepare_river_artwork_shading.py'),str(out)],check=True)
p=out/'objectHF.hlsli';s=p.read_text()
a=s.index('            float art0=artworkTexture.SampleLevel')
z=s.index('            float2 dxP=ddx_coarse',a)
s=s[:a]+'''            float art0=artworkTexture.SampleLevel(sampler_linear_mirror,artUV0,lod).r;
            float art1=artworkTexture.SampleLevel(sampler_linear_mirror,artUV1,lod).r;
            float artMask=lerp(art1,art0,artBlend);
            // A rotated non-integer scale supplies smaller interwoven froth.
            // Both phases follow the same actual velocity as the original layer.
            const float2x2 turn=float2x2(.8,-.6,.6,.8);
            float smallScale=2.13;
            float2 smallArtUV0=mul(turn,artUV0)*smallScale+float2(7.31,2.67);
            float2 smallArtUV1=mul(turn,artUV1)*smallScale+float2(7.31,2.67);
            float smallLOD=lod+log2(smallScale);
            float smallMask=lerp(
                artworkTexture.SampleLevel(sampler_linear_mirror,smallArtUV1,smallLOD).r,
                artworkTexture.SampleLevel(sampler_linear_mirror,smallArtUV0,smallLOD).r,artBlend);
            // Source-born tracer is transported by the native-domain velocity,
            // so cluster hierarchy travels out of the actual generating regions.
            float hierarchy=smoothstep(.16,.62,tracer);
            float fineWeight=lerp(.25,.65,hierarchy);
            artMask=lerp(artMask,smallMask,fineWeight);
            float varianceWeight=sqrt((artBlend*artBlend+(1-artBlend)*(1-artBlend))*
                (fineWeight*fineWeight+(1-fineWeight)*(1-fineWeight)));
            artMask=saturate((artMask-.3844)/max(.58,varianceWeight)+.3844);
            float artCoverage=saturate((artMask-.08)/.75);
            float artPacking=.45+.55*hierarchy;
            // Softer density response keeps weak transported foam translucent.
            // This changes optical coverage only; no density/source is modified.
            float foam=saturate(artCoverage*artPacking*(1-exp(-density*1.3)));
''' + s[z:]
p.write_text(s)
p=out/'RiverFoamArtworkCS.hlsl';s=p.read_text()
# Birth only: broad original artwork forms transported clusters, independent of
# the two fine surface scales. Same RK2, limiter, source gate and physical mass.
assert s.count('texelWorld*width/4')==1 and s.count('world/4,lod')==1
s=s.replace('texelWorld*width/4','texelWorld*width/14').replace('world/4,lod','world/14,lod+1.5')
p.write_text(s)
print('G8H_REFINED_FOAM_SHADERS_STAGED')
