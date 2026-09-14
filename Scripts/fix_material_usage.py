import unreal

material = unreal.load_asset("/Game/Materials/M_MazePlain")
if material is None:
    raise RuntimeError("Maze material missing")
material.set_editor_property("used_with_instanced_static_meshes", True)
unreal.MaterialEditingLibrary.recompile_material(material)
if not unreal.EditorAssetLibrary.save_loaded_asset(material):
    raise RuntimeError("Could not save material")
assert material.get_editor_property("used_with_instanced_static_meshes")
unreal.log("LABY_MATERIAL_USAGE_FIXED")
