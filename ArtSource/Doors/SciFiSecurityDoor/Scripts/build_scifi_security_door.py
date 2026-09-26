from __future__ import annotations

import math
import sys
from pathlib import Path

import bpy
import numpy as np
from mathutils import Vector


ASSET_ROOT = Path(__file__).resolve().parent.parent
PREPARED_DIR = ASSET_ROOT / "Prepared"
TEXTURE_DIR = PREPARED_DIR / "Textures"
PREVIEW_DIR = PREPARED_DIR / "Previews"

PREPARED_DIR.mkdir(parents=True, exist_ok=True)
TEXTURE_DIR.mkdir(parents=True, exist_ok=True)
PREVIEW_DIR.mkdir(parents=True, exist_ok=True)


def clear_scene() -> None:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)

    for datablocks in (
        bpy.data.meshes,
        bpy.data.curves,
        bpy.data.materials,
        bpy.data.cameras,
        bpy.data.lights,
    ):
        for datablock in list(datablocks):
            if datablock.users == 0:
                datablocks.remove(datablock)


def create_collection(name: str) -> bpy.types.Collection:
    collection = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(collection)
    return collection


def move_to_collection(obj: bpy.types.Object, collection: bpy.types.Collection) -> None:
    for existing in list(obj.users_collection):
        existing.objects.unlink(obj)
    collection.objects.link(obj)


def make_box(
    name: str,
    dimensions: tuple[float, float, float],
    location: tuple[float, float, float],
    material: bpy.types.Material,
    collection: bpy.types.Collection,
    bevel: float = 0.025,
) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cube_add(location=location)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = dimensions
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)

    if bevel > 0.0:
        modifier = obj.modifiers.new(name="EdgeBevel", type="BEVEL")
        modifier.width = bevel
        modifier.segments = 3
        modifier.limit_method = "ANGLE"
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=modifier.name)

    obj.data.materials.append(material)
    move_to_collection(obj, collection)
    return obj


def make_cylinder(
    name: str,
    radius: float,
    depth: float,
    location: tuple[float, float, float],
    material: bpy.types.Material,
    collection: bpy.types.Collection,
) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cylinder_add(vertices=24, radius=radius, depth=depth, location=location)
    obj = bpy.context.object
    obj.name = name
    obj.rotation_euler.x = math.radians(90.0)
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)
    obj.data.materials.append(material)
    move_to_collection(obj, collection)
    return obj


def save_image(name: str, rgba: np.ndarray, color_space: str = "sRGB") -> Path:
    height, width, _ = rgba.shape
    image = bpy.data.images.new(name, width=width, height=height, alpha=True, float_buffer=False)
    image.colorspace_settings.name = color_space
    image.pixels.foreach_set(np.flipud(rgba).astype(np.float32).ravel())
    path = TEXTURE_DIR / f"{name}.png"
    image.filepath_raw = str(path)
    image.file_format = "PNG"
    image.save()
    bpy.data.images.remove(image)
    return path


def generate_pbr_textures(size: int = 1024) -> dict[str, Path]:
    rng = np.random.default_rng(25092026)
    y = np.linspace(0.0, 1.0, size, dtype=np.float32)[:, None]
    x = np.linspace(0.0, 1.0, size, dtype=np.float32)[None, :]

    fine_noise = rng.normal(0.0, 1.0, (size, size)).astype(np.float32)
    brushed = np.cumsum(fine_noise, axis=1)
    brushed -= brushed.min()
    brushed /= max(float(brushed.max()), 1e-6)
    brushed = (brushed - 0.5) * 0.10

    broad = (
        0.025 * np.sin(x * math.tau * 7.0)
        + 0.018 * np.sin(y * math.tau * 3.0)
        + 0.012 * fine_noise
    )
    wear = np.clip(brushed + broad, -0.10, 0.10)

    base = np.empty((size, size, 4), dtype=np.float32)
    base[..., 0] = np.clip(0.115 + wear, 0.045, 0.24)
    base[..., 1] = np.clip(0.125 + wear, 0.050, 0.25)
    base[..., 2] = np.clip(0.135 + wear, 0.055, 0.27)
    base[..., 3] = 1.0

    scratch_count = 140
    for _ in range(scratch_count):
        yy = int(rng.integers(0, size))
        xx = int(rng.integers(0, size - 20))
        length = int(rng.integers(15, 180))
        thickness = int(rng.integers(1, 3))
        value = float(rng.uniform(0.02, 0.08))
        base[max(0, yy - thickness) : min(size, yy + thickness + 1), xx : min(size, xx + length), :3] += value

    rough = np.empty((size, size, 4), dtype=np.float32)
    roughness = np.clip(0.42 + 0.18 * np.abs(fine_noise) + 0.08 * brushed, 0.28, 0.78)
    rough[..., :3] = roughness[..., None]
    rough[..., 3] = 1.0

    metallic = np.ones((size, size, 4), dtype=np.float32)
    metallic[..., :3] *= 0.94

    height = 0.55 * fine_noise + 0.45 * brushed
    grad_y, grad_x = np.gradient(height)
    strength = 0.22
    nx = -grad_x * strength
    ny = -grad_y * strength
    nz = np.ones_like(nx)
    norm = np.sqrt(nx * nx + ny * ny + nz * nz)
    normal = np.empty((size, size, 4), dtype=np.float32)
    normal[..., 0] = nx / norm * 0.5 + 0.5
    normal[..., 1] = ny / norm * 0.5 + 0.5
    normal[..., 2] = nz / norm * 0.5 + 0.5
    normal[..., 3] = 1.0

    return {
        "base": save_image("T_SciFiDoor_Metal_BaseColor", base),
        "rough": save_image("T_SciFiDoor_Metal_Roughness", rough, "Non-Color"),
        "metal": save_image("T_SciFiDoor_Metal_Metallic", metallic, "Non-Color"),
        "normal": save_image("T_SciFiDoor_Metal_Normal", normal, "Non-Color"),
    }


def load_image(path: Path, non_color: bool = False) -> bpy.types.Image:
    image = bpy.data.images.load(str(path), check_existing=True)
    if non_color:
        image.colorspace_settings.name = "Non-Color"
    return image


def make_metal_material(name: str, textures: dict[str, Path], tint: float = 1.0) -> bpy.types.Material:
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    nodes.clear()

    output = nodes.new("ShaderNodeOutputMaterial")
    shader = nodes.new("ShaderNodeBsdfPrincipled")
    shader.inputs["Base Color"].default_value = (0.12 * tint, 0.13 * tint, 0.14 * tint, 1.0)
    shader.inputs["Metallic"].default_value = 0.94
    shader.inputs["Roughness"].default_value = 0.46
    links.new(shader.outputs["BSDF"], output.inputs["Surface"])

    base = nodes.new("ShaderNodeTexImage")
    base.image = load_image(textures["base"])
    base.label = "Base Color"
    links.new(base.outputs["Color"], shader.inputs["Base Color"])

    rough = nodes.new("ShaderNodeTexImage")
    rough.image = load_image(textures["rough"], True)
    rough.label = "Roughness"
    links.new(rough.outputs["Color"], shader.inputs["Roughness"])

    metallic = nodes.new("ShaderNodeTexImage")
    metallic.image = load_image(textures["metal"], True)
    metallic.label = "Metallic"
    links.new(metallic.outputs["Color"], shader.inputs["Metallic"])

    normal_tex = nodes.new("ShaderNodeTexImage")
    normal_tex.image = load_image(textures["normal"], True)
    normal_tex.label = "Normal"
    normal = nodes.new("ShaderNodeNormalMap")
    normal.inputs["Strength"].default_value = 0.24
    links.new(normal_tex.outputs["Color"], normal.inputs["Color"])
    links.new(normal.outputs["Normal"], shader.inputs["Normal"])
    return material


def make_simple_material(
    name: str,
    color: tuple[float, float, float, float],
    metallic: float,
    roughness: float,
    emission: tuple[float, float, float, float] | None = None,
    emission_strength: float = 0.0,
) -> bpy.types.Material:
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    shader = material.node_tree.nodes.get("Principled BSDF")
    shader.inputs["Base Color"].default_value = color
    shader.inputs["Metallic"].default_value = metallic
    shader.inputs["Roughness"].default_value = roughness
    if emission is not None:
        emission_input = shader.inputs.get("Emission Color") or shader.inputs.get("Emission")
        if emission_input:
            emission_input.default_value = emission
        if shader.inputs.get("Emission Strength"):
            shader.inputs["Emission Strength"].default_value = emission_strength
    return material


def add_text(
    body: str,
    name: str,
    location: tuple[float, float, float],
    size: float,
    material: bpy.types.Material,
    collection: bpy.types.Collection,
) -> bpy.types.Object:
    curve = bpy.data.curves.new(type="FONT", name=f"{name}_Curve")
    curve.body = body
    curve.align_x = "CENTER"
    curve.align_y = "CENTER"
    curve.size = size
    curve.extrude = 0.003
    obj = bpy.data.objects.new(name, curve)
    collection.objects.link(obj)
    obj.location = location
    obj.rotation_euler.x = math.radians(90.0)
    obj.data.materials.append(material)
    return obj


def add_camera(location: tuple[float, float, float], target: tuple[float, float, float]) -> bpy.types.Object:
    camera_data = bpy.data.cameras.new("PreviewCamera")
    camera = bpy.data.objects.new("PreviewCamera", camera_data)
    bpy.context.scene.collection.objects.link(camera)
    camera.location = location
    direction = Vector(target) - camera.location
    camera.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    camera.data.lens = 58.0
    bpy.context.scene.camera = camera
    return camera


def add_area_light(
    name: str,
    location: tuple[float, float, float],
    energy: float,
    color: tuple[float, float, float],
    size: float,
    target: tuple[float, float, float],
) -> None:
    light_data = bpy.data.lights.new(name, type="AREA")
    light_data.energy = energy
    light_data.color = color
    light_data.shape = "RECTANGLE"
    light_data.size = size
    light_data.size_y = size
    light = bpy.data.objects.new(name, light_data)
    bpy.context.scene.collection.objects.link(light)
    light.location = location
    direction = Vector(target) - light.location
    light.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()


def build_door() -> tuple[bpy.types.Object, bpy.types.Object]:
    textures = generate_pbr_textures()
    gunmetal = make_metal_material("M_SciFiDoor_Gunmetal", textures, 0.78)
    leaf_metal = make_metal_material("M_SciFiDoor_LeafMetal", textures, 1.18)
    trim = make_simple_material("M_SciFiDoor_Trim", (0.018, 0.022, 0.026, 1.0), 0.8, 0.25)
    black = make_simple_material("M_SciFiDoor_Black", (0.006, 0.008, 0.010, 1.0), 0.35, 0.28)
    green = make_simple_material(
        "M_SciFiDoor_GreenEmissive",
        (0.025, 0.34, 0.045, 1.0),
        0.0,
        0.2,
        (0.12, 1.0, 0.19, 1.0),
        12.0,
    )
    white = make_simple_material("M_SciFiDoor_White", (0.92, 0.96, 0.92, 1.0), 0.0, 0.4)

    frame = create_collection("DoorFrame")
    left_leaf = create_collection("DoorLeaf_Left")
    right_leaf = create_collection("DoorLeaf_Right")

    root = bpy.data.objects.new("SciFiSecurityDoor_Root", None)
    bpy.context.scene.collection.objects.link(root)
    left_pivot = bpy.data.objects.new("DoorLeaf_Left_Pivot", None)
    right_pivot = bpy.data.objects.new("DoorLeaf_Right_Pivot", None)
    bpy.context.scene.collection.objects.link(left_pivot)
    bpy.context.scene.collection.objects.link(right_pivot)
    left_pivot.parent = root
    right_pivot.parent = root

    outer_width = 4.40
    outer_height = 3.40
    opening_width = 3.30
    opening_bottom = 0.20
    opening_top = 2.82
    side_width = (outer_width - opening_width) * 0.5
    depth = 0.42

    make_box("SM_DoorFrame_Left", (side_width, depth, outer_height), (-1.925, 0.0, 1.70), gunmetal, frame, 0.045)
    make_box("SM_DoorFrame_Right", (side_width, depth, outer_height), (1.925, 0.0, 1.70), gunmetal, frame, 0.045)
    make_box("SM_DoorFrame_Top", (outer_width, depth, 0.58), (0.0, 0.0, 3.11), gunmetal, frame, 0.045)
    make_box("SM_DoorFrame_Sill", (opening_width + 0.30, depth * 0.82, opening_bottom), (0.0, 0.0, 0.10), gunmetal, frame, 0.02)

    # Recessed black liners and sliding tracks define the opening silhouette.
    for x_value, suffix in ((-1.685, "L"), (1.685, "R")):
        make_box(f"SM_DoorFrame_InnerTrim_{suffix}", (0.12, 0.13, 2.66), (x_value, -0.235, 1.51), trim, frame, 0.014)
        make_box(f"SM_DoorFrame_InnerRail_{suffix}", (0.055, 0.06, 2.52), (x_value * 0.975, -0.31, 1.51), black, frame, 0.008)

    make_box("SM_DoorFrame_TrackTop", (3.42, 0.12, 0.12), (0.0, -0.235, opening_top + 0.01), trim, frame, 0.012)
    make_box("SM_DoorFrame_TrackBottom", (3.42, 0.14, 0.11), (0.0, -0.235, opening_bottom + 0.02), trim, frame, 0.012)

    # Layered outer armour creates the chamfered, heavy industrial look of the reference.
    for x_value, suffix in ((-2.11, "L"), (2.11, "R")):
        make_box(f"SM_DoorFrame_OuterArmor_{suffix}", (0.16, 0.46, 3.08), (x_value, 0.005, 1.55), gunmetal, frame, 0.022)
        for z_value in (0.38, 2.73):
            make_box(f"SM_DoorFrame_Seam_{suffix}_{z_value}", (0.19, 0.025, 0.022), (x_value, -0.239, z_value), black, frame, 0.002)

    make_box("SM_DoorFrame_TopCap", (4.16, 0.47, 0.11), (0.0, 0.0, 3.38), gunmetal, frame, 0.025)
    make_box("SM_DoorFrame_TopShadow", (3.42, 0.08, 0.055), (0.0, -0.255, 2.93), black, frame, 0.005)

    # Top status light.
    make_box("SM_DoorFrame_StatusHousing", (0.52, 0.11, 0.23), (0.0, -0.275, 3.12), black, frame, 0.025)
    make_box("SM_DoorFrame_StatusGlow", (0.30, 0.035, 0.065), (0.0, -0.342, 3.13), green, frame, 0.012)

    # Access panel on the right pillar.
    make_box("SM_DoorFrame_AccessHousing", (0.35, 0.12, 0.72), (1.92, -0.285, 1.65), black, frame, 0.024)
    make_box("SM_DoorFrame_AccessScreen", (0.29, 0.035, 0.27), (1.92, -0.363, 1.82), green, frame, 0.012)
    make_box("SM_DoorFrame_AccessSlot", (0.22, 0.035, 0.025), (1.92, -0.363, 1.39), trim, frame, 0.006)
    add_text("ACCESS\nGRANTED", "SM_DoorFrame_AccessText", (1.92, -0.386, 1.70), 0.095, white, frame)

    for x_value, suffix in ((-1.91, "L"), (1.91, "R")):
        for z_value in (0.31, 3.14):
            make_cylinder(f"SM_DoorFrame_Bolt_{suffix}_{z_value}", 0.040, 0.025, (x_value, -0.233, z_value), black, frame)

    leaf_width = 1.635
    leaf_height = opening_top - opening_bottom - 0.10
    leaf_center_z = opening_bottom + 0.05 + leaf_height * 0.5
    leaf_x = leaf_width * 0.5 + 0.008

    def create_leaf(collection: bpy.types.Collection, sign: float, pivot: bpy.types.Object, suffix: str) -> None:
        x_center = sign * leaf_x
        pivot.location = (x_center, 0.0, leaf_center_z)
        slab = make_box(
            f"SM_DoorLeaf_{suffix}",
            (leaf_width, 0.19, leaf_height),
            (x_center, 0.0, leaf_center_z),
            leaf_metal,
            collection,
            0.028,
        )

        slab.parent = pivot
        slab.matrix_parent_inverse = pivot.matrix_world.inverted()

        # Deep side recess and armour strips mimic the vertical inset in the reference.
        recess_x = x_center - sign * 0.49
        recess = make_box(
            f"SM_DoorLeaf_{suffix}_Recess",
            (0.18, 0.055, 1.42),
            (recess_x, -0.122, leaf_center_z),
            black,
            collection,
            0.022,
        )
        recess.parent = pivot
        recess.matrix_parent_inverse = pivot.matrix_world.inverted()

        for strip_x, index in ((x_center - sign * 0.705, 0), (x_center + sign * 0.715, 1)):
            strip = make_box(
                f"SM_DoorLeaf_{suffix}_Border_{index}",
                (0.045, 0.035, leaf_height - 0.08),
                (strip_x, -0.118, leaf_center_z),
                trim,
                collection,
                0.006,
            )
            strip.parent = pivot
            strip.matrix_parent_inverse = pivot.matrix_world.inverted()

        for z_value, index in ((opening_bottom + 0.10, 0), (opening_top - 0.10, 1)):
            strip = make_box(
                f"SM_DoorLeaf_{suffix}_BorderH_{index}",
                (leaf_width - 0.08, 0.035, 0.045),
                (x_center, -0.118, z_value),
                trim,
                collection,
                0.006,
            )
            strip.parent = pivot
            strip.matrix_parent_inverse = pivot.matrix_world.inverted()

        # Central meeting seal.
        seal_x = x_center - sign * (leaf_width * 0.5 - 0.025)
        seal = make_box(
            f"SM_DoorLeaf_{suffix}_CenterSeal",
            (0.055, 0.06, leaf_height - 0.02),
            (seal_x, -0.13, leaf_center_z),
            black,
            collection,
            0.007,
        )
        seal.parent = pivot
        seal.matrix_parent_inverse = pivot.matrix_world.inverted()

    create_leaf(left_leaf, -1.0, left_pivot, "Left")
    create_leaf(right_leaf, 1.0, right_pivot, "Right")

    for obj in frame.objects:
        obj.parent = root

    root["asset_role"] = "presentation"
    root["dimensions_m"] = [outer_width, depth, outer_height]
    root["opening_dimensions_m"] = [opening_width, opening_top - opening_bottom]
    left_pivot["motion_axis"] = "X"
    left_pivot["open_offset_m"] = -1.42
    right_pivot["motion_axis"] = "X"
    right_pivot["open_offset_m"] = 1.42
    return left_pivot, right_pivot


def configure_render() -> None:
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE_NEXT"
    scene.render.resolution_x = 1280
    scene.render.resolution_y = 800
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.film_transparent = False

    world = bpy.data.worlds.new("DoorPreviewWorld") if not scene.world else scene.world
    scene.world = world
    world.use_nodes = True
    background = world.node_tree.nodes.get("Background")
    background.inputs["Color"].default_value = (0.002, 0.003, 0.004, 1.0)
    background.inputs["Strength"].default_value = 0.06

    floor_material = make_simple_material("M_PreviewFloor", (0.006, 0.008, 0.010, 1.0), 0.25, 0.30)
    make_box("PreviewFloor", (12.0, 8.0, 0.06), (0.0, 0.0, -0.055), floor_material, bpy.context.scene.collection, 0.0)

    add_camera((5.2, -8.4, 4.05), (0.0, 0.0, 1.62))
    add_area_light("Key", (-3.6, -4.0, 5.4), 1350.0, (0.78, 0.88, 1.0), 4.0, (0.0, 0.0, 1.6))
    add_area_light("Fill", (4.0, -2.5, 3.0), 900.0, (0.40, 0.82, 0.56), 3.0, (0.5, 0.0, 1.7))
    add_area_light("Rim", (0.0, 2.5, 4.5), 1500.0, (0.45, 0.65, 1.0), 2.8, (0.0, 0.0, 2.0))


def export_scene(left_pivot: bpy.types.Object, right_pivot: bpy.types.Object) -> None:
    scene = bpy.context.scene
    left_pivot.location.x = -0.8255
    right_pivot.location.x = 0.8255
    bpy.context.view_layer.update()

    blend_path = PREPARED_DIR / "SciFiSecurityDoor.blend"
    bpy.ops.wm.save_as_mainfile(filepath=str(blend_path))

    glb_path = PREPARED_DIR / "SciFiSecurityDoor.glb"
    bpy.ops.export_scene.gltf(
        filepath=str(glb_path),
        export_format="GLB",
        export_apply=True,
        export_yup=True,
        use_visible=True,
    )

    fbx_path = PREPARED_DIR / "SciFiSecurityDoor.fbx"
    bpy.ops.export_scene.fbx(
        filepath=str(fbx_path),
        use_selection=False,
        apply_unit_scale=True,
        apply_scale_options="FBX_SCALE_UNITS",
        axis_forward="-Y",
        axis_up="Z",
        add_leaf_bones=False,
        bake_anim=False,
        path_mode="RELATIVE",
        embed_textures=False,
    )

    scene.render.filepath = str(PREVIEW_DIR / "SciFiSecurityDoor_Closed.png")
    bpy.ops.render.render(write_still=True)

    left_pivot.location.x -= 1.42
    right_pivot.location.x += 1.42
    bpy.context.view_layer.update()
    scene.render.filepath = str(PREVIEW_DIR / "SciFiSecurityDoor_Open.png")
    bpy.ops.render.render(write_still=True)

    left_pivot.location.x += 1.42
    right_pivot.location.x -= 1.42
    bpy.context.view_layer.update()
    bpy.ops.wm.save_as_mainfile(filepath=str(blend_path))


def main() -> None:
    clear_scene()
    left_pivot, right_pivot = build_door()
    configure_render()
    export_scene(left_pivot, right_pivot)
    print(f"Built SciFiSecurityDoor in {PREPARED_DIR}")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(f"ERROR: {error}", file=sys.stderr)
        raise
