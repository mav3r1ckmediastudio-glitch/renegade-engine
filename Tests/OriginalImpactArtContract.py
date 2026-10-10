"""Source contract: independently supplied Renegade impact art, no Unity runtime dependency."""
from pathlib import Path
import hashlib
import re
import struct
import sys

root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]
folder = root / "Runtime/Content/Effects/Impacts/Original"
rc = (root / "Runtime/RuntimeImpactTextures.rc.in").read_text(encoding="utf-8")
loader = (root / "Runtime/src/RuntimeImpactDustResource.cpp").read_text(encoding="utf-8")
visuals = (root / "Runtime/src/RuntimeProjectileVisuals.h").read_text(encoding="utf-8")
geometry = (root / "Runtime/src/RuntimeImpactGeometry.h").read_text(encoding="utf-8")
blood = (root / "Runtime/src/RuntimeBloodSheets.h").read_text(encoding="utf-8")
documentation = (folder / "README.md").read_text(encoding="utf-8")
entries = [
    (7310, "BloodImpact8x8.png", 4096, "Blood8x8"),
    (7311, "ConcreteMarks2x2.png", 1254, "ConcreteMarks"),
    (7312, "DirtMarks2x2.png", 2048, "DirtMarks"),
    (7313, "GlassDebris2x2.png", 2048, "GlassDebris"),
    (7314, "GlassMarks2x2.png", 2048, "GlassMarks"),
    (7315, "MetalMarks2x2.png", 2048, "MetalMarks"),
    (7316, "SkinEntryMarks2x2.png", 2048, "SkinEntryMarks"),
    (7317, "RockDebris2x2.png", 1254, "RockDebris"),
    (7318, "SkinMarks2x2.png", 2048, "SkinMarks"),
    (7319, "StoneMarks2x2.png", 2048, "StoneMarks"),
    (7320, "WoodDebris2x2.png", 1254, "WoodDebris"),
    (7321, "WoodMarks2x2.png", 2048, "WoodMarks"),
]
assert len(list(folder.glob("*.png"))) == 12
for ident, name, side, token in entries:
    data = (folder / name).read_bytes()
    assert data.startswith(bytes.fromhex("89504e470d0a1a0a")), name
    width, height, bit_depth, color_type = struct.unpack(">IIBB", data[16:26])
    assert (width, height, bit_depth, color_type) == (side, side, 8, 6), name
    assert side % (8 if ident == 7310 else 2) == 0, name
    assert f'{ident} RCDATA "@CMAKE_SOURCE_DIR@/Runtime/Content/Effects/Impacts/Original/{name}"' in rc, name
    assert f"renegade_original_{name}" in loader, name
    assert hashlib.sha256(data).hexdigest() in documentation, name
    assert ("BuiltinImpactAtlas::" + token) in (visuals + geometry + blood), name
assert "emitter->framesX=8;emitter->framesY=8;emitter->frameCount=64;" in visuals
assert "scene.PutWaterRipple(point)" in geometry
print("Original impact art source contract: PASS (12 RGBA assets, layouts, hashes, runtime routes, water crown unchanged)")
