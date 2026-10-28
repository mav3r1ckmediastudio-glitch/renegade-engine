from pathlib import Path
import subprocess,sys,hashlib,json
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1])
subprocess.run([sys.executable,str(root/'Tools/prepare_river_junction_foam_shading.py'),str(out)],check=True)
p=out/'objectHF.hlsli';s=p.read_text();marker='                float2 baseUV = UV_normalMap - material.texMulAdd.zw;';assert s.count(marker)==1
s=s.replace(marker,'''                uint4 rapidControls = flowParams.Load4(48);
                Texture2D<float4> rapidSource = bindless_textures[descriptor_index(rapidControls.z)];
                float rippleSteepness = 1 + .55 * saturate(rapidSource.SampleLevel(sampler_linear_clamp,flowUV,0).w);
'''+marker)
for a,z in [('a0 * w0 + a1 * (1 - w0), mask','(a0 * w0 + a1 * (1 - w0)) * rippleSteepness, mask'),('b0 * v0 + b1 * (1 - v0), mask','(b0 * v0 + b1 * (1 - v0)) * rippleSteepness, mask')]:
 assert s.count(a)==1;s=s.replace(a,z)
p.write_text(s)
(out/'rapids-provenance.json').write_text(json.dumps({'stagedSHA256':hashlib.sha256(s.encode()).hexdigest(),'scope':'opt-in measured-source normal steepness; zero source preserves existing normals'},indent=2))
