"""Audit and stage a patch to PINNED Wicked's *native* WATER refraction code.
Never alters upstream's objectHF.hlsli or registers a new custom shader.
Usage: python Tests/NativeWaterDepthMathProof.py [original_source] [staged_copy] [patch]
"""
from __future__ import annotations
from pathlib import Path
import difflib
import hashlib
import math
import sys

def dot(a,b):
    return sum(x*y for x,y in zip(a,b))

def sub(a,b):
    return tuple(x-y for x,y in zip(a,b))

def normalize(a):
    m=math.sqrt(dot(a,a))
    assert m > 0.01
    return tuple(x/m for x in a)

def refdepth(center,hit,normal,cam):
    n=normalize(normal)
    # The shader should compare with the fragment's local geometric plane,
    # which is independent of elevation and animated normal-map distortion.
    sign=-1.0 if dot(sub(cam,center),n)<0 else 1.0
    return dot(sub(center,hit),n)*sign

def global_depth(refplane_y,hit):
    return refplane_y-hit[1]

def near(a,b,tag,epsilon=1e-5):
    if abs(a-b)>epsilon:
        raise AssertionError(f"{tag}: {a} vs {b}")
    print("PASS",tag,"value",round(a,6))

# Important: the scene's actual bed depth remains 0.30 m; world elevation
# must NOT affect water absorption on a mountain stream.
camera=(12,35,11)
for y in (0.,8.,24.,40.,80.):
    cam=(12,y+5,11)
    p=(11.,y,11.)
    hit=(11.,y-.3,11.)
    near(refdepth(p,hit,(0.,1.,0.),cam),.3,
         f"constant_30cm_depth_elevation_{int(y)}")
near(refdepth((0,8,0),(0,7.7,0),(0,1,0),(0,12,0)),
     global_depth(8.,(0,7.7,0)),"flat_Wicked_refplane_parity")
slope_normal=normalize((0,1,.5))
p=(0,24,6)
bed=tuple(p[i]-slope_normal[i]*.3 for i in range(3))
cam=tuple(p[i]+slope_normal[i]*2 for i in range(3))
near(refdepth(p,bed,slope_normal,cam),.3,"tilted_slope_normal_thickness")
near(refdepth(p,bed,tuple(-x for x in slope_normal),cam),.3,
     "slope_geometry_winding_safe")
# A fixed reflection plane 24 m below water would previously turn 0.3 m
# into 24.3 m of optical path length.
print("GLOBAL_PLANE_SLOPED_FAILURE_m=",round(global_depth(0,bed),4))
assert abs(global_depth(0,bed))>20
# Deliberately retained below-water support: signed local distance positive
# for an above-water hit seen through water from the underside.
near(refdepth((0,8,0),(0,8.4,0),(0,1,0),(0,7.2,0)),
     .4,"below_water_view_correct_local_sign")
# Native reflection-depth attenuation should also be relative to each local
# water fragment, not to an unrelated elevation plane.
near(dot(sub((0,8,0),(0,7.7,0)),(0.,1.,0.)),.3,
     "flat_reflection_depth_original_plane_parity")
near(dot(sub(p,bed),slope_normal),.3,"sloping_reflection_local_depth")
print("WATER_DEPTH_GEOMETRY_MATH=PASS")

if len(sys.argv)!=4:
    print("MATH_ONLY; supply original, isolated patched file and diff to stage source patch")
    sys.exit(0)

original_path=Path(sys.argv[1])
dest_path=Path(sys.argv[2])
diff_path=Path(sys.argv[3])
source=original_path.read_text(encoding="utf-8")
# Use exact pin-independent structural sentinels. Fail instead of
# accidentally editing general-purpose PBR code or the normal-map block.
first=source.index("\t\t// Water refraction:")
last=source.index("\n\t\tcolor.a = 1;",first)+len("\n\t\tcolor.a = 1;")
water=source[first:last]
assert source.count("\t\t// Water refraction:")==1

replacements=(
("\t\tfloat4 water_plane = camera.reflection_plane;\n"
 "\t\tconst float camera_above_water = dot(float4(camera.position, 1), water_plane) < 0;",
 "\t\t// The scene-depth image is sampled BEHIND this exact water fragment.\n"
 "\t\t// Global planar-reflection height is not the riverbed depth on a slope.\n"
 "\t\t// TBN[2] is the geometry normal, not the animated surface ripple.\n"
 "\t\tconst float3 local_water_normal = normalize(TBN[2]);\n"
 "\t\tconst bool camera_below_local_water = dot(camera.position - surface.P, local_water_normal) < 0;\n"
 "\t\tconst float water_depth_side = camera_below_local_water ? -1.0f : 1.0f;"),
("\t\tfloat water_depth = -dot(float4(refraction_position, 1), water_plane);\n"
 "\t\tif(camera_above_water)\n"
 "\t\t\twater_depth = -water_depth;",
 "\t\tfloat water_depth = dot(surface.P - refraction_position, local_water_normal) * water_depth_side;"),
("\t\twater_depth = max(water_depth, -dot(float4(refraction_position, 1), water_plane));\n"
 "\t\tif(camera_above_water)\n"
 "\t\t\twater_depth = -water_depth;",
 "\t\twater_depth = max(water_depth, dot(surface.P - refraction_position, local_water_normal) * water_depth_side);"),
)
for a,b in replacements:
    if water.count(a)!=1:
        raise RuntimeError("native shader structure drift or duplicate marker: "+repr(a[:65]))
    water=water.replace(a,b)
patched=source[:first]+water+source[last:]
assert "#ifdef WATER\n\t//NORMALMAP" in patched
# Byte-identical native water normal/reflection portions, only change
# refracted scene-depth part.
assert source[:first]==patched[:first]
assert source[last:]==patched[len(source[:first])+len(water):]
assert "float4 water_plane = camera.reflection_plane;" not in water
assert "camera.texture_reflection_index" in patched
# Keep the original native reflection-capture pipeline; correct only the
# attenuation depth so it is local to the actual WATER surface fragment.
reflection_old=(
    "\t\t\tfloat4 water_plane = camera.reflection_plane;\n"
    "\t\t\tfloat water_depth = -dot(float4(reflectivePosition, 1), water_plane);"
)
reflection_new=(
    "\t\t\tconst float3 local_reflection_normal = normalize(TBN[2]);\n"
    "\t\t\tfloat water_depth = dot(surface.P - reflectivePosition, local_reflection_normal);"
)
if patched.count(reflection_old)!=1:
    raise RuntimeError("Pinned native WATER reflection-depth marker drift")
patched=patched.replace(reflection_old,reflection_new)
dest_path.parent.mkdir(parents=True,exist_ok=True)
diff_path.parent.mkdir(parents=True,exist_ok=True)
dest_path.write_text(patched,encoding="utf-8",newline="")
diff="".join(difflib.unified_diff(source.splitlines(True),patched.splitlines(True),
       fromfile="a/WickedEngine/shaders/objectHF.hlsli",
       tofile="b/WickedEngine/shaders/objectHF.hlsli",n=5))
diff_path.write_text(diff,encoding="utf-8",newline="")
print("NATIVE_WICKED_SOURCE_PATCH_STAGED=PASS")
print("ORIGINAL_NATIVE_SHADER_SHA256="+hashlib.sha256(source.encode()).hexdigest())
print("PATCHED_NATIVE_SHADER_SHA256="+hashlib.sha256(patched.encode()).hexdigest())
print("ONLY_NATIVE_WATER_REFRACTION_AND_REFLECTION_DEPTH_CHANGED=PASS")
