"""Generate public synthetic glTF fixtures for native importer interaction checks.
Usage: python Tests/GenerateImporterUiFixture.py BUILD/renegade/ui-input-fixtures
No downloaded or private content is used. This is a test mannequin, not an art asset.
"""
import base64
import json
import math
from pathlib import Path
import struct
import sys

out = Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
bones = [
    ('Hips', -1, (0, .95, 0)), ('Spine', 0, (0, .15, 0)),
    ('Spine1', 1, (0, .15, 0)), ('Spine2', 2, (0, .15, 0)),
    ('Neck', 3, (0, .15, 0)), ('Head', 4, (0, .15, 0)),
    ('LeftShoulder', 3, (.12, .02, 0)), ('LeftArm', 6, (.12, 0, 0)),
    ('LeftForeArm', 7, (.25, 0, 0)), ('LeftHand', 8, (.23, 0, 0)),
    ('RightShoulder', 3, (-.12, .02, 0)), ('RightArm', 10, (-.12, 0, 0)),
    ('RightForeArm', 11, (-.25, 0, 0)), ('RightHand', 12, (-.23, 0, 0)),
    ('LeftUpLeg', 0, (.12, -.08, 0)), ('LeftLeg', 14, (0, -.4, 0)),
    ('LeftFoot', 15, (0, -.4, 0)), ('LeftToeBase', 16, (0, 0, .12)),
    ('RightUpLeg', 0, (-.12, -.08, 0)), ('RightLeg', 18, (0, -.4, 0)),
    ('RightFoot', 19, (0, -.4, 0)), ('RightToeBase', 20, (0, 0, .12)),
]
nodes = [{'name': 'Synthetic Humanoid', 'children': [1, len(bones) + 1]}]
world = []
for i, (name, parent, pos) in enumerate(bones):
    origin = world[parent] if parent >= 0 else (0, 0, 0)
    world.append(tuple(a+b for a, b in zip(origin, pos)))
    nodes.append({'name': 'mixamorig:' + name, 'translation': pos})
    if parent >= 0:
        nodes[parent+1].setdefault('children', []).append(i+1)
nodes.append({'name': 'Synthetic two-material body', 'mesh': 0, 'skin': 0})
data = bytearray()
views, accessors = [], []
def accessor(values, components, kind, component_type=5126, bounds=False):
    while len(data) % 4:
        data.append(0)
    offset = len(data)
    fmt = 'f' if component_type == 5126 else 'H'
    data.extend(struct.pack('<' + fmt * len(values), *values))
    views.append({'buffer': 0, 'byteOffset': offset, 'byteLength': len(data)-offset})
    item = {'bufferView': len(views)-1, 'componentType': component_type,
            'count': len(values)//components, 'type': kind}
    if bounds:
        item['min'] = [min(values[c::components]) for c in range(components)]
        item['max'] = [max(values[c::components]) for c in range(components)]
    accessors.append(item)
    return len(accessors)-1

positions, joints, weights, indices = [], [], [], [[], []]
faces = [(0,2,1),(1,2,3),(4,5,6),(5,7,6),(0,1,4),(1,5,4),
         (2,6,3),(3,6,7),(0,4,2),(2,4,6),(1,3,5),(3,7,5)]
for i, center in enumerate(world):
    half = (.09, .11, .075) if i < 6 else (.06, .065, .06)
    base = len(positions)//3
    for z in (-1,1):
        for y in (-1,1):
            for x in (-1,1):
                positions.extend(center[c] + (x,y,z)[c]*half[c] for c in range(3))
                joints.extend((i,0,0,0)); weights.extend((1,0,0,0))
    indices[i%2].extend(base+k for face in faces for k in face)
attrs = {'POSITION': accessor(positions,3,'VEC3',bounds=True),
         'JOINTS_0': accessor(joints,4,'VEC4',5123),
         'WEIGHTS_0': accessor(weights,4,'VEC4')}
primitives = [{'attributes': attrs, 'indices': accessor(idx,1,'SCALAR',5123),
               'material': i} for i,idx in enumerate(indices)]
inverse = []
for x,y,z in world:
    inverse.extend((1,0,0,0, 0,1,0,0, 0,0,1,0, -x,-y,-z,1))
ibm = accessor(inverse,16,'MAT4')
times = accessor([0,.5,1],1,'SCALAR',bounds=True)
rotations = accessor([0,0,0,1, 0,math.sin(.15),0,math.cos(.15), 0,0,0,1],4,'VEC4')
doc = {'asset': {'version': '2.0', 'generator': 'Renegade synthetic UI fixture'},
       'scene': 0, 'scenes': [{'nodes':[0]}], 'nodes': nodes,
       'skins':[{'name':'Synthetic Mixamo Rig','joints':list(range(1,len(bones)+1)),
                 'skeleton':1,'inverseBindMatrices':ibm}],
       'meshes':[{'primitives':primitives}],
       'materials':[{'name':'Copper','pbrMetallicRoughness':{'baseColorFactor':[.8,.3,.1,1]}},
                    {'name':'Blue','pbrMetallicRoughness':{'baseColorFactor':[.1,.3,.8,1]}}],
       'animations':[{'name':'Synthetic Hip Turn','samplers':[{'input':times,'output':rotations}],
                      'channels':[{'sampler':0,'target':{'node':1,'path':'rotation'}}]}],
       'bufferViews':views,'accessors':accessors,
       'buffers':[{'byteLength':len(data),'uri':'data:application/octet-stream;base64,'+
                   base64.b64encode(data).decode('ascii')}]}
for filename, clipname in [('character.gltf','Embedded Hip Turn'),
                            ('external-a.gltf','External Hip Turn A'),
                            ('external-b.gltf','External Hip Turn B')]:
    doc['animations'][0]['name'] = clipname
    (out/filename).write_text(json.dumps(doc),encoding='utf-8')
print('Generated 3 synthetic fixtures:', out)
