import bpy, math, struct
from pathlib import Path
out=Path(__file__).resolve().parent
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
bpy.ops.mesh.primitive_cube_add(size=2,location=(0,0,1))
cube=bpy.context.object
cube.name='Rigged Checker Actor'
mat=bpy.data.materials.new('Rig Checker Material');mat.use_nodes=True
shader=mat.node_tree.nodes.get('Principled BSDF');shader.inputs['Roughness'].default_value=0.65
image=bpy.data.images.new('rig_checker.png',width=64,height=64)
pixels=[]
for y in range(64):
 for x in range(64):
  pixels.extend((0.85,0.1,0.05,1.0) if ((x//8+y//8)%2) else (0.05,0.6,0.9,1.0))
image.pixels=pixels
image.filepath_raw=str(out/'rig_checker.png');image.file_format='PNG';image.save()
texture=mat.node_tree.nodes.new('ShaderNodeTexImage');texture.image=image
mat.node_tree.links.new(texture.outputs['Color'],shader.inputs['Base Color']);cube.data.materials.append(mat)
bpy.ops.object.armature_add()
rig=bpy.context.object;rig.name='Fixture Skeleton'
bpy.ops.object.mode_set(mode='EDIT')
root=rig.data.edit_bones[0];root.name='Root';root.head=(0,0,0);root.tail=(0,0,1)
child=rig.data.edit_bones.new('MovingBone');child.head=(0,0,1);child.tail=(0,0,2);child.parent=root
bpy.ops.object.mode_set(mode='OBJECT')
group=cube.vertex_groups.new(name='MovingBone');group.add(list(range(len(cube.data.vertices))),1.0,'REPLACE')
modifier=cube.modifiers.new('Skin','ARMATURE');modifier.object=rig
cube.parent=rig
bone=rig.pose.bones['MovingBone'];bone.rotation_mode='XYZ'
for name, axis in [('Wave',0),('Turn',2)]:
 rig.animation_data_create();rig.animation_data.action=bpy.data.actions.new(name)
 for frame,angle in [(1,0),(13,0.8),(25,0)]:
  bone.rotation_euler=(0,0,0);bone.rotation_euler[axis]=angle
  bone.keyframe_insert('rotation_euler',frame=frame)
 rig.animation_data.action=None
bpy.context.scene.frame_start=1;bpy.context.scene.frame_end=25;bpy.context.scene.frame_set(1)
bone.rotation_euler=(0,0,0)
bpy.ops.object.select_all(action='DESELECT');cube.select_set(True);rig.select_set(True);bpy.context.view_layer.objects.active=rig
# Export scene units in the FBX header; per-node 100x units scaling is not
# compatible with the pinned native skin conversion.
bpy.ops.export_scene.fbx(filepath=str(out/'animated_character.fbx'),use_selection=True,apply_scale_options='FBX_SCALE_ALL',object_types={'MESH','ARMATURE'},bake_anim=True,bake_anim_use_all_actions=True,bake_anim_use_nla_strips=False,path_mode='COPY',embed_textures=True,add_leaf_bones=False)
f=out/'animated_character.fbx';data=bytearray(f.read_bytes())
while True:
 pos=data.find(b'C:')
 if pos<0: break
 assert data[pos-5]==ord('S')
 n=struct.unpack_from('<I',data,pos-4)[0]
 assert bytes(data[pos:pos+n]).endswith(b'rig_checker.png')
 base=b'rig_checker.png';padding=n-len(base)
 data[pos:pos+n]=b'./'*(padding//2)+(b'/' if padding%2 else b'')+base
assert b'Users' not in data
f.write_bytes(data)
print('RIGGED ANIMATED FIXTURE CREATED',f)
