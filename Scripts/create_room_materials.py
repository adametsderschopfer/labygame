"""Create deterministic instanced materials for flooded and pool room geometry.

Run in the editor with: py "C:/ue_prj/laby/Scripts/create_room_materials.py"
"""

from pathlib import Path
import runpy
import unreal


builder = runpy.run_path(str(Path(__file__).with_name("material_builder.py")))
create = builder["create"]

create(
    "PoolTile",
    {"TileSizeCm": 24.0, "GroutWidthCm": 1.2, "FinishRoughness": 0.24},
    {"TileColor": (0.20, 0.66, 0.72), "GroutColor": (0.035, 0.075, 0.08)},
    r"""
        float3 p = WorldPos / max(TileSizeCm, 1.0);
        float3 axis = abs(normalize(SurfaceNormal));
        float2 uv = axis.z > max(axis.x, axis.y) ? p.xy : (axis.x > axis.y ? p.yz : p.xz);
        float2 edge = min(frac(uv), 1.0 - frac(uv)) * TileSizeCm;
        float grout = 1.0 - step(GroutWidthCm, min(edge.x, edge.y));
        float variation = frac(sin(dot(floor(uv), float2(12.9898, 78.233))) * 43758.5453);
        float3 tile = TileColor.rgb * lerp(0.88, 1.08, variation);
        WorldNormal = normalize(SurfaceNormal);
        Roughness = lerp(FinishRoughness, 0.72, grout);
        Emission = 0;
        return lerp(tile, GroutColor.rgb, grout);
    """,
    instanced=True,
)

create(
    "RoomWater",
    {"WaveScaleCm": 150.0, "WaveSpeed": 0.38, "FinishRoughness": 0.08},
    {"ShallowColor": (0.025, 0.26, 0.31), "HighlightColor": (0.12, 0.62, 0.70)},
    r"""
        float2 p = WorldPos.xy / max(WaveScaleCm, 1.0);
        float a = sin((p.x + p.y) * 6.28318 + GameTime * WaveSpeed * 6.28318);
        float b = sin((p.x * 1.71 - p.y * 1.19) * 6.28318 - GameTime * WaveSpeed * 4.1);
        float2 slope = float2(cos((p.x + p.y) * 6.28318 + GameTime * WaveSpeed * 6.28318),
                              cos((p.x * 1.71 - p.y * 1.19) * 6.28318 - GameTime * WaveSpeed * 4.1));
        WorldNormal = normalize(float3(-slope.x * 0.09, -slope.y * 0.09, 1.0));
        Roughness = FinishRoughness;
        Emission = HighlightColor.rgb * saturate((a + b) * 0.025 + 0.02);
        return lerp(ShallowColor.rgb, HighlightColor.rgb, saturate((a + b) * 0.08 + 0.28));
    """,
    instanced=True,
    animated=True,
)

lib = unreal.MaterialEditingLibrary
water = unreal.load_asset("/Game/Materials/Laboratory/M_RoomWater")
water.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
water.set_editor_property("two_sided", True)
opacity = lib.create_material_expression(water, unreal.MaterialExpressionScalarParameter)
opacity.set_editor_property("parameter_name", "Opacity")
opacity.set_editor_property("default_value", 0.64)
opacity.set_editor_property("group", "RoomWater")
if not lib.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY):
    raise RuntimeError("Cannot connect room-water opacity")
lib.layout_material_expressions(water)
errors = lib.recompile_material(water)
if errors:
    raise RuntimeError("Room-water material compilation failed:\n" + "\n".join(errors))
if not unreal.EditorAssetLibrary.save_loaded_asset(water):
    raise RuntimeError("Cannot save room-water material")

instance = unreal.load_asset("/Game/Materials/Laboratory/MI_RoomWater")
lib.update_material_instance(instance)
if not unreal.EditorAssetLibrary.save_loaded_asset(instance):
    raise RuntimeError("Cannot save room-water material instance")

unreal.log("LABY_ROOM_MATERIALS_SAVED")
