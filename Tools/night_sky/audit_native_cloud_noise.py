"""Report the native GPU-generated 64-cubed shape distribution used for calibration."""
from pathlib import Path
import hashlib,json,statistics,sys
root=Path(sys.argv[1]);raw=root/'native-shape-rgba8-64x64x64.bin'
data=raw.read_bytes()
if len(data)!=64**3*4:raise ValueError("Unexpected native texture extent or format")
values=[]
for i in range(0,len(data),4):
    f=(data[i+1]*.625+data[i+2]*.25+data[i+3]*.125)/255
    values.append((data[i]/255+1-f)/(2-f))
values.sort()
result={'raw_sha256':hashlib.sha256(data).hexdigest(),'voxels':len(values),
    'native_body_quantiles':{str(q):values[int((len(values)-1)*q)] for q in (0,.05,.1,.25,.5,.75,.9,.95,1)},
    'mean':statistics.mean(values)}
(root/'quantiles.json').write_text(json.dumps(result,indent=2))
print(json.dumps(result,indent=2))
