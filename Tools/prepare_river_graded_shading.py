"""G10A local graded optics, preserving all native WATER shading operations."""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]).resolve()
subprocess.run([sys.executable,str(root/'Tools/prepare_river_refined_foam_shading.py'),str(out)],check=True)
# WATER fog/refraction use the local geometric tangent plane instead of one
# global elevation. Native lighting, reflection texture, normals and optics stay.
p=out/'objectHF.hlsli';s=p.read_text()
old='float4 water_plane = camera.reflection_plane;'
new='''float3 gradedNormal=normalize(input.nor);
            if(dot(gradedNormal,camera.reflection_plane.xyz)<0)gradedNormal=-gradedNormal;
            float4 water_plane=float4(gradedNormal,-dot(gradedNormal,surface.P));'''
assert s.count(old)==2;s=s.replace(old,new);p.write_text(s)
p=out/'SurfaceObjectHF.hlsli';s=p.read_text()
s=s.replace('surface.normal=normalize(float3(-hx,1,-hz))*(surface.normal.y<0?-1:1);','surface.normal=normalize(surface.normal-float3(hx,0,hz)*surface.normal.y);')
s=s.replace('surface.tangent.y=hx*surface.tangent.x+hz*surface.tangent.z;','surface.tangent.y+=hx*surface.tangent.x+hz*surface.tangent.z;')
p.write_text(s)
print('GRADED_LOCAL_WATER_OPTICS_STAGED')
