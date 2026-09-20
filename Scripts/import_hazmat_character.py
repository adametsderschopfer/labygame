"""Import Hazmat Suit 3 using UE's supported FBX and material editor APIs.

Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>, editor closed.
Original FBX and texture maps remain under ArtSource/Characters/HazmatSuit3.
"""
from pathlib import Path
import json
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE = ROOT / 'ArtSource/Characters/HazmatSuit3/Original/Hazmat_Suit'
DEST = '/Game/Characters/HazmatSuit3'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.MaterialEditingLibrary


def save(asset):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f'Cannot save {asset.get_path_name()}')


def import_texture(filename, kind):
    name = 'T_' + Path(filename).stem.replace('(andalpha)', '_Alpha')
    path = f'{DEST}/Textures/{name}'
    texture = unreal.load_asset(path)
    if texture is None:
        task = unreal.AssetImportTask()
        task.filename = str(SOURCE / 'textures' / filename)
        task.destination_path = f'{DEST}/Textures'
        task.destination_name = name
        task.automated = True
        TOOLS.import_asset_tasks([task])
        texture = unreal.load_asset(path)
    if texture is None:
        raise RuntimeError(f'Missing texture: {filename}')
    texture.set_editor_property('srgb', kind == 'color')
    if kind == 'normal':
        texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
        # Source maps were authored for Blender/OpenGL (+Y); Unreal expects -Y.
        texture.set_editor_property('flip_green_channel', True)
    elif kind == 'linear':
        texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)
    save(texture)
    return texture


def material(slot, prefix):
    name = 'M_' + slot
    mat = unreal.load_asset(f'{DEST}/Materials/{name}')
    if mat is None:
        mat = TOOLS.create_asset(name, f'{DEST}/Materials', unreal.Material, unreal.MaterialFactoryNew())
    LIB.delete_all_material_expressions(mat)
    mat.set_editor_property('two_sided', slot in ('H_SUIT', 'H_VISOR', 'H_VISOR2', 'H_BROW'))
    LIB.set_base_material_usage(mat, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH)
    maps = [('Basecolor', 'color', unreal.MaterialProperty.MP_BASE_COLOR),
            ('Normal', 'normal', unreal.MaterialProperty.MP_NORMAL),
            ('Roughness', 'linear', unreal.MaterialProperty.MP_ROUGHNESS),
            ('Metallic', 'linear', unreal.MaterialProperty.MP_METALLIC)]
    for index, (suffix, kind, prop) in enumerate(maps):
        candidates = list((SOURCE / 'textures').glob(f'{prefix}_{suffix}.*'))
        if suffix == 'Basecolor' and slot == 'H_FACE':
            candidates = [SOURCE / 'textures/H_Head_Baescolor.png']
        if suffix == 'Basecolor' and slot == 'H_BROW':
            candidates = [SOURCE / 'textures/H_Brow_Base(andalpha).png']
        if not candidates:
            continue
        texture = import_texture(candidates[0].name, kind)
        node = LIB.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -450, index * 220)
        node.texture = texture
        node.sampler_type = {'color': unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
                             'normal': unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
                             'linear': unreal.MaterialSamplerType.SAMPLERTYPE_MASKS}[kind]
        LIB.connect_material_property(node, 'RGB' if kind != 'linear' else 'R', prop)
        if slot == 'H_BROW' and suffix == 'Basecolor':
            mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
            LIB.connect_material_property(node, 'A', unreal.MaterialProperty.MP_OPACITY_MASK)
    if slot in ('H_VISOR', 'H_VISOR2'):
        mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
        mat.set_editor_property('translucency_lighting_mode', unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
        node = LIB.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -450, 900)
        node.texture = import_texture(f'{prefix}_Alpha.png', 'linear')
        node.sampler_type = unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
        LIB.connect_material_property(node, 'R', unreal.MaterialProperty.MP_OPACITY)
    if slot == 'H_Tube':
        color = LIB.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -450, 0)
        color.constant = unreal.LinearColor(0.012, 0.015, 0.018, 1)
        LIB.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
    LIB.layout_material_expressions(mat)
    errors = LIB.recompile_material(mat)
    if errors:
        raise RuntimeError(f'Material compilation failed for {name}:\n' + '\n'.join(errors))
    save(mat)
    return mat


def main():
    editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if editor is not None and editor.is_in_play_in_editor():
        raise RuntimeError('Stop Play before importing the character; no assets were changed')
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    mesh = unreal.load_asset(f'{DEST}/SK_HazmatSuit3')
    if mesh is None:
        options = unreal.FbxImportUI()
        options.automated_import_should_detect_type = False
        options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
        options.import_as_skeletal = True
        options.import_mesh = True
        options.import_animations = False
        options.import_materials = False
        options.import_textures = False
        options.create_physics_asset = False
        options.skeletal_mesh_import_data.set_editor_property('import_morph_targets', True)
        options.skeletal_mesh_import_data.set_editor_property('convert_scene', True)
        options.skeletal_mesh_import_data.set_editor_property('convert_scene_unit', True)
        task = unreal.AssetImportTask()
        task.filename = str(SOURCE / 'models/Hazmatsuit.fbx')
        task.destination_path = DEST
        task.destination_name = 'SK_HazmatSuit3'
        task.automated = True
        task.options = options
        task.factory = unreal.FbxFactory()
        TOOLS.import_asset_tasks([task])
        mesh = unreal.load_asset(f'{DEST}/SK_HazmatSuit3')
        if not isinstance(mesh, unreal.SkeletalMesh):
            raise RuntimeError(f'Skeletal import failed: {list(task.imported_object_paths)}')
    prefixes = {'H_HARDHAT': 'H_Hardhat', 'H_VISOR': 'H_Visor', 'H_Tube': 'H_Tube',
                'H_SHIRT': 'H_Shirt', 'H_GLOVE': 'H_Glove', 'H_EYE': 'H_Eye',
                'H_BROW': 'H_Brow', 'H_FACE': 'H_Head', 'H_VISOR2': 'H_Visor2',
                'H_VALVE': 'H_Valve', 'H_SUIT': 'H_Suit', 'H_MASK': 'H_Gasmask', 'H_BOOT': 'H_Boots'}
    slots = mesh.get_editor_property('materials')
    for index, slot in enumerate(slots):
        name = str(slot.get_editor_property('imported_material_slot_name'))
        if name not in prefixes:
            raise RuntimeError(f'Unexpected material slot: {name}')
        slot.set_editor_property('material_interface', material(name, prefixes[name]))
        # Unreal Array iteration returns struct copies: write the changed slot back.
        slots[index] = slot
    mesh.set_editor_property('materials', slots)
    save(mesh)
    save(mesh.get_editor_property('skeleton'))
    report = {'mesh': mesh.get_path_name(), 'bounds': str(mesh.get_bounds()),
              'materials': [str(s.material_slot_name) for s in slots],
              'skeleton': mesh.skeleton.get_path_name()}
    (ROOT / 'Saved/Hazmat/import-report.json').write_text(json.dumps(report, indent=2))
    unreal.log('HAZMAT_IMPORT_COMPLETE ' + json.dumps(report))


if __name__ == '__main__':
    main()


