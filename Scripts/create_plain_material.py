import unreal

path = "/Game/Materials/M_MazePlain"
material = unreal.load_asset(path)
if material is None:
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_MazePlain", "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
if material is None:
    raise RuntimeError("Cannot create maze material")
material.set_editor_property("used_with_instanced_static_meshes", True)
unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
color = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant3Vector)
color.set_editor_property("constant", unreal.LinearColor(0.55, 0.55, 0.55, 1.0))
unreal.MaterialEditingLibrary.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
roughness = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant)
roughness.set_editor_property("r", 0.9)
unreal.MaterialEditingLibrary.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
unreal.MaterialEditingLibrary.recompile_material(material)
if not unreal.EditorAssetLibrary.save_loaded_asset(material):
    raise RuntimeError("Cannot save maze material")
unreal.log("LABY_PLAIN_MATERIAL_SAVED")
