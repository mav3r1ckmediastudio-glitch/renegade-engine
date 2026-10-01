import bpy
from pathlib import Path
out=Path(__file__).resolve().parent
out.mkdir(parents=True,exist_ok=True)
(out/'textures').mkdir(exist_ok=True)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
bpy.ops.mesh.primitive_cube_add(size=2)
cube=bpy.context.object
cube.name='Static Textured Cube'
mat=bpy.data.materials.new('Checker Material')
mat.use_nodes=True
shader=mat.node_tree.nodes.get('Principled BSDF')
shader.inputs['Roughness'].default_value=0.65
image=bpy.data.images.new('checker.png',width=64,height=64)
pixels=[]
for y in range(64):
 for x in range(64):
  color=(0.85,0.1,0.05,1.0) if ((x//8+y//8)%2) else (0.05,0.6,0.9,1.0)
  pixels.extend(color)
image.pixels=pixels
image.filepath_raw=str(out/'textures'/'checker.png')
image.file_format='PNG'
image.save()
texture=mat.node_tree.nodes.new('ShaderNodeTexImage')
texture.image=image
mat.node_tree.links.new(texture.outputs['Color'],shader.inputs['Base Color'])
cube.data.materials.append(mat)
bpy.ops.export_scene.fbx(filepath=str(out/'static_textured_cube.fbx'),use_selection=True,object_types={'MESH'},bake_anim=False,path_mode='RELATIVE',embed_textures=False,add_leaf_bones=False)
bpy.ops.export_scene.fbx(filepath=str(out/'static_embedded_cube.fbx'),use_selection=True,object_types={'MESH'},bake_anim=False,path_mode='COPY',embed_textures=True,add_leaf_bones=False)
print('STATIC FBX FIXTURES CREATED',out)

import struct
for f in (out/'static_textured_cube.fbx', out/'static_embedded_cube.fbx'):
 data=bytearray(f.read_bytes())
 while True:
  pos=data.find(b'C:')
  if pos<0: break
  assert data[pos-5]==ord('S'), (f,pos)
  n=struct.unpack_from('<I',data,pos-4)[0]
  old=bytes(data[pos:pos+n])
  assert old.endswith(b'checker.png'), old
  base=b'checker.png' if 'embedded' in f.name else b'textures/checker.png'
  padding=n-len(base)
  assert padding>=0
  replacement=b'./'*(padding//2)+(b'/' if padding%2 else b'')+base
  assert len(replacement)==n
  data[pos:pos+n]=replacement
 assert b'Users' not in data
 f.write_bytes(data)
 print('Sanitized portable fixture',f.name)
