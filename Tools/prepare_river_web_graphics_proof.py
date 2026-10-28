"""Label-only derivative of the validated G8E harness for connected-web review."""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
subprocess.run([sys.executable,str(root/'Tools/prepare_river_strands_graphics_proof.py'),str(out)],check=True)
s=(out/'RiverFoamStrandsProof.cpp').read_text()
s=s.replace('Renegade River V2 - Advected Foam Strands','Renegade River V2 - Connected Foam Web')
s=s.replace('ADVECTED FOAM STRANDS / ','CONNECTED FOAM WEB / ')
s=s.replace('ADVECTED STRANDS','CONNECTED WEB')
s=s.replace('Source-born detail follows actual flow; same displaced WATER. B compares broad coverage with transported strands.','CONNECTED WEB REVIEW: short shared links and irregular pockets. B previous broad coverage; V waves; Space pause.')
(out/'RiverFoamWebProof.cpp').write_text(s)
print('G8F_LABELLED_WEB_HARNESS_STAGED')
