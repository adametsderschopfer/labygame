"""Author softly marbled green sheet flooring in the existing active material."""
from pathlib import Path
import runpy
import unreal

create = runpy.run_path(str(Path(__file__).with_name("material_builder.py")))["create"]
# Keep the preceding appearance as a separate parent before editing the live asset.
previous = "/Game/Materials/Laboratory/M_LabVinylSatin"
backup = "/Game/Materials/Laboratory/M_LabVinylSatinBeforeMarbling"
if unreal.EditorAssetLibrary.does_asset_exist(previous) and not unreal.EditorAssetLibrary.does_asset_exist(backup):
    saved = unreal.EditorAssetLibrary.duplicate_asset(previous, backup)
    if saved is None or not unreal.EditorAssetLibrary.save_loaded_asset(saved):
        raise RuntimeError("Cannot preserve previous floor material")
create("LabVinylSatin", {
    "FloorRoughness": 0.43,
    "FleckContrast": 0.16,
    "MarbleContrast": 0.32,
    "PatternScaleCm": 8.0,
    "SheenVariation": 0.055,
    "MicroReliefCm": 0.0004,
    "WearStrength": 0.055,
    "BroadPatternScaleCm": 45.0,
}, {"FloorColor": (0.20, 0.265, 0.235)}, r"""
struct VinylSurface
{
    float Hash(float2 p)
    {
        float3 q = frac(float3(p.x, p.y, p.x) * 0.1031);
        q += dot(q, q.yzx + 33.33);
        return frac((q.x + q.y) * q.z);
    }
    // Continuous value noise: value and analytic derivatives, no cell-center dots.
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
    float Fade(float pixelCm, float frequency)
    {
        return 1.0 - smoothstep(0.15, 0.75, pixelCm * frequency);
    }
};
VinylSurface surface;
float2 p = WorldPos.xy;
float pixelCm = max(length(ddx(p)), length(ddy(p)));
float2 rotated = float2(p.x * 0.8 - p.y * 0.6, p.x * 0.6 + p.y * 0.8);
float scale = max(PatternScaleCm, 2.0);
float2 q = rotated / scale;
// Warped pigment, not periodic sine veins: sheet linoleum has blended streaks.
float2 warp = float2(surface.Noise(q * 0.65 + 7.3).x,
                     surface.Noise(q * 0.65 + 61.8).x) - 0.5;
float2 flow = q + warp * 2.8;
float marble = surface.Noise(flow * float2(0.85, 3.5) + 19.1).x - 0.5;
float streak = surface.Noise(flow * float2(2.3, 12.0) + 47.8).x - 0.5;
// Filter each actual warped frequency; a fixed worst-case multiplier erased
// the entire pigment field in a visible band at grazing view angles.
float2 marbleUV = flow * float2(0.85, 3.5);
float2 streakUV = flow * float2(2.3, 12.0);
float marblePixel = max(length(ddx(marbleUV)), length(ddy(marbleUV)));
float streakPixel = max(length(ddx(streakUV)), length(ddy(streakUV)));
float marbleFade = 1.0 - smoothstep(0.4, 1.3, marblePixel);
float streakFade = 1.0 - smoothstep(0.4, 1.3, streakPixel);
// A separate broad pigment layer remains after fine inclusions become subpixel.
float broadScale = max(BroadPatternScaleCm, 20.0);
float2 broadUV = rotated / broadScale;
float broad = surface.Noise(broadUV + 293.7).x - 0.5;
float broadPixel = max(length(ddx(broadUV)), length(ddy(broadUV)));
float broadFade = 1.0 - smoothstep(0.4, 1.3, broadPixel);
float pigment = surface.Noise(rotated * float2(1.6, 4.7) + warp * 0.7).x;
float fleck = (smoothstep(0.48, 0.84, pigment) - 0.36) * surface.Fade(pixelCm, 5.5);
// Pigment does not displace the surface. Only microscopic finish affects normals.
float3 micro = surface.Noise(p * 9.0 + 137.4);
float microFade = surface.Fade(pixelCm, 9.0);
float2 slope = micro.yz * 9.0 * clamp(MicroReliefCm, 0.0, 0.001) * microFade;
float3 N = normalize(SurfaceNormal);
float3 gradient = float3(slope, 0);
gradient -= N * dot(gradient, N);
WorldNormal = normalize(N - gradient);
// Broad, subtle polishing variation changes the highlight more than the color.
float polish = surface.Noise(float2(rotated.x * 0.025, rotated.y * 0.12) + 41.3).x - 0.5;
float sheen = polish * clamp(SheenVariation, 0.0, 0.15);
// Sparse finite scuffs: primarily visible in reflected light, never deep grooves.
float2 wearCell = floor(p / 160.0);
float wear = 0.0;
for (int wy = -1; wy <= 1; ++wy)
for (int wx = -1; wx <= 1; ++wx)
{
    float2 cell = wearCell + float2(wx, wy);
    float seed = surface.Hash(cell + 211.0);
    float angle = seed * 6.283185;
    float2 dir = float2(cos(angle), sin(angle));
    float2 center = (cell + float2(surface.Hash(cell + 33.0), surface.Hash(cell + 91.0))) * 160.0;
    float2 delta = p - center;
    float along = dot(delta, dir);
    float across = abs(dot(delta, float2(-dir.y, dir.x)));
    float width = lerp(0.12, 0.45, seed);
    float lengthCm = lerp(7.0, 24.0, seed);
    float scuffMask = (1.0 - smoothstep(width, width + max(pixelCm, 0.05), across)) *
                 (1.0 - smoothstep(lengthCm * 0.6, lengthCm, abs(along)));
    wear += scuffMask * step(0.78, seed) * surface.Fade(pixelCm, 0.35);
}
wear *= saturate(SurfaceNormal.z);
Roughness = clamp(FloorRoughness + sheen + streak * streakFade * 0.018 +
                  wear * clamp(WearStrength, 0.0, 0.12), 0.35, 0.75);
Emission = 0;
float blend = broad * broadFade * 0.75 + marble * marbleFade * 1.1 + streak * streakFade * 0.45;
float color = blend * saturate(MarbleContrast) + fleck * saturate(FleckContrast);
// Slight hue variation keeps inclusions from reading as a grayscale noise overlay.
float3 tint = float3(0.012, 0.006, -0.006) * blend;
return saturate(FloorColor.rgb * (1.0 + color) + tint);
""", instanced=True)
unreal.log("LABY_SATIN_VINYL_SAVED")
