"""Build the editable door and two rigid Unreal meshes in Blender 5.0."""
from pathlib import Path
import math

import bpy


ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "Prepared"
OUT.mkdir(parents=True, exist_ok=True)

bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system = "METRIC"


def material(name, color, metallic=0.0, roughness=0.5):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*color, 1)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*color, 1)
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    return mat


steel = material("BlackSteel", (0.045, 0.057, 0.064), 0.78, 0.34)
glass = material("FrostedWireGlass", (0.68, 0.74, 0.75), 0.0, 0.82)
silver = material("SatinHardware", (0.46, 0.49, 0.48), 0.82, 0.29)
shadow = material("Recess", (0.008, 0.012, 0.014), 0.3, 0.75)

# Diffuse pattern deliberately keeps the glass opaque: a regular translucent
# material would expose unloaded visual chunks behind these resident doors.
size = 256
pixels = []
for y in range(size):
    for x in range(size):
        dx = min(x % 16, 16 - x % 16)
        dy = min(y % 16, 16 - y % 16)
        wire = dx < 1 or dy < 1
        grain = (math.sin(x * 0.47 + y * 0.13) + math.sin(y * 0.61 - x * 0.19)) * 0.012
        base = 0.53 if wire else 0.78 + grain
        pixels.extend((base * .94, base * .98, base, 1.0))
image = bpy.data.images.new("T_DoorFrostedWire", width=size, height=size)
image.pixels[:] = pixels
image.filepath_raw = str(OUT / "T_DoorFrostedWire.png")
image.file_format = "PNG"
image.save()
texture_node = glass.node_tree.nodes.new("ShaderNodeTexImage")
texture_node.image = image
glass.node_tree.links.new(texture_node.outputs["Color"],
                          glass.node_tree.nodes.get("Principled BSDF").inputs["Base Color"])


def box(name, loc, dim, mat, bevel=0.002):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = dim
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.data.materials.append(mat)
    if bevel:
        mod = obj.modifiers.new("SoftMachinedEdges", "BEVEL")
        mod.width = bevel
        mod.segments = 2
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=mod.name)
        mod = obj.modifiers.new("WeightedNormals", "WEIGHTED_NORMAL")
        bpy.ops.object.modifier_apply(modifier=mod.name)
    return obj


def cylinder(name, loc, radius, depth, mat, axis="Y"):
    bpy.ops.mesh.primitive_cylinder_add(vertices=20, radius=radius, depth=depth, location=loc)
    obj = bpy.context.object
    obj.name = name
    if axis == "Y":
        obj.rotation_euler[0] = math.pi / 2
    elif axis == "X":
        obj.rotation_euler[1] = math.pi / 2
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)
    obj.data.materials.append(mat)
    return obj


def join(name, parts):
    bpy.ops.object.select_all(action="DESELECT")
    for obj in parts:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    parts[0].name = name
    bpy.context.scene.cursor.location = (0, 0, 0)
    bpy.ops.object.origin_set(type="ORIGIN_CURSOR")
    return parts[0]


# Normalized local geometry: X spans the opening, Z its full height, Y its
# wall thickness. Runtime ISM scales these 100 cm dimensions to the opening.
frame_parts = []
for x in (-.468, .468):
    frame_parts.append(box("FrameJamb", (x, 0, 0), (.064, .92, 1), steel))
frame_parts.append(box("FrameLintel", (0, 0, .475), (1, .92, .05), steel))
frame_parts.append(box("FrameSill", (0, 0, -.49), (1, .92, .02), steel))
for z in (-.34, .03, .36):
    for y in (-.36, .36):
        frame_parts.append(box("FrameHingePlate", (-.438, y, z), (.065, .025, .095), silver, .002))
    for y in (-.47, .47):
        frame_parts.append(cylinder("FrameHingePin", (-.435, y, z), .018, .11, silver, "Z"))
for y in (-.468, .468):
    for x in (-.424, .424):
        frame_parts.append(box("FrameReveal", (x, y, -.012), (.008, .01, .95), shadow, .001))
    frame_parts.append(box("FrameHeaderReveal", (0, y, .437), (.85, .01, .008), shadow, .001))
frame = join("SM_FrostedDoorFrame", frame_parts)

leaf_parts = []
for x in (-.43, .43):
    leaf_parts.append(box("LeafStile", (x, 0, 0), (.14, .62, 1), steel))
for z, height in ((.455, .09), (-.075, .15), (-.455, .09)):
    leaf_parts.append(box("LeafRail", (0, 0, z), (.72, .62, height), steel))
# The large upper pane and the smaller lower pane remain individually editable.
for name, z, height in (("UpperFrostedGlass", .19, .455), ("LowerFrostedGlass", -.295, .235)):
    leaf_parts.append(box(name, (0, 0, z), (.705, .25, height), glass, .001))
    for y in (-.322, .322):
        for x in (-.36, .36):
            leaf_parts.append(box(name + "Bead", (x, y, z), (.018, .018, height + .02), shadow, .001))
        for edge in (-1, 1):
            leaf_parts.append(box(name + "Bead", (0, y, z + edge * (height + .014) / 2),
                                  (.73, .018, .018), shadow, .001))
for y in (-.35, .35):
    leaf_parts.append(box("LockPlate", (.36, y, -.0), (.065, .018, .14), silver, .002))
    leaf_parts.append(cylinder("Keyhole", (.36, y + math.copysign(.012, y), -.042), .006, .003, shadow))
for z in (-.34, .03, .36):
    for y in (-.31, .31):
        leaf_parts.append(box("LeafHingePlate", (-.49, y, z), (.055, .02, .08), silver, .002))
leaf = join("SM_FrostedDoorLeaf", leaf_parts)

# Local pivot is the spindle at the latch. Its only runtime animation is a
# brief lever rotation; the escutcheon remains part of the swinging leaf.
handle_parts = []
for y in (-.35, .35):
    handle_parts.append(cylinder("Spindle", (0, y, 0), .18, .16, silver))
    handle_parts.append(box("Lever", (-.38, y + math.copysign(.08, y), 0),
                            (.8, .17, .22), silver, .04))
handle = join("SM_FrostedDoorHandle", handle_parts)


def export(obj):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=str(OUT / (obj.name + ".fbx")), use_selection=True,
                             object_types={"MESH"}, axis_forward="-Y", axis_up="Z",
                             apply_unit_scale=True, add_leaf_bones=False, bake_anim=False,
                             path_mode="AUTO", mesh_smooth_type="FACE")


export(frame)
export(leaf)
export(handle)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT / "FrostedPanelDoor.blend"))
print("FROSTED_DOOR_SAVED", OUT)
