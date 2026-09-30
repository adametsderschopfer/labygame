"""Rebuild the generated card reader screen graph at its stable package path.

Safe to rerun; replaces this generated graph only. Run with Unreal Python.
"""
from pathlib import Path
import unreal

if Path(unreal.Paths.project_dir()).resolve() != Path(__file__).resolve().parent.parent:
    raise RuntimeError("Wrong project: expected laby")
# LevelEditorSubsystem requires Slate and cannot be created in a commandlet.
if "-run=" not in unreal.SystemLibrary.get_command_line().lower():
    editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if editor and editor.is_in_play_in_editor():
        raise RuntimeError("Stop Play before rebuilding the card reader material")

folder = "/Game/Doors/CardAccess/Materials"
path = f"{folder}/M_CardReaderScreen"
lib = unreal.MaterialEditingLibrary
material = unreal.load_asset(path)
if material is None:
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_CardReaderScreen", folder, unreal.Material, unreal.MaterialFactoryNew())
if material is None:
    raise RuntimeError(f"Cannot create {path}")
material.set_editor_property("used_with_instanced_static_meshes", True)
material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
while lib.get_num_material_expressions(material):
    remaining = lib.get_num_material_expressions(material)
    lib.delete_all_material_expressions(material)
    if lib.get_num_material_expressions(material) >= remaining:
        raise RuntimeError("Could not clear the generated graph")

def node(cls, **properties):
    expression = lib.create_material_expression(material, cls)
    for name, value in properties.items():
        expression.set_editor_property(name, value)
    return expression

red = node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(2.5, .025, .01))
green = node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(.015, 2.5, .08))
state = node(unreal.MaterialExpressionPerInstanceCustomData, data_index=0, const_default_value=0.0)
interpolator = node(unreal.MaterialExpressionVertexInterpolator)
blend = node(unreal.MaterialExpressionLinearInterpolate)
for source, target, pin in [(red, blend, "A"), (green, blend, "B"),
                            (state, interpolator, "VS"), (interpolator, blend, "Alpha")]:
    if not lib.connect_material_expressions(source, "", target, pin):
        raise RuntimeError(f"Could not connect {pin}")
if not lib.connect_material_property(blend, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
    raise RuntimeError("Could not connect emissive output")
lib.layout_material_expressions(material)
errors = lib.recompile_material(material)
if errors:
    raise RuntimeError(f"Material compilation failed: {errors}")
if not unreal.EditorAssetLibrary.save_loaded_asset(material):
    raise RuntimeError(f"Could not save {path}")
expressions = lib.get_num_material_expressions(material)
if expressions != 5:
    raise RuntimeError(f"Unexpected expression count: {expressions}")
unreal.log(f"Card access screen saved: {path}; {expressions} expressions; ISM custom data index 0")
