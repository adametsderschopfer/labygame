from __future__ import annotations

import json
import math
import sys
from pathlib import Path

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector


ASSET_ROOT = Path(__file__).resolve().parent.parent
SOURCE_DIR = ASSET_ROOT / "Original" / "Meshy_AI_Secure_Access_Door_0925014413_texture_fbx"
PREPARED_DIR = ASSET_ROOT / "Prepared"
TEXTURE_DIR = PREPARED_DIR / "Textures"
PREVIEW_DIR = PREPARED_DIR / "Previews"

SOURCE_FBX = SOURCE_DIR / "Meshy_AI_Secure_Access_Door_0925014413_texture.fbx"
SOURCE_TEXTURES = {
    "base_color": SOURCE_DIR / "Meshy_AI_Secure_Access_Door_0925014413_texture.png",
    "metallic": SOURCE_DIR / "Meshy_AI_Secure_Access_Door_0925014413_texture_metallic.png",
    "normal": SOURCE_DIR / "Meshy_AI_Secure_Access_Door_0925014413_texture_normal.png",
    "roughness": SOURCE_DIR / "Meshy_AI_Secure_Access_Door_0925014413_texture_roughness.png",
}

TARGET_WIDTH_METERS = 1.80
TARGET_HEIGHT_METERS = 2.50
TARGET_DEPTH_METERS = 0.58
OPEN_OFFSET_METERS = 0.62


def clear_scene() -> None:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    PREPARED_DIR.mkdir(parents=True, exist_ok=True)
    TEXTURE_DIR.mkdir(parents=True, exist_ok=True)
    PREVIEW_DIR.mkdir(parents=True, exist_ok=True)


def world_bounds(obj: bpy.types.Object) -> tuple[Vector, Vector]:
    points = [obj.matrix_world @ vertex.co for vertex in obj.data.vertices]
    minimum = Vector((min(v.x for v in points), min(v.y for v in points), min(v.z for v in points)))
    maximum = Vector((max(v.x for v in points), max(v.y for v in points), max(v.z for v in points)))
    return minimum, maximum


def copy_textures() -> dict[str, Path]:
    copied = {}
    names = {
        "base_color": "T_MeshySecureDoor_BaseColor.png",
        "metallic": "T_MeshySecureDoor_Metallic.png",
        "normal": "T_MeshySecureDoor_Normal.png",
        "roughness": "T_MeshySecureDoor_Roughness.png",
    }
    for role, source in SOURCE_TEXTURES.items():
        destination = TEXTURE_DIR / names[role]
        source_image = bpy.data.images.load(str(source), check_existing=False)
        pixels = np.empty(len(source_image.pixels), dtype=np.float32)
        source_image.pixels.foreach_get(pixels)
        prepared_image = bpy.data.images.new(
            f"Prepared_{role}",
            width=source_image.size[0],
            height=source_image.size[1],
            alpha=True,
            float_buffer=False,
        )
        prepared_image.pixels.foreach_set(pixels)
        prepared_image.filepath_raw = str(destination)
        prepared_image.file_format = "PNG"
        prepared_image.save()
        bpy.data.images.remove(prepared_image)
        bpy.data.images.remove(source_image)
        copied[role] = destination
    return copied


def make_material(textures: dict[str, Path]) -> bpy.types.Material:
    material = bpy.data.materials.new("M_MeshySecureDoor")
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    nodes.clear()

    output = nodes.new("ShaderNodeOutputMaterial")
    shader = nodes.new("ShaderNodeBsdfPrincipled")
    shader.inputs["Metallic"].default_value = 0.85
    shader.inputs["Roughness"].default_value = 0.45
    links.new(shader.outputs["BSDF"], output.inputs["Surface"])

    base_color = nodes.new("ShaderNodeTexImage")
    base_color.image = bpy.data.images.load(str(textures["base_color"]), check_existing=True)
    links.new(base_color.outputs["Color"], shader.inputs["Base Color"])

    metallic = nodes.new("ShaderNodeTexImage")
    metallic.image = bpy.data.images.load(str(textures["metallic"]), check_existing=True)
    metallic.image.colorspace_settings.name = "Non-Color"
    links.new(metallic.outputs["Color"], shader.inputs["Metallic"])

    roughness = nodes.new("ShaderNodeTexImage")
    roughness.image = bpy.data.images.load(str(textures["roughness"]), check_existing=True)
    roughness.image.colorspace_settings.name = "Non-Color"
    links.new(roughness.outputs["Color"], shader.inputs["Roughness"])

    normal_texture = nodes.new("ShaderNodeTexImage")
    normal_texture.image = bpy.data.images.load(str(textures["normal"]), check_existing=True)
    normal_texture.image.colorspace_settings.name = "Non-Color"
    normal = nodes.new("ShaderNodeNormalMap")
    normal.inputs["Strength"].default_value = 1.0
    links.new(normal_texture.outputs["Color"], normal.inputs["Color"])
    links.new(normal.outputs["Normal"], shader.inputs["Normal"])
    return material


def split_loose_parts(source: bpy.types.Object) -> list[bpy.types.Object]:
    bpy.context.view_layer.objects.active = source
    source.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.separate(type="LOOSE")
    bpy.ops.object.mode_set(mode="OBJECT")
    return [obj for obj in bpy.context.selected_objects if obj.type == "MESH"]


def keep_half(source: bpy.types.Object, name: str, clear_outer: bool) -> bpy.types.Object:
    obj = source.copy()
    obj.data = source.data.copy()
    obj.name = name
    bpy.context.scene.collection.objects.link(obj)

    mesh = bmesh.new()
    mesh.from_mesh(obj.data)
    geometry = list(mesh.verts) + list(mesh.edges) + list(mesh.faces)
    bmesh.ops.bisect_plane(
        mesh,
        geom=geometry,
        dist=0.00005,
        plane_co=Vector((0.0, 0.0, 0.0)),
        plane_no=Vector((1.0, 0.0, 0.0)),
        clear_outer=clear_outer,
        clear_inner=not clear_outer,
    )
    mesh.to_mesh(obj.data)
    mesh.free()
    obj.data.update()
    return obj


def apply_shared_transform(objects: list[bpy.types.Object]) -> None:
    minimum = Vector(
        (
            min(world_bounds(obj)[0].x for obj in objects),
            min(world_bounds(obj)[0].y for obj in objects),
            min(world_bounds(obj)[0].z for obj in objects),
        )
    )
    maximum = Vector(
        (
            max(world_bounds(obj)[1].x for obj in objects),
            max(world_bounds(obj)[1].y for obj in objects),
            max(world_bounds(obj)[1].z for obj in objects),
        )
    )
    dimensions = maximum - minimum
    scale = Vector(
        (
            TARGET_WIDTH_METERS / dimensions.x,
            TARGET_DEPTH_METERS / dimensions.y,
            TARGET_HEIGHT_METERS / dimensions.z,
        )
    )
    center_x = (minimum.x + maximum.x) * 0.5
    center_y = (minimum.y + maximum.y) * 0.5
    transform = Matrix.Diagonal((scale.x, scale.y, scale.z, 1.0)) @ Matrix.Translation(
        Vector((-center_x, -center_y, -minimum.z))
    )
    for obj in objects:
        obj.data.transform(transform)
        obj.matrix_world = Matrix.Identity(4)
        obj.data.update()


def export_object(obj: bpy.types.Object, destination: Path) -> None:
    # Unreal's FBX importer consumes vertex coordinates as centimeters for this
    # source, so export a temporary 100x copy while keeping the Blender source in
    # human-scale meters for editing and preview rendering.
    export_copy = obj.copy()
    export_copy.data = obj.data.copy()
    export_copy.data.transform(Matrix.Scale(100.0, 4))
    bpy.context.scene.collection.objects.link(export_copy)

    bpy.ops.object.select_all(action="DESELECT")
    export_copy.select_set(True)
    bpy.context.view_layer.objects.active = export_copy
    bpy.ops.export_scene.fbx(
        filepath=str(destination),
        use_selection=True,
        apply_unit_scale=True,
        apply_scale_options="FBX_SCALE_UNITS",
        axis_forward="-Y",
        axis_up="Z",
        mesh_smooth_type="FACE",
        use_tspace=True,
        add_leaf_bones=False,
        bake_anim=False,
        path_mode="RELATIVE",
        embed_textures=False,
    )
    bpy.data.objects.remove(export_copy, do_unlink=True)


def add_area_light(name: str, location: tuple[float, float, float], energy: float, color: tuple[float, float, float]) -> None:
    data = bpy.data.lights.new(name, type="AREA")
    data.energy = energy
    data.color = color
    data.shape = "DISK"
    data.size = 3.0
    light = bpy.data.objects.new(name, data)
    bpy.context.scene.collection.objects.link(light)
    light.location = location
    direction = Vector((0.0, 0.0, 1.25)) - light.location
    light.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()


def configure_preview() -> None:
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 1280
    scene.render.resolution_y = 960
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.film_transparent = False

    world = bpy.data.worlds.new("PreviewWorld")
    scene.world = world
    world.use_nodes = True
    background = world.node_tree.nodes["Background"]
    background.inputs["Color"].default_value = (0.004, 0.006, 0.008, 1.0)
    background.inputs["Strength"].default_value = 0.08

    camera_data = bpy.data.cameras.new("PreviewCamera")
    camera = bpy.data.objects.new("PreviewCamera", camera_data)
    bpy.context.scene.collection.objects.link(camera)
    camera.location = (2.35, -4.2, 2.25)
    direction = Vector((0.0, 0.0, 1.25)) - camera.location
    camera.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    camera.data.lens = 62.0
    scene.camera = camera

    add_area_light("Key", (-2.5, -2.8, 4.0), 1200.0, (0.76, 0.86, 1.0))
    add_area_light("Fill", (2.8, -2.2, 2.5), 850.0, (0.38, 0.75, 0.50))
    add_area_light("Rim", (0.0, 1.6, 3.8), 1350.0, (0.35, 0.50, 1.0))


def render_preview(left: bpy.types.Object, right: bpy.types.Object, name: str, open_amount: float) -> None:
    left.location.x = -OPEN_OFFSET_METERS * open_amount
    right.location.x = OPEN_OFFSET_METERS * open_amount
    bpy.context.view_layer.update()
    bpy.context.scene.render.filepath = str(PREVIEW_DIR / name)
    bpy.ops.render.render(write_still=True)
    left.location.x = 0.0
    right.location.x = 0.0
    bpy.context.view_layer.update()


def main() -> None:
    clear_scene()
    textures = copy_textures()
    material = make_material(textures)
    bpy.ops.import_scene.fbx(filepath=str(SOURCE_FBX), use_image_search=False)
    imported = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
    if len(imported) != 1:
        raise RuntimeError(f"Expected one source mesh, found {len(imported)}")

    parts = split_loose_parts(imported[0])
    parts.sort(key=lambda obj: len(obj.data.polygons), reverse=True)
    if len(parts) < 2:
        raise RuntimeError("Expected separate frame and leaf components in the source mesh")

    frame = parts[0]
    leaf_assembly = parts[1]
    for extra in parts[2:]:
        bpy.data.objects.remove(extra, do_unlink=True)

    frame.name = "SM_MeshySecureDoor_Frame"
    frame.data.name = frame.name
    left = keep_half(leaf_assembly, "SM_MeshySecureDoor_LeafLeft", clear_outer=True)
    right = keep_half(leaf_assembly, "SM_MeshySecureDoor_LeafRight", clear_outer=False)
    left.data.name = left.name
    right.data.name = right.name
    bpy.data.objects.remove(leaf_assembly, do_unlink=True)

    for obj in (frame, left, right):
        obj.data.materials.clear()
        obj.data.materials.append(material)

    apply_shared_transform([frame, left, right])

    left_min, left_max = world_bounds(left)
    right_min, right_max = world_bounds(right)
    if (left_min.x + left_max.x) > (right_min.x + right_max.x):
        left, right = right, left
    left.name = "SM_MeshySecureDoor_LeafLeft"
    left.data.name = left.name
    right.name = "SM_MeshySecureDoor_LeafRight"
    right.data.name = right.name
    if world_bounds(left)[1].x > 0.02 or world_bounds(right)[0].x < -0.02:
        raise RuntimeError(
            "The center split did not produce independent left and right leaves: "
            f"left={world_bounds(left)}, right={world_bounds(right)}"
        )

    export_object(frame, PREPARED_DIR / "SM_MeshySecureDoor_Frame.fbx")
    export_object(left, PREPARED_DIR / "SM_MeshySecureDoor_LeafLeft.fbx")
    export_object(right, PREPARED_DIR / "SM_MeshySecureDoor_LeafRight.fbx")

    bpy.ops.wm.save_as_mainfile(filepath=str(PREPARED_DIR / "MeshySecureAccessDoor.blend"))
    configure_preview()
    render_preview(left, right, "MeshySecureAccessDoor_Closed.png", 0.0)
    render_preview(left, right, "MeshySecureAccessDoor_Open.png", 1.0)

    report = {
        "source": str(SOURCE_FBX),
        "target_dimensions_m": [TARGET_WIDTH_METERS, TARGET_DEPTH_METERS, TARGET_HEIGHT_METERS],
        "open_offset_m": OPEN_OFFSET_METERS,
        "meshes": {},
    }
    for obj in (frame, left, right):
        minimum, maximum = world_bounds(obj)
        report["meshes"][obj.name] = {
            "vertices": len(obj.data.vertices),
            "triangles": len(obj.data.polygons),
            "bounds_min": list(minimum),
            "bounds_max": list(maximum),
        }
    (PREPARED_DIR / "MeshySecureAccessDoor.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(f"ERROR: {error}", file=sys.stderr)
        raise
