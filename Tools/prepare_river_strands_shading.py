"""Optional advected strand appearance. Accepted bulk foam and native WATER unchanged."""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]).resolve()
subprocess.run([sys.executable,str(root/'Tools/prepare_river_surface_motion_proof.py'),str(out)],check=True)
p=out/'objectHF.hlsli';s=p.read_text()
a='            float foam=saturate(coverage*mass*packing*(.97+.03*cap));'
assert s.count(a)==1
s=s.replace(a,'''            uint4 detailControls=foamParams.Load4(96);
            uint detailIndex=(material.userdata.z&2)==0?detailControls.x:detailControls.y;
            Texture2D<float> detailField=bindless_textures_float[descriptor_index(detailIndex)];
            float tracer=detailField.SampleLevel(sampler_linear_clamp,uv,0);
            uint4 motionControls=foamParams.Load4(80);
            Texture2D<float4> motionField=bindless_textures[descriptor_index(motionControls.x)];
            float bankDistance=motionField.SampleLevel(sampler_linear_clamp,uv,0).z;
            // Direct source-born concentration, never an isocontour outlining the bank.
            float ribbons=smoothstep(.30,.72,tracer)*smoothstep(.12,.34,bankDistance);
            float youngDense=smoothstep(.6,1.5,density)*(1-smoothstep(.4,1.4,age))*.65;
            float foam=saturate(max(ribbons,youngDense)*saturate(density*3)*(.96+.04*cap));
''')
p.write_text(s)
print('G8E_STRAND_SHADING_STAGED')
