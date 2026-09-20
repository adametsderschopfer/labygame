"""Rebuild only the cell-aligned square ceiling, without touching walls/floor."""
from pathlib import Path
import runpy
import unreal

create = runpy.run_path(str(Path(__file__).with_name("material_builder.py")))["create"]
textures = {}
for parameter, suffix, normal, linear in [
    ("AcousticColor", "color_4k.jpg", False, False),
    ("AcousticNormal", "normal_directx_4k.png", True, True),
    ("AcousticRoughness", "roughness_4k.jpg", False, True),
]:
    destination = "/Game/Textures/Ceiling"
    name = f"T_Ceiling_{parameter}"
    asset = unreal.load_asset(f"{destination}/{name}")
    if asset is None:
        task = unreal.AssetImportTask()
        task.filename = str(Path(unreal.Paths.project_dir()) / "ArtSource/Ceiling/TextureCan0013" / f"tiles_0013_{suffix}")
        task.destination_path = destination
        task.destination_name = name
        task.automated = True
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        asset = unreal.load_asset(f"{destination}/{name}")
    if asset is None:
        raise RuntimeError(f"Cannot import {name}")
    asset.set_editor_property("srgb", not linear)
    if normal:
        asset.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        asset.set_editor_property("flip_green_channel", False)
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
        raise RuntimeError(f"Cannot save {name}")
    textures[parameter] = asset

create("LabCeilingMineral", {
    # Runtime adapter supplies these dimensions from its read-only ECS snapshot.
    "CellSizeCm": 462.5, "WallThicknessCm": 50.0,
    "TargetPanelSizeCm": 120.0, "RailWidthCm": 2.4,
    "LightStrength": 2.3, "PoreContrast": 0.48,
    "OffFraction": 0.18, "FlickerFraction": 0.12,
    "FlickerPeriodSeconds": 12.0,
}, {"MazeOrigin": (0.0, 0.0, 0.0), "MazeSeed": (0.0, 0.0, 0.0),
    "PanelColor": (0.72, 0.74, 0.73), "RailColor": (0.42, 0.45, 0.44),
    "LightColor": (0.96, 0.98, 1.0)}, r"""
struct CeilingPattern
{
    float Hash(float2 p)
    {
        float3 q = frac(float3(p.x, p.y, p.x) * 0.1031);
        q += dot(q, q.yzx + 33.33);
        return frac((q.x + q.y) * q.z);
    }
    float3 Noise(float2 p)
    {
        float2 cell = floor(p);
        float2 f = frac(p);
        float2 w = f * f * (3.0 - 2.0 * f);
        float2 dw = 6.0 * f * (1.0 - f);
        float a = Hash(cell);
        float b = Hash(cell + float2(1, 0));
        float c = Hash(cell + float2(0, 1));
        float d = Hash(cell + float2(1, 1));
        return float3(lerp(lerp(a, b, w.x), lerp(c, d, w.x), w.y),
                      lerp(b - a, d - c, w.y) * dw.x,
                      lerp(c - a, d - b, w.x) * dw.y);
    }
};
CeilingPattern pattern;
float pitch = max(CellSizeCm, 10.0);
float thickness = clamp(WallThicknessCm, 0.0, pitch * 0.8);
float count = max(round(pitch / max(TargetPanelSizeCm, 10.0)), 3.0);
float size = pitch / count;
float2 p = WorldPos.xy - MazeOrigin.xy;
float2 bay = floor(p / pitch);
float2 within = p - bay * pitch;
float2 pixel = max(abs(ddx(p)) + abs(ddy(p)), 0.01);
// Continuous acoustic panels across open bays, with no full-width trim bands.
// Only passive perimeter tiles are cut by walls, as in a suspended ceiling.
float2 grid = within / size;
float2 tile = floor(grid);
float2 f = frac(grid);
float2 edge = (0.5 - abs(f - 0.5)) * size;
float rail = clamp(RailWidthCm, 0.1, size * 0.1);
float2 inside = smoothstep(rail * 0.5 - pixel * 0.5, rail * 0.5 + pixel * 0.5, edge);
float mask = inside.x * inside.y;
// One complete diffuser per bay (previously nine). Never intersect a wall strip.
float center = floor((count - 1.0) * 0.5);
float fixture = all(abs(tile - center) < 0.5) ? 1.0 : 0.0;
float2 start = tile * size;
float2 end = start + size;
fixture *= all(start >= thickness * 0.5) && all(end <= pitch - thickness * 0.5) ? 1.0 : 0.0;
fixture *= saturate(-SurfaceNormal.z);
float2 diffuser = smoothstep(rail + 1.0 - pixel * 0.5, rail + 1.0 + pixel * 0.5, edge);
float lamp = fixture * diffuser.x * diffuser.y;

// Static cosmetic state is reproducible from the replicated maze seed and tile ID.
float2 id = bay * count + tile + MazeSeed.xy;
float state = pattern.Hash(id + 19.73);
float offChance = saturate(OffFraction);
float flickerChance = clamp(FlickerFraction, 0.0, 1.0 - offChance);
float power = state < offChance ? 0.0 : 1.0;
float isFlickering = state >= offChance && state < offChance + flickerChance ? 1.0 : 0.0;
float phase = pattern.Hash(id + 137.1);
float period = max(FlickerPeriodSeconds, 3.0) * lerp(0.8, 1.3, phase);
float clock = GameTime / period + phase;
float cycle = floor(clock);
float seconds = frac(clock) * period;
float depth = lerp(0.25, 0.65, pattern.Hash(id + cycle + 57.4));
// One soft dropout per cycle; no rapid full-screen or high-frequency flashing.
float dropout = smoothstep(0.0, 0.45, seconds) * (1.0 - smoothstep(0.8, 1.5, seconds));
power *= 1.0 - isFlickering * depth * dropout;

float2 t = saturate((edge - rail * 0.5) / 0.35);
float2 raised = t * t * (3.0 - 2.0 * t);
float2 slope = 6.0 * t * (1.0 - t) * -sign(f - 0.5) / 0.35 * raised.yx;
float fade = 1.0 - smoothstep(0.15, 0.7, max(pixel.x, pixel.y));
// Irregular pores and short fibrous fissures, in centimeters and filtered at distance.
float pixelCm = max(pixel.x, pixel.y);
float poreFade = 1.0 - smoothstep(0.1, 0.55, pixelCm * 2.5);
float3 grain = pattern.Noise(p * 2.5 + 13.9);
float fibre = pattern.Noise(float2(p.x * 4.0 + p.y * 1.1, p.y * 1.7)).x;
float pores = smoothstep(0.68, 0.9, grain.x * 0.7 + fibre * 0.3) * poreFade;
float passive = mask * (1.0 - fixture);
float2 microSlope = grain.yz * 2.5 * 0.004 * poreFade * passive;
WorldNormal = normalize(SurfaceNormal - float3(slope * 0.07 * fade + microSlope, 0));
Roughness = lerp(0.48, 0.88, mask);
Roughness = lerp(Roughness, 0.48, lamp);
Emission = LightColor.rgb * clamp(LightStrength, 0.0, 4.0) * lamp * power;
float shade = (pattern.Hash(bay * count + tile + 9.2) - 0.5) * 0.025;
// The source is a 16x16 tile atlas with rails at half-cell offsets. Sample
// inset interiors only: our own world-space grid remains the sole rail layout.
float2 atlasCell = floor(float2(pattern.Hash(id + 8.1), pattern.Hash(id + 38.2)) * 16.0);
float2 uv = (atlasCell + 0.5 + 0.08 + f * 0.84) / 16.0;
float2 du = ddx(p) / size * (0.84 / 16.0);
float2 dv = ddy(p) / size * (0.84 / 16.0);
float3 scannedColor = Texture2DSampleGrad(AcousticColor, AcousticColorSampler, uv, du, dv).rgb;
float scannedRoughness = Texture2DSampleGrad(AcousticRoughness, AcousticRoughnessSampler, uv, du, dv).r;
float2 normalXY = Texture2DSampleGrad(AcousticNormal, AcousticNormalSampler, uv, du, dv).rg * 2.0 - 1.0;
float3 detail = float3(normalXY.x, -normalXY.y, 0) * passive * 0.35;
WorldNormal = normalize(WorldNormal + detail);
Roughness = lerp(Roughness, clamp(scannedRoughness, 0.65, 1.0), passive);
float3 acoustic = scannedColor * PanelColor.rgb * (1.0 + shade - pores * saturate(PoreContrast) * 0.25);
float3 base = lerp(RailColor.rgb, acoustic, mask);
// Off fixtures retain a visible diffuser, rather than turning into black holes.
return lerp(base, LightColor.rgb * 0.65, lamp);
""", animated=True, textures=textures)
unreal.log("LABY_ACOUSTIC_CEILING_SAVED")
