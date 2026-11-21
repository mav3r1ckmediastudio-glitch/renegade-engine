"""Build the original Renegade Workshop Van. Run in Blender background mode."""
import math
import sys
from pathlib import Path

import bpy

out = Path(sys.argv[sys.argv.index("--") + 1])
out.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)


def material(name, color, roughness=0.45, metallic=0.0):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*color, 1.0)
    mat.use_nodes = True
    shader = mat.node_tree.nodes.get("Principled BSDF")
    shader.inputs["Base Color"].default_value = mat.diffuse_color
    shader.inputs["Roughness"].default_value = roughness
    shader.inputs["Metallic"].default_value = metallic
    return mat


white = material("Warm ivory paint", (0.64, 0.57, 0.43), 0.34, 0.24)
teal = material("Workshop blue-green", (0.046, 0.22, 0.22), 0.38, 0.30)
iron = material("Charcoal rubber and trim", (0.023, 0.028, 0.032), 0.83)
tread = material("Tyre shoulder", (0.042, 0.046, 0.051), 0.88)
steel = material("Satin alloy", (0.34, 0.39, 0.40), 0.39, 0.68)
seat = material("Slate seat cloth", (0.065, 0.091, 0.101), 0.92)
glass = material("Smoky blue glazing", (0.047, 0.10, 0.135), 0.19, 0.26)
red = material("M_VehicleBrakeLights", (0.38, 0.017, 0.012), 0.28)
amber = material("Amber indicators", (0.82, 0.28, 0.031), 0.27)
lamp = material("Headlamp reflectors", (0.65, 0.75, 0.76), 0.22, 0.23)
yellow = material("Worksite markings", (0.88, 0.53, 0.13), 0.47)


def pivot(name, xyz=(0, 0, 0), parent=None):
    ob = bpy.data.objects.new(name, None)
    bpy.context.collection.objects.link(ob)
    ob.parent = parent
    ob.location = xyz
    return ob


root = pivot("WorkshopVan")


def finish(ob, name, mat, parent, bevel):
    ob.name = name
    ob.parent = parent
    ob.data.materials.append(mat)
    if bevel:
        mod = ob.modifiers.new("Rounded edges", "BEVEL")
        mod.width = bevel
        mod.segments = 2
        ob.modifiers.new("Face normals", "WEIGHTED_NORMAL")
    return ob


def box(name, loc, size, mat, bevel=0.014, parent=root):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    ob = bpy.context.object
    ob.dimensions = size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return finish(ob, name, mat, parent, bevel)


def mesh(name, verts, faces, mat, parent=root):
    data = bpy.data.meshes.new(name)
    data.from_pydata(verts, [], faces)
    data.update()
    ob = bpy.data.objects.new(name, data)
    bpy.context.collection.objects.link(ob)
    return finish(ob, name, mat, parent, 0)


def section(name, outline, low_x, high_x, mat, bevel):
    n = len(outline)
    verts = [(x, y, z) for x in (low_x, high_x) for y, z in outline]
    faces = [tuple(range(n - 1, -1, -1)), tuple(range(n, 2 * n))]
    faces += [(i, (i + 1) % n, (i + 1) % n + n, i + n) for i in range(n)]
    ob = mesh(name, verts, faces, mat)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.select_all(action="DESELECT")
    ob.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.object.mode_set(mode="OBJECT")
    if bevel:
        mod = ob.modifiers.new("Panel radii", "BEVEL")
        mod.width = bevel
        mod.segments = 3
        ob.modifiers.new("Face normals", "WEIGHTED_NORMAL")
    return ob


def cylinder(name, loc, radius, width, mat, parent, rotation=(0, math.pi / 2, 0), sides=24):
    bpy.ops.mesh.primitive_cylinder_add(vertices=sides, radius=radius, depth=width,
                                        end_fill_type="NGON", location=loc, rotation=rotation)
    ob = bpy.context.object
    return finish(ob, name, mat, parent, 0.009)


# Dimensions use the chassis centre. Blender +Y imports as Renegade +Z.
box("Chassis rails", (0, 0, -0.07), (1.43, 4.02, 0.20), iron, 0.07)
box("Cab and load floor", (0, 0, 0.062), (1.56, 3.96, 0.14), teal, 0.03)
section("Nose", [(0.82, 0.02), (2.03, 0.02), (2.06, 0.25), (1.86, 0.36),
                 (0.94, 0.40), (0.79, 0.31)], -0.80, 0.80, teal, 0.055)
section("Roof", [(-2.01, 0.92), (0.49, 0.92), (0.59, 0.99), (0.46, 1.09),
                 (-1.83, 1.09), (-2.03, 1.02)], -0.76, 0.76, white, 0.055)
box("Front bumper", (0, 2.04, 0.035), (1.72, 0.19, 0.19), iron, 0.04)
box("Rear step", (0, -2.05, 0.01), (1.73, 0.22, 0.17), iron, 0.045)
box("Radiator inset", (0, 2.023, 0.214), (1.01, 0.076, 0.153), iron, 0.026)
for z in (0.17, 0.205, 0.24, 0.275):
    box("Grille bar", (0, 2.068, z), (0.94, 0.018, 0.012), steel, 0.004)
for side in (-1, 1):
    x = side * 0.788
    a, b = sorted((x - side * 0.055, x + side * 0.045))
    # Open wheel arches and continuous painted sills between axles.
    outline = [(-2.02, 0.025)]
    for axle in (-1.42, 1.42):
        outline += [(axle + 0.39 * math.cos(math.pi - i * math.pi / 10),
                     -0.14 + 0.39 * math.sin(math.pi - i * math.pi / 10))
                    for i in range(11)]
    outline += [(2.03, 0.015), (1.89, 0.35), (0.91, 0.43), (-2.02, 0.43)]
    section("Arched sill", outline, a, b, teal, 0.018)
    box("Cargo panel", (x, -1.01, 0.69), (0.084, 2.01, 0.56), white, 0.033)
    box("Pressed cargo insert", (side * 0.838, -1.04, 0.71), (0.018, 1.62, 0.33), white, 0.013)
    box("Side rub strip", (side * 0.846, -0.43, 0.27), (0.034, 2.95, 0.069), iron, 0.012)
    box("Door handle", (side * 0.85, -0.13, 0.49), (0.048, 0.21, 0.037), iron, 0.009)
    section("A pillar", [(0.85, 0.38), (0.99, 0.39), (0.59, 0.99), (0.49, 0.98)], a, b, white, 0.012)
    section("Window1" if side < 0 else "Window2",
            [(0.04, 0.45), (0.85, 0.45), (0.52, 0.93), (0.04, 0.93)],
            a + 0.02, b - 0.02, glass, 0)
    box("Mirror mount", (side * 0.87, 0.73, 0.62), (0.19, 0.067, 0.056), iron)
    box("Mirror cap", (side * 0.976, 0.73, 0.69), (0.129, 0.115, 0.18), iron, 0.027)
    box("Headlight surround", (side * 0.643, 2.008, 0.237), (0.275, 0.086, 0.16), iron, 0.027)
    box("Headlight", (side * 0.633, 2.056, 0.244), (0.204, 0.029, 0.101), lamp, 0.016)
    box("Front indicator", (side * 0.772, 2.042, 0.23), (0.048, 0.041, 0.091), amber, 0.011)
    box("Rear lamp surround", (side * 0.709, -2.035, 0.42), (0.20, 0.078, 0.37), iron, 0.024)
    box("Rear indicator", (side * 0.709, -2.083, 0.37), (0.142, 0.029, 0.09), amber, 0.012)
    box("Seat base", (side * 0.32, -0.24, 0.27), (0.49, 0.49, 0.15), seat, 0.048)
    back = box("Seat back", (side * 0.32, -0.46, 0.51), (0.47, 0.125, 0.48), seat, 0.051)
    back.rotation_euler.x = math.radians(-8)
    box("Headrest", (side * 0.32, -0.49, 0.79), (0.25, 0.115, 0.14), seat, 0.038)
    box("Rear cargo door", (side * 0.367, -2.008, 0.66), (0.70, 0.09, 0.62), white, 0.03)
    box("Rear door hinge", (side * 0.65, -2.06, 0.8), (0.14, 0.037, 0.059), steel, 0.008)
    box("Rear access handle", (side * 0.18, -2.073, 0.48), (0.16, 0.051, 0.037), iron, 0.011)
    for y in (-1.33, -0.63):
        box("Cargo identification stripe", (side * 0.855, y, 0.69), (0.016, 0.29, 0.066), yellow, 0.004)

mesh("Windshield", [(-0.73, 0.90, 0.43), (0.73, 0.90, 0.43),
                    (0.71, 0.53, 0.94), (-0.71, 0.53, 0.94)],
     [(0, 1, 2, 3)], glass)
box("Dashboard", (0, 0.744, 0.30), (1.50, 0.31, 0.18), iron, 0.042)
box("Instrument binnacle", (-0.32, 0.64, 0.409), (0.43, 0.21, 0.12), iron, 0.033)
box("Instrument face", (-0.32, 0.527, 0.405), (0.32, 0.027, 0.065), glass, 0.017)
box("Rear license plate", (0, -2.101, 0.218), (0.31, 0.026, 0.097), lamp, 0.012)

# The shared brake material only covers the independently emissive lens geometry.
lenses = [box("Lens", (x, -2.085, 0.516), (0.142, 0.032, 0.122), red, 0.015)
          for x in (-0.709, 0.709)]
for ob in lenses:
    bpy.context.view_layer.objects.active = ob
    for mod in list(ob.modifiers):
        bpy.ops.object.modifier_apply(modifier=mod.name)
bpy.ops.object.select_all(action="DESELECT")
for ob in lenses:
    ob.select_set(True)
bpy.context.view_layer.objects.active = lenses[0]
bpy.ops.object.join()
bpy.context.object.name = "BrakeLights"

steering = pivot("SteeringWheel", (-0.32, 0.375, 0.34), root)
bpy.ops.mesh.primitive_torus_add(major_segments=32, minor_segments=10, major_radius=0.169,
                                 minor_radius=0.021, rotation=(math.pi / 2, 0, 0))
finish(bpy.context.object, "Steering rim", iron, steering, 0)
box("Steering centre", (0, 0, 0), (0.17, 0.055, 0.09), iron, 0.022, steering)
box("Steering spoke", (0, 0, -0.087), (0.046, 0.032, 0.14), steel, 0.009, steering)
box("Steering crossbar", (0, 0.011, 0.01), (0.30, 0.028, 0.041), steel, 0.008, steering)

for name, x, y in (("Wheel_FL", -0.855, 1.42), ("Wheel_FR", 0.855, 1.42),
                   ("Wheel_RL", -0.855, -1.42), ("Wheel_RR", 0.855, -1.42)):
    wheel = pivot(name, (x, y, -0.205), root)
    cylinder("Tyre", (0, 0, 0), 0.322, 0.205, iron, wheel, sides=32)
    for side in (-1, 1):
        cylinder("Sidewall", (side * 0.096, 0, 0), 0.301, 0.03, tread, wheel, sides=32)
        cylinder("Wheel rim", (side * 0.119, 0, 0), 0.177, 0.036, steel, wheel)
        cylinder("Wheel hub", (side * 0.145, 0, 0), 0.070, 0.035, iron, wheel)
        for i in range(6):
            angle = i * math.tau / 6
            cylinder("Lug inset", (side * 0.142, 0.114 * math.cos(angle), 0.114 * math.sin(angle)),
                     0.021, 0.018, iron, wheel, sides=8)
    for i in range(24):
        angle = i * math.tau / 24
        ob = box("Tread block", (0, 0.318 * math.cos(angle), 0.318 * math.sin(angle)),
                 (0.17, 0.018, 0.037), tread, 0.004, wheel)
        ob.rotation_euler.x = angle

bpy.ops.wm.save_as_mainfile(filepath=str(out / "WorkshopVan.blend"))
bpy.ops.export_scene.gltf(filepath=str(out / "WorkshopVan.glb"), export_format="GLB",
                          export_apply=True, export_texcoords=False, export_normals=True)
