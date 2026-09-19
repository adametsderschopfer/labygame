"""Create the procedural wall material (Unreal Editor Python or Python commandlet).

No textures or mesh UVs required. Re-running rebuilds the parent and preserves
artist overrides on MI_MazeSubway. All distances are Unreal centimeters.
"""
import unreal

LIB = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
FOLDER = globals().get("MATERIAL_FOLDER", "/Game/Materials")
NAME = globals().get("MATERIAL_NAME", "MazeSubway")
material = unreal.load_asset(f"{FOLDER}/M_{NAME}")
if material is None:
    material = TOOLS.create_asset(f"M_{NAME}", FOLDER, unreal.Material, unreal.MaterialFactoryNew())
if material is None:
    raise RuntimeError("Cannot create subway material")
LIB.delete_all_material_expressions(material)
material.set_editor_property("tangent_space_normal", False)


def node(cls, **properties):
    result = LIB.create_material_expression(material, cls)
    for key, value in properties.items():
        result.set_editor_property(key, value)
    return result


def connect(source, target, pin):
    if not LIB.connect_material_expressions(source, "", target, pin):
        raise RuntimeError(f"Cannot connect {pin}")


def custom_pin(cls, **properties):
    result = cls()
    for key, value in properties.items():
        result.set_editor_property(key, value)
    return result


inputs = {
    "WorldPos": node(unreal.MaterialExpressionWorldPosition),
    "SurfaceNormal": node(unreal.MaterialExpressionVertexNormalWS),
}
for name, value in {
    "TileWidthCm": 30.0,
    "TileHeightCm": 15.0,
    "GroutWidthCm": 0.26,
    "BevelWidthCm": 0.24,
    "ReliefCm": 0.12,
    "CrownHeightCm": 0.07,
    "GlazeWavinessCm": 0.003,
    "GroutGrainCm": 0.006,
    "TileRoughness": 0.19,
    "GroutRoughness": 0.85,
    "ShadeVariation": 0.04,
}.items():
    inputs[name] = node(unreal.MaterialExpressionScalarParameter,
                        parameter_name=name, default_value=value, group="Subway Tiles")
for name, value in {
    "TileColor": (0.78, 0.77, 0.73, 1.0),
    "TileOrigin": (0.0, 0.0, 320.0, 1.0),
    "GroutColor": (0.035, 0.038, 0.04, 1.0),
}.items():
    inputs[name] = node(unreal.MaterialExpressionVectorParameter,
                        parameter_name=name, default_value=unreal.LinearColor(*value),
                        group="Subway Tiles")

shader = node(
    unreal.MaterialExpressionCustom,
    description="World-space staggered ceramic tiles; dimensions in cm",
    output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,
    inputs=[custom_pin(unreal.CustomInput, input_name=name) for name in inputs],
    additional_outputs=[
        custom_pin(unreal.CustomOutput, output_name="Roughness", output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1),
        custom_pin(unreal.CustomOutput, output_name="WorldNormal", output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3),
        custom_pin(unreal.CustomOutput, output_name="Occlusion", output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1),
    ],
    code=r"""
struct TileFilter
{
    float Hash(float2 p)
    {
        float3 q = frac(float3(p.x, p.y, p.x) * 0.1031);
        q += dot(q, q.yzx + 33.33);
        return frac((q.x + q.y) * q.z);
    }
    // Smooth value noise and its exact UV derivatives (value, dU, dV).
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
    float Integral(float t, float period, float width)
    {
        return floor(t / period) * width + min(frac(t / period) * period, width);
    }
    float Coverage(float t, float period, float width, float footprint)
    {
        // Reduce coordinates first to preserve precision far from the origin.
        t = frac(t / period) * period;
        return saturate((Integral(t + footprint * 0.5, period, width) -
                         Integral(t - footprint * 0.5, period, width)) / footprint);
    }
};
TileFilter filter;
float3 N = normalize(SurfaceNormal);
float3 axis = abs(N);
float3 localPosition = WorldPos - TileOrigin.rgb;
float2 p = axis.z > max(axis.x, axis.y) ? localPosition.xy :
           float2(axis.x > axis.y ? localPosition.y : localPosition.x, localPosition.z);
float2 size = max(float2(TileWidthCm, TileHeightCm), 1.0);
float grout = clamp(GroutWidthCm, 0.01, min(size.x, size.y) * 0.25);
float row = floor(p.y / size.y);
float2 grid = p / size + float2(frac(row * 0.5), 0);
float2 id = floor(grid);
float2 edge = (0.5 - abs(frac(grid) - 0.5)) * size;
// Derivatives come from continuous coordinates, never from frac at a seam.
float2 footprint = max(abs(ddx(p)) + abs(ddy(p)), 0.001);
// Integrate both alternating rows over the pixel footprint. This preserves
// grout coverage at distance instead of widening dark lines as they blur.
float evenX = filter.Coverage(p.x - grout * 0.5, size.x, size.x - grout, footprint.x);
float oddX = filter.Coverage(p.x + size.x * 0.5 - grout * 0.5, size.x, size.x - grout, footprint.x);
float evenY = filter.Coverage(p.y - grout * 0.5, size.y * 2, size.y - grout, footprint.y);
float oddY = filter.Coverage(p.y - size.y - grout * 0.5, size.y * 2, size.y - grout, footprint.y);
float resolved = 1.0 - smoothstep(0.2, 0.65, max(footprint.x / size.x, footprint.y / size.y));
float mask = saturate(evenX * evenY + oddX * oddY);
float random = filter.Hash(id) - 0.5;
float variation = random * saturate(ShadeVariation) * resolved;
float bevel = clamp(BevelWidthCm, 0.01, min(size.x, size.y) * 0.2);
float2 t = saturate((edge - grout * 0.5) / bevel);
float2 raised = t * t * (3.0 - 2.0 * t);
float2 edgeDirection = -sign(frac(grid) - 0.5);
float2 raisedGradient = 6.0 * t * (1.0 - t) * edgeDirection / bevel;
float face = raised.x * raised.y;
float2 faceGradient = raisedGradient * raised.yx;
float2 s = frac(grid) * 2.0 - 1.0;
float2 dome = 1.0 - s * s;
float crown = dome.x * dome.y;
float2 crownGradient = -4.0 * s / size * dome.yx;

// Small glaze ripples and grout aggregate have separate physical scales.
// Filter their amplitudes before they become subpixel to suppress shimmer.
float pixelCm = max(footprint.x, footprint.y);
float bevelFade = 1.0 - smoothstep(bevel * 0.5, bevel * 2.0, pixelCm);
float glazeFade = 1.0 - smoothstep(0.15, 0.7, pixelCm * 1.7);
float grainFade = 1.0 - smoothstep(0.15, 0.7, pixelCm * 18.0);
float3 glaze = filter.Noise(p * 1.7 + random * 11.0);
float3 grain = filter.Noise(p * 18.0);
float3 groutMottle = filter.Noise(p * 2.0);
float mottleFade = 1.0 - smoothstep(0.15, 0.7, pixelCm * 2.0);
float glazeAmplitude = clamp(GlazeWavinessCm, 0.0, 0.02) * glazeFade;
float grainAmplitude = clamp(GroutGrainCm, 0.0, 0.02) * grainFade;
float crownHeight = clamp(CrownHeightCm, 0.0, 0.15);
float surfaceHeight = clamp(ReliefCm, 0.0, 0.3) + crown * crownHeight +
                      (glaze.x - 0.5) * glazeAmplitude;
float groutHeight = (grain.x - 0.5) * grainAmplitude;
float2 gradient = faceGradient * (surfaceHeight - groutHeight) * bevelFade +
                  face * (crownGradient * crownHeight + glaze.yz * 1.7 * glazeAmplitude) +
                  (1.0 - face) * grain.yz * 18.0 * grainAmplitude;
// Analytic slopes retain the rounded profile even at subpixel bevel widths;
// no screen-space height differences across adjacent tiles, UVs or tangents.
float3 U = axis.z > max(axis.x, axis.y) ? float3(1, 0, 0) :
           (axis.x > axis.y ? float3(0, 1, 0) : float3(1, 0, 0));
float3 V = axis.z > max(axis.x, axis.y) ? float3(0, 1, 0) : float3(0, 0, 1);
WorldNormal = normalize(N - (U * gradient.x + V * gradient.y) * resolved);
float glazeRoughness = TileRoughness + variation * 0.35 + (glaze.x - 0.5) * 0.035 * glazeFade;
float groutRoughness = GroutRoughness + (grain.x - 0.5) * 0.12 * grainFade;
Roughness = clamp(lerp(groutRoughness, glazeRoughness, mask), 0.06, 1.0);
Occlusion = lerp(1.0, lerp(0.8, 1.0, face), resolved);
float3 groutColor = GroutColor.rgb * (1.0 + (groutMottle.x - 0.5) * 0.25 * mottleFade);
return lerp(saturate(groutColor), saturate(TileColor.rgb * (1.0 + variation)), mask);
""",
)
for name, source in inputs.items():
    connect(source, shader, name)
for pin, prop in [
    ("", unreal.MaterialProperty.MP_BASE_COLOR),
    ("Roughness", unreal.MaterialProperty.MP_ROUGHNESS),
    ("WorldNormal", unreal.MaterialProperty.MP_NORMAL),
    ("Occlusion", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION),
]:
    if not LIB.connect_material_property(shader, pin, prop):
        raise RuntimeError(f"Cannot connect material output {pin}")
LIB.layout_material_expressions(material)
LIB.recompile_material(material)
if not unreal.EditorAssetLibrary.save_loaded_asset(material):
    raise RuntimeError("Cannot save subway material")

instance = unreal.load_asset(f"{FOLDER}/MI_{NAME}")
if instance is None:
    instance = TOOLS.create_asset(f"MI_{NAME}", FOLDER, unreal.MaterialInstanceConstant,
                                  unreal.MaterialInstanceConstantFactoryNew())
if instance is None:
    raise RuntimeError("Cannot create subway material instance")
LIB.set_material_instance_parent(instance, material)
LIB.update_material_instance(instance)
if not unreal.EditorAssetLibrary.save_loaded_asset(instance):
    raise RuntimeError("Cannot save subway material instance")
unreal.log("LABY_SUBWAY_MATERIAL_SAVED")
