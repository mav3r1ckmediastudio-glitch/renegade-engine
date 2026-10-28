"""Pack repeat tiles for Wicked terrain VT, which ignores material texMulAdd.
Inputs remain untouched. Supply original map directory and a NEW output directory.
"""
from pathlib import Path
from PIL import Image
import argparse, json, hashlib
p = argparse.ArgumentParser()
p.add_argument('source', type=Path)
p.add_argument('output', type=Path)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=False)
manifest = {'repeat_per_axis': 4, 'resolution': 4096, 'maps': {}}
for name in ('rocky_basecolor.png', 'rocky_normal_dx.png', 'rocky_surface.png'):
    original = Image.open(a.source / name)
    # All PBR channels use the same repeat grid. No shader/geometry changes.
    tile = original.resize((1024, 1024), Image.Resampling.LANCZOS)
    packed = Image.new(original.mode, (4096, 4096))
    for y in range(4):
        for x in range(4):
            packed.paste(tile, (x*1024, y*1024))
    target = a.output / name
    packed.save(target)
    manifest['maps'][name] = hashlib.sha256(target.read_bytes()).hexdigest()
(a.output/'packed_tiles_manifest.json').write_text(json.dumps(manifest, indent=2))
print('PACKED_ROCKY_TILES=PASS repeat=4 resolution=4096')
