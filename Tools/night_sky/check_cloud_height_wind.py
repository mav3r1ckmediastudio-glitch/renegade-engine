"""Regression: vertical noise wind must not advect macro clouds out of their layer."""
from pathlib import Path
from array import array
import json,math,sys
before=Path(sys.argv[1]);reports=[]
def measure(folder):
    meta=json.loads((folder/'volume.json').read_text(encoding='utf-8'))
    values=array('f',(folder/'density-r32f-128x128x128.bin').read_bytes())
    if sys.byteorder!='little':values.byteswap()
    assert len(values)==128**3 and all(math.isfinite(v) and v>=0 for v in values)
    occupied=[i for i,v in enumerate(values) if v>=.005]
    depth=meta['y_metres'][1]-meta['y_metres'][0]
    return dict(meta=meta,occupied=len(occupied),
        support_centroid_above_base=sum((i//128)%128+.5 for i in occupied)*depth/128/max(1,len(occupied)))
old=measure(before/'up-half')
assert old['occupied']==0,'Baseline must reproduce the original empty-volume failure.'
for argument in sys.argv[2:]:
    folder=Path(argument);zero=measure(folder/'up-zero');up=measure(folder/'up-half')
    for value in (zero,up):
        assert value['meta']['gpu_time']==600
        assert value['meta']['coverage']==.8
        assert value['occupied']>128**3*.01,'Macro field vanished after upward noise wind.'
    assert zero['meta']['gpu_wind_offset'][1]==0
    assert up['meta']['gpu_wind_offset'][1]==-4500
    assert zero['meta']['gpu_wind_offset'][::2]==up['meta']['gpu_wind_offset'][::2]
    assert abs(up['occupied']/zero['occupied']-1)<.02,'Noise wind changed macro support excessively.'
    assert abs(up['support_centroid_above_base']-zero['support_centroid_above_base'])<20
    reports.append(dict(folder=str(folder),zero=zero,up=up))
result=dict(baseline_empty=old,corrected=reports,
    scope='Actual GPU shared density/time/wind; no appearance or performance approval.')
Path(sys.argv[2]).parent.joinpath('body-height-wind-check.json').write_bytes(json.dumps(result,indent=2).encode('utf-8'))
print('PASS: 600-second vertical-noise wind preserves macro altitude and populated density on all supplied APIs.')
