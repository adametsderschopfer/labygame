"""Import licensed socket PBR maps and Scopia's smoke detector with native UE importers."""
from pathlib import Path
import math
import unreal

editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if editor is not None and editor.is_in_play_in_editor():
    raise RuntimeError('Stop Play before importing fixtures; no assets were changed')

ROOT = Path(unreal.Paths.project_dir()).resolve()
DEST = '/Game/Fixtures'
LIB = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

def save(asset):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f'Cannot save {asset.get_path_name()} (stop Play before authoring)')

def texture(suffix, normal=False, linear=False):
    name = 'T_Socket_' + suffix
    asset = unreal.load_asset(f'{DEST}/{name}')
    if asset is None:
        ext = 'png' if normal else 'jpg'
        task = unreal.AssetImportTask()
        task.filename = str(ROOT / 'ArtSource/Fixtures/TextureCan0023' / f'others_0023_{suffix}_1k.{ext}')
        task.destination_path = DEST
        task.destination_name = name
        task.automated = True
        TOOLS.import_asset_tasks([task])
        asset = unreal.load_asset(f'{DEST}/{name}')
    if asset is None:
        raise RuntimeError(f'Import failed: {name}')
    asset.set_editor_property('srgb', not (normal or linear))
    if normal:
        asset.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
    save(asset)
    return asset

def material(name, color=None):
    mat = unreal.load_asset(f'{DEST}/{name}')
    if mat is None:
        mat = TOOLS.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    LIB.delete_all_material_expressions(mat)
    mat.set_editor_property('used_with_instanced_static_meshes', True)
    if color:
        c = LIB.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector)
        c.set_editor_property('constant', unreal.LinearColor(*color, 1))
        LIB.connect_material_property(c, '', unreal.MaterialProperty.MP_BASE_COLOR)
        rough = LIB.create_material_expression(mat, unreal.MaterialExpressionConstant)
        rough.set_editor_property('r', 0.42)
        LIB.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    return mat

def finish(mat):
    errors = LIB.recompile_material(mat)
    if errors:
        raise RuntimeError('\n'.join(errors))
    LIB.layout_material_expressions(mat)
    save(mat)

socket = material('M_SocketPBR')
uv = LIB.create_material_expression(socket, unreal.MaterialExpressionTextureCoordinate)
for suffix, prop, normal, linear in [
    ('color', unreal.MaterialProperty.MP_BASE_COLOR, False, False),
    ('normal_directx', unreal.MaterialProperty.MP_NORMAL, True, True),
    ('roughness', unreal.MaterialProperty.MP_ROUGHNESS, False, True),
    ('metallic', unreal.MaterialProperty.MP_METALLIC, False, True),
]:
    sample = LIB.create_material_expression(socket, unreal.MaterialExpressionTextureSample)
    sample.set_editor_property('texture', texture(suffix, normal, linear))
    sample.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else
                              unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR if linear else
                              unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    LIB.connect_material_expressions(uv, '', sample, 'Coordinates')
    LIB.connect_material_property(sample, 'RGB' if normal or not linear else 'R', prop)
finish(socket)
plastic = material('M_FixturePlastic', (0.65, 0.67, 0.64))
dark = material('M_FixtureRecess', (0.012, 0.014, 0.014))
red = material('M_FixtureIndicator', (0.55, 0.012, 0.008))
for mat in [plastic, dark, red]:
    finish(mat)

prepared = ROOT / 'ArtSource/Fixtures/Prepared'
prepared.mkdir(parents=True, exist_ok=True)
# Thin plate with explicit UVs on its front. Crop the source through UVs only.
faces = [
    ('SocketFace', [(1.2,-7.3,-4.3),(1.2,7.3,-4.3),(1.2,7.3,4.3),(1.2,-7.3,4.3)]),
    ('Plastic', [(0,-7.3,4.3),(0,7.3,4.3),(0,7.3,-4.3),(0,-7.3,-4.3)]),
    ('Plastic', [(0,7.3,-4.3),(0,7.3,4.3),(1.2,7.3,4.3),(1.2,7.3,-4.3)]),
    ('Plastic', [(0,-7.3,4.3),(0,-7.3,-4.3),(1.2,-7.3,-4.3),(1.2,-7.3,4.3)]),
    ('Plastic', [(0,7.3,4.3),(0,-7.3,4.3),(1.2,-7.3,4.3),(1.2,7.3,4.3)]),
    ('Plastic', [(0,-7.3,-4.3),(0,7.3,-4.3),(1.2,7.3,-4.3),(1.2,-7.3,-4.3)]),
]
out = ['mtllib socket.mtl', 'o Socket']
for i, (name, verts) in enumerate(faces):
    out.extend('v %f %f %f' % v for v in verts)
    out.extend('vt %f %f' % uv for uv in [(0.13,0.28),(0.87,0.28),(0.87,0.72),(0.13,0.72)])
    out.append('usemtl ' + name)
    base = i * 4 + 1
    out.append('f ' + ' '.join(f'{v}/{v}' for v in range(base, base + 4)))
(prepared / 'socket.obj').write_text('\n'.join(out))
(prepared / 'socket.mtl').write_text('newmtl SocketFace\nKd 0.8 0.8 0.8\nnewmtl Plastic\nKd 0.65 0.67 0.64\n')

# Source XY disk faces +Z; rotate 180 around X so its face points down.
src = ROOT / 'ArtSource/Fixtures/ScopiaSmokeDetector'
source_lines = (src / 'smoke-detector.obj').read_text().splitlines()
# This solid-color model has normals but no UVs (v//vn faces). UE's OBJ
# translator requires valid UV indices even for materials without textures.
needs_uv = not any(line.startswith('vt ') for line in source_lines)
out = ['vt 0.5 0.5'] if needs_uv else []
for l in source_lines:
    if l.startswith('v '):
        x,y,z = map(float, l.split()[1:])
        l = f'v {x*1.2} {-(y-2.693373)*1.2} {-(z+0.803288)*1.2}'
    elif l.startswith('vn '):
        x,y,z = map(float, l.split()[1:])
        l = f'vn {x} {-y} {-z}'
    elif l.startswith('f ') and needs_uv:
        corners = []
        for corner in l.split()[1:]:
            indices = corner.split('/')
            if len(indices) == 1:
                corner = f'{indices[0]}/1'
            elif len(indices) == 3 and not indices[1]:
                corner = f'{indices[0]}/1/{indices[2]}'
            else:
                raise RuntimeError(f'OBJ face references UVs but has no texture coordinates: {l}')
            corners.append(corner)
        l = 'f ' + ' '.join(corners)
    out.append(l)
(prepared / 'smoke-detector.obj').write_text('\n'.join(out))
(prepared / 'smoke-detector.mtl').write_text((src / 'smoke-detector.mtl').read_text())

for filename, name, mapping in [
    ('socket.obj', 'SM_LabSocket', {'SocketFace': socket, 'Plastic': plastic}),
    ('smoke-detector.obj', 'SM_LabSmokeDetector', {'whiteplastic': plastic, 'hole': dark, 'redbulb': red}),
]:
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_materials = False
    options.import_textures = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    options.static_mesh_import_data.combine_meshes = True
    options.static_mesh_import_data.auto_generate_collision = False
    options.static_mesh_import_data.convert_scene = False
    options.static_mesh_import_data.convert_scene_unit = False
    task = unreal.AssetImportTask()
    task.filename = str(prepared / filename)
    task.destination_path = DEST
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.options = options
    TOOLS.import_asset_tasks([task])
    mesh = unreal.load_asset(f'{DEST}/{name}')
    if mesh is None:
        raise RuntimeError(f'Import failed: {name}; imported {task.imported_object_paths}')
    for i, slot in enumerate(mesh.get_editor_property('static_materials')):
        key = str(slot.get_editor_property('imported_material_slot_name'))
        if key not in mapping:
            raise RuntimeError(f'Unexpected material slot {key}')
        mesh.set_material(i, mapping[key])
    save(mesh)
    print(name, mesh.get_bounding_box(), 'slots', len(mesh.get_editor_property('static_materials')))
unreal.log('LABY_IMPORTED_FIXTURES_SAVED')
