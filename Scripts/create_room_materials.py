"""Rebuild generated room materials (overwrites graphs, preserves package paths).

Run in the laby editor with Play stopped. No third-party textures or Water plugin.
"""
from pathlib import Path
import runpy
import unreal

if Path(unreal.Paths.project_dir()).resolve() != Path(__file__).resolve().parent.parent:
    raise RuntimeError("Wrong project: this authoring script must run inside laby")
if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    raise RuntimeError("Stop Play before rebuilding room materials")

builder = runpy.run_path(str(Path(__file__).with_name("material_builder.py")))
create = builder["create"]
lib = unreal.MaterialEditingLibrary
folder = builder["FOLDER"]  # Keep existing compatible package references stable.

def water_output(material):
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)
    material.set_editor_property("two_sided", False)
    output = lib.create_material_expression(material, unreal.MaterialExpressionSingleLayerWaterMaterialOutput)
    for pin, color in [
        ("ScatteringCoefficients", (0.00015, 0.00035, 0.00045)),
        ("AbsorptionCoefficients", (0.0040, 0.0015, 0.0006)),
    ]:
        parameter = lib.create_material_expression(material, unreal.MaterialExpressionVectorParameter)
        parameter.set_editor_property("parameter_name", pin)
        parameter.set_editor_property("default_value", unreal.LinearColor(*color, 1.0))
        parameter.set_editor_property("group", "RoomWater")
        if not lib.connect_material_expressions(parameter, "", output, pin):
            raise RuntimeError(f"Cannot connect Single Layer Water {pin}")
    for name, value, prop in [
        ("SurfaceOpacity", 0.0, unreal.MaterialProperty.MP_OPACITY),
        ("WaterSpecular", 0.25, unreal.MaterialProperty.MP_SPECULAR),
    ]:
        parameter = lib.create_material_expression(material, unreal.MaterialExpressionScalarParameter)
        parameter.set_editor_property("parameter_name", name)
        parameter.set_editor_property("default_value", value)
        parameter.set_editor_property("group", "RoomWater")
        if not lib.connect_material_property(parameter, "", prop):
            raise RuntimeError(f"Cannot connect {name}")

create(
    "PoolTile",
    {"TileSizeCm": 15.0, "GroutWidthCm": 0.3, "FinishRoughness": 0.22},
    {"TileColor": (0.42, 0.70, 0.73), "GroutColor": (0.18, 0.24, 0.25)},
    r"""
        float3 n = normalize(SurfaceNormal);
        float3 axis = abs(n);
        float3 u = axis.z > max(axis.x, axis.y) ? float3(1,0,0) :
                   (axis.x > axis.y ? float3(0,1,0) : float3(1,0,0));
        float3 v = axis.z > max(axis.x, axis.y) ? float3(0,1,0) : float3(0,0,1);
        float2 uv = float2(dot(WorldPos,u), dot(WorldPos,v)) / max(TileSizeCm,1.0);
        float2 f = frac(uv);
        float2 edge = min(f,1-f) * TileSizeCm;
        float d = min(edge.x,edge.y);
        float aa = max(fwidth(d),0.02);
        float grout = 1-smoothstep(GroutWidthCm*0.5-aa,GroutWidthCm*0.5+aa,d);
        float bevel = 1-smoothstep(GroutWidthCm*0.5,GroutWidthCm*0.5+0.14,d);
        float2 direction = (edge.x < edge.y ? float2(1,0) : float2(0,1)) * sign(0.5-f);
        float variation = frac(sin(dot(floor(uv),float2(12.9898,78.233)))*43758.5453);
        WorldNormal = normalize(n - (u*direction.x+v*direction.y)*bevel*(1-grout)*0.12);
        Roughness = lerp(FinishRoughness,0.7,grout);
        Emission = 0;
        return lerp(TileColor.rgb*lerp(0.94,1.04,variation),GroutColor.rgb,grout);
    """,
    instanced=True,
)

create(
    "RoomWater",
    {"RippleStrength": 0.022, "RippleSpeed": 0.30, "FinishRoughness": 0.09},
    {},
    r"""
        // Small, independent wave directions. Derivatives are continuous across chunk edges.
        float2 p = WorldPos.xy;
        float t = GameTime * RippleSpeed;
        float2 slope = float2(0,0);
        slope += float2(0.94,0.34)*cos(dot(p,float2(0.94,0.34))*0.071+t*1.7);
        slope += float2(-0.39,0.92)*cos(dot(p,float2(-0.39,0.92))*0.109-t*2.1)*0.55;
        slope += float2(0.64,-0.77)*cos(dot(p,float2(0.64,-0.77))*0.193+t*2.9)*0.24;
        WorldNormal = normalize(float3(slope*RippleStrength,1));
        Roughness = FinishRoughness;
        Emission = 0;
        return float3(0,0,0);
    """,
    animated=True,
    configure=water_output,
)

# These existing masters now serve chunk ISMs as well as their original meshes.
# Do not rebuild their carefully authored graphs or overwrite instance tuning.
for name in ("LabCeilingMineral", "MazeWallCeramic"):
    material = unreal.load_asset(f"{folder}/M_{name}")
    if material is None:
        raise RuntimeError(f"Missing existing material M_{name}")
    material.set_editor_property("used_with_instanced_static_meshes", True)
    errors = lib.recompile_material(material)
    if errors:
        raise RuntimeError(f"{name}: {errors}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError(f"Cannot save M_{name}")

for name in ("RoomWater", "PoolTile", "LabCeilingMineral", "MazeWallCeramic"):
    material = unreal.load_asset(f"{folder}/M_{name}")
    errors = lib.recompile_material(material)
    if errors:
        raise RuntimeError(f"{name}: {errors}")
    unreal.log(f"LABY_MATERIAL_VERIFIED {name}: shading={material.get_editor_property('shading_model')}, "
               f"instanced={material.get_editor_property('used_with_instanced_static_meshes')}")

unreal.log("LABY_ROOM_MATERIALS_SAVED")
