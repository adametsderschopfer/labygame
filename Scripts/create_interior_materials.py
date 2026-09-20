"""Small service fittings: native default-lit materials shared by batched geometry."""
from pathlib import Path
import runpy
import unreal

create = runpy.run_path(str(Path(__file__).with_name("material_builder.py")))["create"]
for name, color, roughness, metal in [
    ("LabTrimMetal", (0.46, 0.49, 0.50), 0.34, 0.85),
    ("LabServiceIvory", (0.70, 0.72, 0.69), 0.42, 0.0),
    ("LabServiceRecess", (0.018, 0.022, 0.023), 0.72, 0.0),
]:
    create(name, {"FinishRoughness": roughness}, {"FinishColor": color}, """
        WorldNormal = normalize(SurfaceNormal);
        Roughness = FinishRoughness;
        Emission = 0;
        return FinishColor.rgb;
    """)
    material = unreal.load_asset(f"/Game/Materials/Laboratory/M_{name}")
    metallic = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionScalarParameter)
    metallic.set_editor_property("parameter_name", "Metallic")
    metallic.set_editor_property("default_value", metal)
    if not unreal.MaterialEditingLibrary.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC):
        raise RuntimeError("Cannot connect metallic")
    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    errors = unreal.MaterialEditingLibrary.recompile_material(material)
    if errors:
        raise RuntimeError(f"Material compilation failed for {name}:\n" + "\n".join(errors))
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError(f"Cannot save {name}")
unreal.log("LABY_INTERIOR_MATERIALS_SAVED")
