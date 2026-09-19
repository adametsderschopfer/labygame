"""Author a satin vinyl floor variant; preserves the original for comparison."""
from pathlib import Path
import runpy
import unreal

create = runpy.run_path(str(Path(__file__).with_name("material_builder.py")))["create"]
create("LabVinylSatin", {
    "FloorRoughness": 0.36,
    "FleckContrast": 0.10,
    "ColorVariation": 0.025,
    "SheenVariation": 0.09,
    "MicroReliefCm": 0.001,
    "SurfaceWavinessCm": 0.002,
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
float3 broad = surface.Noise(p * 0.035);
float3 wave = surface.Noise(p * 0.45 + 31.7);
// Rotate the fine pigment field to avoid aligning both scales into a visible grid.
float2 rotated = float2(p.x * 0.8 - p.y * 0.6, p.x * 0.6 + p.y * 0.8);
float3 pigment = surface.Noise(rotated * 2.0 + wave.x * 0.65);
float3 fine = surface.Noise(rotated * 7.0 + 79.1);
float coarseFade = surface.Fade(pixelCm, 2.0);
float fineFade = surface.Fade(pixelCm, 7.0);
// Irregular inclusions are buried pigment, not raised gravel.
float coarse = (smoothstep(0.52, 0.8, pigment.x) - (1.0 - smoothstep(0.2, 0.48, pigment.x))) * coarseFade;
float small = (fine.x - 0.5) * fineFade;
float3 micro = surface.Noise(p * 9.0 + 137.4);
float microFade = surface.Fade(pixelCm, 9.0);
float waveFade = surface.Fade(pixelCm, 0.45);
float2 slope = micro.yz * 9.0 * clamp(MicroReliefCm, 0.0, 0.005) * microFade +
               wave.yz * 0.45 * clamp(SurfaceWavinessCm, 0.0, 0.02) * waveFade;
float3 N = normalize(SurfaceNormal);
float3 gradient = float3(slope, 0);
gradient -= N * dot(gradient, N);
WorldNormal = normalize(N - gradient);
// Broad, subtle polishing variation changes the highlight more than the color.
float polish = surface.Noise(float2(rotated.x * 0.025, rotated.y * 0.12) + 41.3).x - 0.5;
float sheen = ((broad.x - 0.5) * 0.65 + polish * 0.7) * clamp(SheenVariation, 0.0, 0.3);
Roughness = clamp(FloorRoughness + sheen + small * 0.025, 0.18, 0.8);
Emission = 0;
float color = (coarse * 0.65 + small * 0.35) * saturate(FleckContrast) +
              (broad.x - 0.5) * clamp(ColorVariation, 0.0, 0.15);
return saturate(FloorColor.rgb * (1.0 + color));
""", instanced=True)
unreal.log("LABY_SATIN_VINYL_SAVED")
