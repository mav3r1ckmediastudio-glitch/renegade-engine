"""Isolated G12 waterfall veil PS, preserving the approved river shader binaries.
Source is the existing native G11F river-course shader stage. No Wicked edits.
"""
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[1]
source = root / "BUILD/g6/river-course-stage"
destination = Path(sys.argv[1]).resolve()
assert source.is_dir(), "The previously verified river course shader stage is required"
shutil.copytree(root / "WickedEngine/WickedEngine/shaders", destination, dirs_exist_ok=True)
shutil.copytree(source, destination, dirs_exist_ok=True)
(destination / "RiverWaterfallPS.hlsl").write_text(
    "#define WATER\n#define TRANSPARENT\n" +
    (root / "WickedEngine/WickedEngine/shaders/objectPS.hlsl").read_text()
)
p = destination / "objectHF.hlsli"
s = p.read_text()
needle = "        float falling=coursePath.y;"
assert s.count(needle) == 1
# Break up only steep free-falling spans. The flowing holes follow uvsets.w
# (travel time); ordinary flat river/rapids and the lip/toe remain connected.
breakup = """
        // G12 waterfall-only: coarse coherent ropes with thinner holes between
        // them. This shader is NOT the accepted shared river course PS.
        float fallBlend=smoothstep(.30,.85,falling);
        float veilTime=asfloat(material.userdata.w);
        float2 veilUV=float2(uvsets.z*15.0,uvsets.w*1.2-veilTime*.64);
        float coarse=noise_simplex_2D(float2(
            veilUV.x*.65+.8*sin(veilUV.y*.34),veilUV.y*.27));
        float finer=noise_simplex_2D(float2(veilUV.x*1.37,veilUV.y*.63));
        float continuity=.62+.2*coarse+.16*finer+
            .10*sin(veilUV.x*1.2+sin(veilUV.y*.4));
        clip(continuity-lerp(-1.0,.51,fallBlend));
"""
s = s.replace(needle, needle + breakup, 1)
p.write_text(s)
print("G12_WATERFALL_ISOLATED_BREAKUP_SHADER_STAGE_READY", destination)
