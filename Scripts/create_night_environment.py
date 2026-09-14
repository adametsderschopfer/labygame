"""Import ground and night assets; run with UnrealEditor-Cmd -run=pythonscript."""
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
LIB = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def texture(source, name, destination, normal=False, linear=False):
    asset = unreal.load_asset(f"{destination}/{name}")
    if asset is None:
        task = unreal.AssetImportTask()
        task.filename = str(ROOT / source)
        task.destination_path = destination
        task.destination_name = name
        task.automated = True
        task.save = True
        TOOLS.import_asset_tasks([task])
        asset = unreal.load_asset(f"{destination}/{name}")
    if asset is None:
        raise RuntimeError(f"Import failed: {source}")
    if normal:
        asset.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        # Preserve the supplied convention; this remains editable on the texture.
        asset.set_editor_property("flip_green_channel", False)
    if normal or linear:
        asset.set_editor_property("srgb", False)
    if isinstance(asset, unreal.TextureCube):
        asset.set_editor_property("max_texture_size", 2048)
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    return asset


def material(name, destination):
    result = unreal.load_asset(f"{destination}/{name}")
    if result is None:
        result = TOOLS.create_asset(name, destination, unreal.Material, unreal.MaterialFactoryNew())
    if result is None:
        raise RuntimeError(f"Cannot create {name}")
    LIB.delete_all_material_expressions(result)
    return result


def node(mat, cls, **properties):
    result = LIB.create_material_expression(mat, cls)
    for key, value in properties.items():
        result.set_editor_property(key, value)
    return result


def wire(source, target, pin, output=""):
    if not LIB.connect_material_expressions(source, output, target, pin):
        raise RuntimeError(f"Cannot connect {pin}")


def output(source, mat, prop, pin=""):
    if not LIB.connect_material_property(source, pin, prop):
        raise RuntimeError(f"Cannot connect {prop}")


def save(mat):
    LIB.layout_material_expressions(mat)
    errors = LIB.recompile_material(mat)
    if errors:
        raise RuntimeError(f"Material compilation failed: {mat.get_name()}: {errors}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(mat):
        raise RuntimeError(f"Cannot save {mat.get_name()}")


ground = material("M_MazeGround", "/Game/Materials")
ground.set_editor_property("used_with_instanced_static_meshes", True)
ground.set_editor_property("tangent_space_normal", False)
position = node(ground, unreal.MaterialExpressionWorldPosition)
xy = node(ground, unreal.MaterialExpressionComponentMask, r=True, g=True, b=False, a=False)
wire(position, xy, "")
size = node(ground, unreal.MaterialExpressionScalarParameter,
            parameter_name="TileSizeCm", default_value=200.0)
uv = node(ground, unreal.MaterialExpressionDivide)
wire(xy, uv, "A")
wire(size, uv, "B")
for source, suffix, prop, normal, linear in [
    ("Albedo.jpg", "Albedo", unreal.MaterialProperty.MP_BASE_COLOR, False, False),
    ("Normal.jpg", "Normal", unreal.MaterialProperty.MP_NORMAL, True, True),
    ("Roughness.jpg", "Roughness", unreal.MaterialProperty.MP_ROUGHNESS, False, True),
    ("AO.jpg", "AO", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION, False, True),
]:
    tex = texture(f"ArtSource/Ground/RuggedGravel/{source}", f"T_Gravel_{suffix}",
                  "/Game/Textures/Ground", normal, linear)
    sample = node(ground, unreal.MaterialExpressionTextureSample, texture=tex,
                  sampler_type=(unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else
                                unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR if linear else
                                unreal.MaterialSamplerType.SAMPLERTYPE_COLOR))
    wire(uv, sample, "UVs")
    output(sample, ground, prop, "RGB" if suffix in ("Albedo", "Normal") else "R")
save(ground)

cube = texture("ArtSource/Environment/qwantani_moon_noon_4k.hdr", "T_MoonNight",
               "/Game/Environment", linear=True)
sky = material("M_MoonNight", "/Game/Environment")
sky.set_editor_property("two_sided", True)
sky.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
sky.set_editor_property("is_sky", True)
direction = node(sky, unreal.MaterialExpressionCameraVectorWS)
outward = node(sky, unreal.MaterialExpressionMultiply, const_b=-1.0)
wire(direction, outward, "A")
sample = node(sky, unreal.MaterialExpressionTextureSample, texture=cube,
              sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
wire(outward, sample, "UVs")
brightness = node(sky, unreal.MaterialExpressionScalarParameter,
                  parameter_name="SkyBrightness", default_value=0.015)
multiply = node(sky, unreal.MaterialExpressionMultiply)
wire(sample, multiply, "A", "RGB")
wire(brightness, multiply, "B")
output(multiply, sky, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
save(sky)
unreal.log("LABY_NIGHT_ASSETS_SAVED")
