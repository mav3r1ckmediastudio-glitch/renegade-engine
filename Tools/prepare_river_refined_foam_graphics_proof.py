"""Labelled G8H derivative of the original-artwork proof; no production adoption."""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
subprocess.run([sys.executable,str(root/'Tools/prepare_river_artwork_graphics_proof.py'),str(out)],check=True)
s=(out/'RiverFoamArtworkProof.cpp').read_text()
s=s.replace('Renegade River V2 - Original Foam Detail','Renegade River V2 - Refined Foam')
s=s.replace('ORIGINAL FOAM DETAIL / ','REFINED FOAM / ').replace('ORIGINAL FROTH','REFINED FROTH')
s=s.replace('B broad coverage / original froth.','B original detail / refined froth.')
(out/'RiverRefinedFoamProof.cpp').write_text(s)
print('G8H_REFINED_FOAM_HARNESS_STAGED')
