"""Shared Unreal material authoring helpers; importing creates no assets."""
import unreal

FOLDER = "/Game/Materials/Laboratory"
LIB = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

def properties(obj, **values):
    for name, value in values.items():
        obj.set_editor_property(name, value)
    return obj


def create(name, scalars, colors, code, instanced=False, animated=False, textures=None):
    editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if editor is not None and editor.is_in_play_in_editor():
        raise RuntimeError("Stop Play before rebuilding materials; no material was changed")
    path = f"{FOLDER}/M_{name}"
    material = unreal.load_asset(path)
    if material is None:
        material = TOOLS.create_asset(f"M_{name}", FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        raise RuntimeError(f"Cannot create {path}")
    properties(material, tangent_space_normal=False, used_with_instanced_static_meshes=instanced)
    # UE 5.8 removes from the same array it iterates, which can skip expressions.
    # Drain it fully so stale parameter defaults cannot shadow the new graph.
    remaining = LIB.get_num_material_expressions(material)
    while remaining:
        LIB.delete_all_material_expressions(material)
        current = LIB.get_num_material_expressions(material)
        if current >= remaining:
            raise RuntimeError(f"Cannot clear all expressions from {path}")
        remaining = current

    def node(cls, **values):
        return properties(LIB.create_material_expression(material, cls), **values)

    inputs = {
        "WorldPos": node(unreal.MaterialExpressionWorldPosition),
        "SurfaceNormal": node(unreal.MaterialExpressionVertexNormalWS),
    }
    if animated:
        inputs["GameTime"] = node(unreal.MaterialExpressionTime, ignore_pause=False)
    for key, texture in (textures or {}).items():
        normal = texture.get_editor_property("compression_settings") == unreal.TextureCompressionSettings.TC_NORMALMAP
        sampler = (unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else
                   unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if texture.get_editor_property("srgb") else
                   unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        inputs[key] = node(unreal.MaterialExpressionTextureObjectParameter,
                           parameter_name=key, texture=texture, sampler_type=sampler)
    for key, value in scalars.items():
        inputs[key] = node(unreal.MaterialExpressionScalarParameter, parameter_name=key,
                           default_value=value, group=name)
    for key, value in colors.items():
        inputs[key] = node(unreal.MaterialExpressionVectorParameter, parameter_name=key,
                           default_value=unreal.LinearColor(*value, 1.0), group=name)
    shader = node(unreal.MaterialExpressionCustom,
                  description=f"World-space {name}; dimensions in cm",
                  output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                  inputs=[properties(unreal.CustomInput(), input_name=key) for key in inputs],
                  additional_outputs=[
                      properties(unreal.CustomOutput(), output_name=key, output_type=kind)
                      for key, kind in [
                          ("Roughness", unreal.CustomMaterialOutputType.CMOT_FLOAT1),
                          ("WorldNormal", unreal.CustomMaterialOutputType.CMOT_FLOAT3),
                          ("Emission", unreal.CustomMaterialOutputType.CMOT_FLOAT3),
                      ]], code=code)
    for key, source in inputs.items():
        if not LIB.connect_material_expressions(source, "", shader, key):
            raise RuntimeError(f"Cannot connect {key}")
    for pin, prop in [("", unreal.MaterialProperty.MP_BASE_COLOR),
                      ("Roughness", unreal.MaterialProperty.MP_ROUGHNESS),
                      ("WorldNormal", unreal.MaterialProperty.MP_NORMAL),
                      ("Emission", unreal.MaterialProperty.MP_EMISSIVE_COLOR)]:
        if not LIB.connect_material_property(shader, pin, prop):
            raise RuntimeError(f"Cannot connect {pin}")
    LIB.layout_material_expressions(material)
    errors = LIB.recompile_material(material)
    if errors:
        raise RuntimeError(f"Material compilation failed for {path}:\n" + "\n".join(errors))
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError(f"Cannot save {path}; execute inside the editor if the asset is open")
    instance = unreal.load_asset(f"{FOLDER}/MI_{name}")
    if instance is None:
        instance = TOOLS.create_asset(f"MI_{name}", FOLDER, unreal.MaterialInstanceConstant,
                                     unreal.MaterialInstanceConstantFactoryNew())
    if instance is None:
        raise RuntimeError(f"Cannot create MI_{name}")
    LIB.set_material_instance_parent(instance, material)
    LIB.update_material_instance(instance)
    if not unreal.EditorAssetLibrary.save_loaded_asset(instance):
        raise RuntimeError(f"Cannot save MI_{name}")



