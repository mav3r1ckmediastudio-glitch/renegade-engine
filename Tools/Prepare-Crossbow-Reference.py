"""Prepare the private crossbow reference using retained native FBX data.

Run with Blender factory startup and --python-exit-code 1. Supply an export
workspace containing exported/ from Unreal's native exporter. Outputs remain
under prepared/. Original geometry, bone spaces and mechanical animation
curves are retained; no third-party assets are bundled with this tool.
"""
import bpy,json,hashlib,sys
from pathlib import Path
args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
if len(args)!=1:raise SystemExit('Usage: blender --background --factory-startup --python-exit-code 1 --python Tools/Prepare-Crossbow-Reference.py -- export-workspace')
root=Path(args[0]).resolve()
out=root/'prepared';out.mkdir(exist_ok=True)
from io_scene_fbx import parse_fbx,encode_bin
from pathlib import Path
from copy import deepcopy
from array import array
import json,numpy as np
out=root/'prepared'
methods={'C':'add_char','Z':'add_bool','B':'add_int8','Y':'add_int16','I':'add_int32','L':'add_int64','F':'add_float32','D':'add_float64','R':'add_bytes','S':'add_string','f':'add_float32_array','d':'add_float64_array','i':'add_int32_array','l':'add_int64_array','b':'add_bool_array','c':'add_byte_array'}
def encoded(e):
 n=encode_bin.FBXElem(e.id)
 for t,v in zip(e.props_type,e.props):
  types={'i':np.int32,'l':np.int64,'f':np.float32,'d':np.float64}
  if chr(t) in types:v=np.asarray(v,dtype=types[chr(t)])
  getattr(n,methods[chr(t)])(v)
 n.elems=[encoded(c) for c in e.elems];return n
def child(e,name):return next(c for c in e.elems if c.id==name)
def name(e):return e.props[1].split(b'\x00')[0]
base,version=parse_fbx.parse(str(root/'exported/Meshes/Crossbow/SK_LightCrossBow.fbx'))
objects=child(base,b'Objects');connections=child(base,b'Connections')
removed={e.props[0] for e in objects.elems if e.id==b'Model' and any(s in name(e) for s in [b'LOD1',b'LOD2'])}
for c in connections.elems:
 if c.id==b'C' and c.props[2] in removed:
  o=next((e for e in objects.elems if e.props and e.props[0]==c.props[1]),None)
  if o and o.id==b'Geometry':removed.add(o.props[0])
while True:
 previous=len(removed)
 for c in connections.elems:
  if c.id==b'C' and c.props[2] in removed:
   o=next((e for e in objects.elems if e.props and e.props[0]==c.props[1]),None)
   if o and o.id==b'Deformer':removed.add(o.props[0])
 if len(removed)==previous:break
objects.elems[:]=[e for e in objects.elems if e.props[0] not in removed and not e.id.startswith(b'Animation')]
connections.elems[:]=[e for e in connections.elems if e.props[1] not in removed and e.props[2] not in removed]
for e in objects.elems:
 if e.id==b'Pose':
  e.elems[:]=[p for p in e.elems if not(p.id==b'PoseNode' and child(p,b'Node').props[0] in removed)]
  for p in e.elems:
   if p.id==b'NbPoseNodes':p.props[0]=sum(c.id==b'PoseNode' for c in e.elems)
assert sum(e.id==b'Geometry' and e.props[2]==b'Mesh' for e in objects.elems)==1
encode_bin.write(str(out/'Weapon_Base.fbx'),encoded(base),version)
models={name(e):e.props[0] for e in objects.elems if e.id==b'Model'}
results=[]
for label,source_label in [('Idle','Fire'),('Fire','Fire'),('Reload','Reload')]:
 source,_=parse_fbx.parse(str(root/('exported/Meshes/Crossbow/Anim_'+source_label+'.fbx')))
 tree=deepcopy(base);obj=child(tree,b'Objects');con=child(tree,b'Connections');source_obj=child(source,b'Objects')
 mapping={e.props[0]:models[name(e)] for e in source_obj.elems if e.id==b'Model' and name(e) in models}
 animation=[deepcopy(e) for e in source_obj.elems if e.id.startswith(b'Animation')]
 next_id=max(e.props[0] for e in obj.elems)+1
 for e in animation:mapping[e.props[0]]=next_id;next_id+=1
 animation_ids={e.props[0] for e in animation}
 for e in animation:e.props[0]=mapping[e.props[0]]
 if label=='Idle':
  ticks=46186158000
  for e in animation:
   if e.id==b'AnimationCurve':
    for p in e.elems:
     if p.id==b'KeyTime':p.props[0]=array('q',[0,ticks])
     elif p.id==b'KeyValueFloat':p.props[0]=array('f',[p.props[0][0],p.props[0][0]])
     elif p.id==b'KeyAttrFlags':p.props[0]=array('i',[p.props[0][0]])
     elif p.id==b'KeyAttrDataFloat':p.props[0]=array('f',p.props[0][:4])
     elif p.id==b'KeyAttrRefCount':p.props[0]=array('i',[2])
   elif e.id==b'AnimationStack':
    for p in child(e,b'Properties70').elems:
     if p.props[0] in [b'LocalStart',b'ReferenceStart']:p.props[4]=0
     elif p.props[0] in [b'LocalStop',b'ReferenceStop']:p.props[4]=ticks
 obj.elems.extend(animation)
 for e in child(source,b'Connections').elems:
  if e.id!=b'C' or not(e.props[1] in animation_ids or e.props[2] in animation_ids):continue
  copied=deepcopy(e)
  for i in [1,2]:
   if copied.props[i]!=0:
    assert copied.props[i] in mapping,(label,'unmapped animation connection',copied.props)
    copied.props[i]=mapping[copied.props[i]]
  con.elems.append(copied)
 for tag in [b'Takes']:
  found=next((e for e in source.elems if e.id==tag),None)
  if found:
   tree.elems[:]=[e for e in tree.elems if e.id!=tag];take=deepcopy(found)
   if label=='Idle':
    for t in take.elems:
     if t.id==b'Take':
      for p in t.elems:
       if p.id in [b'LocalTime',b'ReferenceTime']:p.props[:]=[0,46186158000]
   tree.elems.append(take)
 defs=child(tree,b'Definitions');existing={e.props[0] for e in defs.elems if e.id==b'ObjectType'}
 for e in child(source,b'Definitions').elems:
  if e.id==b'ObjectType' and e.props[0].startswith(b'Animation') and e.props[0] not in existing:defs.elems.append(deepcopy(e))
 for e in defs.elems:
  if e.id==b'Count':e.props[0]=len(obj.elems)
 encode_bin.write(str(out/('Weapon_'+label+'.fbx')),encoded(tree),version)
 results.append({'clip':label,'bones':'original base skin binding','animation':'original Unreal curves' if label!='Idle' else 'constant first Fire pose','meshCount':1})
(out/'weapon-native-graft.json').write_text(json.dumps(results,indent=2));print('NATIVE WEAPON GRAFT',results)

import bpy,json,hashlib
from pathlib import Path
out=root/'prepared';out.mkdir(exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(root/'exported/Meshes/Bolt/SM_Bolt_01.fbx'))
for o in list(bpy.data.objects):
 if o.type=='MESH' and o.name.startswith('UCX_'):bpy.data.objects.remove(o,do_unlink=True)
mesh=next(o for o in bpy.data.objects if o.type=='MESH')
# Static projectile appearance: retain world geometry and UVs, centre its pivot.
# Direction remains authored through the projectile visual rotation setting.
from mathutils import Matrix,Vector
world=[mesh.matrix_world@v.co for v in mesh.data.vertices]
centre=Vector(tuple((min(v[i] for v in world)+max(v[i] for v in world))/2 for i in range(3)))
for vertex,position in zip(mesh.data.vertices,world):vertex.co=position-centre
mesh.parent=None;mesh.matrix_world=Matrix.Identity(4)
mesh.data.update()
m=bpy.data.materials.new('Bolt_PBR');m.use_nodes=True
n=m.node_tree.nodes;l=m.node_tree.links;p=n.get('Principled BSDF')
for suffix,socket in [('BaseColor','Base Color'),('Metallic','Metallic'),('Roughness','Roughness'),('Normal','Normal')]:
 im=bpy.data.images.load(str(root/('exported/Textures/Bolt/T_Bolt_'+suffix+'.tga')))
 if suffix!='BaseColor':im.colorspace_settings.name='Non-Color'
 t=n.new('ShaderNodeTexImage');t.image=im
 if suffix=='Normal':
  sep=n.new('ShaderNodeSeparateColor');comb=n.new('ShaderNodeCombineColor');inv=n.new('ShaderNodeMath');inv.operation='SUBTRACT';inv.inputs[0].default_value=1
  l.new(t.outputs['Color'],sep.inputs['Color']);l.new(sep.outputs['Red'],comb.inputs['Red']);l.new(sep.outputs['Green'],inv.inputs[1]);l.new(inv.outputs[0],comb.inputs['Green']);l.new(sep.outputs['Blue'],comb.inputs['Blue'])
  normal=n.new('ShaderNodeNormalMap');l.new(comb.outputs['Color'],normal.inputs['Color']);l.new(normal.outputs['Normal'],p.inputs['Normal'])
 else:l.new(t.outputs['Color'],p.inputs[socket])
mesh.data.materials.clear();mesh.data.materials.append(m)
# Bake Unreal DirectX normal green-channel conversion into a texture so GLB can retain it.
normal_node=next(t for t in n if t.type=='TEX_IMAGE' and t.image.name.startswith('T_Bolt_Normal'))
im=normal_node.image
import numpy as np
pixels=np.array(im.pixels[:],dtype=np.float32).reshape(-1,4);pixels[:,1]=1-pixels[:,1]
im.pixels.foreach_set(pixels.ravel());im.filepath_raw=str(out/'Bolt_Normal_OpenGL.png');im.file_format='PNG';im.save()
normal_map=next(t for t in n if t.type=='NORMAL_MAP');l.new(normal_node.outputs['Color'],normal_map.inputs['Color'])
for o in bpy.context.selected_objects:o.select_set(False)
mesh.select_set(True);bpy.context.view_layer.objects.active=mesh
bpy.ops.export_scene.gltf(filepath=str(out/'Bolt.glb'),export_format='GLB',use_selection=True,export_animations=False)
bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.gltf(filepath=str(out/'Bolt.glb'))
meshes=[o for o in bpy.data.objects if o.type=='MESH']
assert len(meshes)==1 and not meshes[0].name.startswith('UCX_')
r={'artifact':'prepared/Bolt.glb','sha256':hashlib.sha256((out/'Bolt.glb').read_bytes()).hexdigest(),'meshes':len(meshes),'dimensions':list(meshes[0].dimensions),'uvLayers':len(meshes[0].data.uv_layers),'materialCount':len(meshes[0].data.materials),'normalConvention':'Unreal DirectX green inverted for glTF OpenGL','validation':'Blender cold reopen; native governed import and visual proof pending'}
(out/'bolt-preparation.json').write_text(json.dumps(r,indent=2));print(json.dumps(r,indent=2))

# Keep native Unreal arms animation spaces; only correct material colours.
from io_scene_fbx import parse_fbx,encode_bin
from pathlib import Path
import shutil,json
src,version=parse_fbx.parse(str(root/'exported/Character/Mesh/SK_Mannequin_Arms.fbx'))
methods={'C':'add_char','Y':'add_int16','I':'add_int32','L':'add_int64','F':'add_float32','D':'add_float64','R':'add_bytes','S':'add_string','f':'add_float32_array','d':'add_float64_array','i':'add_int32_array','l':'add_int64_array','b':'add_bool_array','c':'add_byte_array'}
changes=[]
def clone(e):
 n=encode_bin.FBXElem(e.id);props=list(e.props)
 if e.id==b'P' and props:
  if props[0] in [b'DiffuseColor',b'Diffuse'] and len(props)>=7:
   changes.append({'property':props[0].decode(),'before':props[4:7]});props[4:7]=[.65,.65,.65]
  elif props[0]==b'DiffuseFactor' and len(props)>=5:props[4]=1.
 for t,v in zip(e.props_type,props):getattr(n,methods[chr(t)])(v)
 n.elems=[clone(c) for c in e.elems]
 return n
encode_bin.write(str(root/'prepared/Arms_Base.fbx'),clone(src),version)
# Compare the parsed scene payload, excluding the deliberately edited materials.
def structural_digest(e):
 h=hashlib.sha256(e.id+bytes(e.props_type))
 for value in e.props:
  h.update(value.tobytes() if hasattr(value,'tobytes') else repr(value).encode('utf-8'))
 for child in e.elems:h.update(structural_digest(child))
 return h.digest()
def scene_payload(tree):
 values=[]
 for parent in tree.elems:
  if parent.id==b'Objects':values.extend(structural_digest(e) for e in parent.elems if e.id!=b'Material')
  elif parent.id==b'Connections':values.append(structural_digest(parent))
 return values
patched,_=parse_fbx.parse(str(root/'prepared/Arms_Base.fbx'))
assert scene_payload(src)==scene_payload(patched),'Material patch altered the rig, geometry or animation payload'
for clip in ['Idle','Fire','Run','JumpStart','JumpLoop','JumpEnd']:
 shutil.copyfile(root/('exported/Animation/FirstPerson_'+clip+'.fbx'),root/('prepared/Arms_'+clip+'.fbx'))
 assert (root/('exported/Animation/FirstPerson_'+clip+'.fbx')).read_bytes()==(root/('prepared/Arms_'+clip+'.fbx')).read_bytes()
(root/'prepared/arms-native-preservation.json').write_text(json.dumps({'materialEdits':changes,'animations':'original Unreal FBX bytes copied without rebaking','transforms':'original parsed transforms preserved'},indent=2))
print('MATERIAL PATCH',changes,'PRESERVED SIX NATIVE ARM CLIPS')
