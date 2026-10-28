"""Reuse the accepted G4 classifier algorithm with a read-only junction query adapter."""
from pathlib import Path
import sys,hashlib,json
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]).resolve();out.mkdir(parents=True,exist_ok=True)
src=root/'Tests/RiverWaterlineClassifier.h';text=src.read_text()
needle='inline Mask Build(const renegade::bridge::RiverFlowField& flow,'
assert text.count(needle)==1
text=text.replace('namespace river_g4 {','namespace junction_classification {')
text=text.replace(needle,'template<class Flow> inline Mask Build(const Flow& flow,')
(out/'RiverJunctionClassifier.h').write_text(text)
(out/'classifier-provenance.json').write_text(json.dumps({'source_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'adaptation':'namespace isolation and templated read-only flow query only; classification algorithm unchanged'},indent=2))
