"""G10D appearance-only A/B on the exact G10C native geometry and flow."""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1])
subprocess.run([sys.executable,str(root/'Tools/prepare_river_smooth_cascade_graphics_proof.py'),str(out)],check=True)
s=(out/'RiverSmoothCascadeProof.cpp').read_text()
s=s.replace('Renegade River V2 - Smooth Cascade','Renegade River V2 - Cascade Foam Recovery')
s=s.replace('GLOBAL DEPTH DIAGNOSTIC','PREVIOUS DENSE FOAM').replace('LOCAL GRADED DEPTH','DOWNSTREAM OPENING')
s=s.replace('B global/local depth diagnostic','B previous/new foam')
(out/'RiverOpeningFoamProof.cpp').write_text(s)
print('OPENING_FOAM_PROOF_STAGED')
