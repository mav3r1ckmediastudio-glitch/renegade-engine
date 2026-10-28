"""Deploy the owner-accepted G8H native WATER appearance to shared consumers."""
from pathlib import Path
import subprocess,sys,shutil
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]).resolve()
subprocess.run([sys.executable,str(root/'Tools/prepare_river_refined_foam_shading.py'),str(out)],check=True)
# Threshold is an existing authoring control; zero preserves signed-off appearance.
p=out/'objectHF.hlsli';s=p.read_text()
s=s.replace('float artCoverage=saturate((artMask-.08)/.75);','float artCoverage=saturate((artMask-.08-options.y*.25)/.75);')
p.write_text(s)
shutil.copy2(root/'EngineBridge/assets/river-v2-original-foam/foam_coverage_v1.png',out/'OriginalFoamCoverage.png')
print('SHARED_RIVER_APPEARANCE_STAGED')
