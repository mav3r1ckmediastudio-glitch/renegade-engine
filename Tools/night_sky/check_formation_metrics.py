"""Check real cloud-opacity coverage semantics and record temporal convergence."""
from pathlib import Path
from PIL import Image
import json,sys
root=Path(sys.argv[1]);rows=json.loads((root/'opacity-metrics.json').read_text())
failures=[]
for row in rows:
    name=row['name'];mode=int(row['formation']);pitch=float(row['pitch'])
    if mode==0 or pitch<0:continue
    coverage=float(row['coverage']);fraction=row['sky_fraction_alpha05']
    if coverage==0 and fraction>.002:failures.append(f"{name}: clear sky contains cloud opacity ({fraction:.4f})")
    if coverage==1 and fraction<.985:failures.append(f"{name}: maximum coverage incomplete ({fraction:.4f})")
for hour in (0,12,18.2):
    for pitch in (0,35,70):
        sequence=sorted((float(row['coverage']),row['sky_fraction_alpha05'],row['name']) for row in rows
            if '-matrix' in row['name'] and float(row['hour'])==hour and float(row['pitch'])==pitch)
        for before,after in zip(sequence,sequence[1:]):
            if after[1]+.025<before[1]:failures.append(f"non-progressive opacity: {before[2]} -> {after[2]}")
temporal={}
for name in ('motion-warm','motion-settled'):
    p=root/(name+'-opacity.png')
    if p.exists():temporal[name]=list(Image.open(p).getchannel('A').resize((320,180)).get_flattened_data())
if len(temporal)==2:
    differences=sorted(abs(a-b)/255 for a,b in zip(temporal['motion-warm'],temporal['motion-settled']))
    motion={'mean_opacity_delta':sum(differences)/len(differences),
        'p95_opacity_delta':differences[int(len(differences)*.95)]}
else:motion={'status':'not captured'}
result={'coverage_checks_pass':not failures,'failures':failures,'temporal_convergence':motion,
    'note':'Opacity threshold 0.05; positive-elevation sky rays only. Temporal deltas are evidence, not perceptual approval.'}
(root/'validation.json').write_text(json.dumps(result,indent=2))
print(json.dumps(result,indent=2))
sys.exit(0 if not failures else 1)
