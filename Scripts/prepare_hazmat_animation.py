"""Finalize retargeted Hazmat clips and first-person materials in the editor.

Uses the native animation data controller. Does not enter Play or modify a map.
"""
import unreal

BASE = '/Game/Characters/HazmatSuit3'
LIB = unreal.AnimationLibrary
ASSETS = unreal.EditorAssetLibrary


def pose(seq, bone, time):
    return LIB.get_bone_pose_for_time(seq, bone, time, False)


def normalize_clips():
    if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
        raise RuntimeError('Editor mode required')
    results = []
    for path in ASSETS.list_assets(BASE + '/Animations'):
        seq = unreal.load_asset(path)
        if not isinstance(seq, unreal.AnimSequence):
            continue
        if ASSETS.get_metadata_tag(seq, 'HazmatNormalized') == '1':
            continue
        count = LIB.get_num_frames(seq) + 1
        length = seq.get_play_length()
        times = [length * i / (count - 1) for i in range(count)]
        roots = [pose(seq, 'H_Armature', t) for t in times]
        hips = [pose(seq, 'Hips', t) for t in times]
        moving = '_Walk_' in seq.get_name() or '_Jog_' in seq.get_name()
        delta = hips[-1].translation - hips[0].translation if moving else unreal.Vector()
        # The exporter writes the pelvis in centimeters but drops the imported
        # armature's 100x scale. Restore both halves of that coordinate system.
        positions = []
        for i, p in enumerate(hips):
            v = p.translation
            fraction = i / (count - 1)
            positions.append(unreal.Vector((v.x-delta.x*fraction)/100,
                                           (v.y-delta.y*fraction)/100, v.z/100))
        controller = seq.get_editor_property('controller')
        controller.open_bracket('Normalize Hazmat units and in-place motion')
        try:
            assert controller.set_bone_track_keys('H_Armature',
                [p.translation for p in roots], [p.rotation for p in roots],
                [unreal.Vector(100,100,100)] * count)
            assert controller.set_bone_track_keys('Hips', positions,
                [p.rotation for p in hips], [p.scale3d for p in hips])
        finally:
            controller.close_bracket()
        seq.set_editor_property('enable_root_motion', False)
        seq.set_editor_property('force_root_lock', False)
        ASSETS.set_metadata_tag(seq, 'HazmatNormalized', '1')
        assert ASSETS.save_loaded_asset(seq)
        results.append({'asset': seq.get_name(), 'keys': count,
                        'natural_speed': (delta.x**2+delta.y**2)**0.5 / length})
    return results


def first_person_materials():
    lib = unreal.MaterialEditingLibrary
    path = BASE + '/Materials/M_H_SUIT_FirstPerson'
    mat = unreal.load_asset(path) or ASSETS.duplicate_asset(BASE+'/Materials/M_H_SUIT', path)
    lib.delete_all_material_expressions(mat)
    for suffix, prop, sampler in [
        ('Basecolor', unreal.MaterialProperty.MP_BASE_COLOR, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR),
        ('Normal', unreal.MaterialProperty.MP_NORMAL, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL),
        ('Roughness', unreal.MaterialProperty.MP_ROUGHNESS, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)]:
        node = lib.create_material_expression(mat, unreal.MaterialExpressionTextureSample)
        node.texture = unreal.load_asset(BASE+'/Textures/T_H_Suit_'+suffix)
        node.sampler_type = sampler
        assert lib.connect_material_property(node, 'R' if suffix == 'Roughness' else 'RGB', prop)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    # Reference-pose Z keeps the hood out of the camera without clipping gloves
    # as they swing. World/remote meshes keep their original opaque suit.
    pos = lib.create_material_expression(mat, unreal.MaterialExpressionLocalPosition)
    pos.set_editor_property('local_origin', unreal.LocalPositionOrigin.INSTANCE_PRE_SKINNING)
    interpolator = lib.create_material_expression(mat, unreal.MaterialExpressionVertexInterpolator)
    subtract = lib.create_material_expression(mat, unreal.MaterialExpressionSubtract)
    subtract.set_editor_property('const_a', 145.0)
    assert lib.connect_material_expressions(pos, 'Z', interpolator, 'VS')
    assert lib.connect_material_expressions(interpolator, '', subtract, 'B')
    lib.connect_material_property(subtract, '', unreal.MaterialProperty.MP_OPACITY_MASK)
    lib.layout_material_expressions(mat)
    errors = lib.recompile_material(mat)
    if errors:
        raise RuntimeError(str(errors))
    assert ASSETS.save_loaded_asset(mat)
    path = BASE + '/Materials/M_FirstPersonHidden'
    hidden = unreal.load_asset(path) or unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'M_FirstPersonHidden', BASE+'/Materials', unreal.Material, unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(hidden)
    hidden.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    lib.set_base_material_usage(hidden, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH)
    zero = lib.create_material_expression(hidden, unreal.MaterialExpressionConstant)
    zero.set_editor_property('r', 0.0)
    lib.connect_material_property(zero, '', unreal.MaterialProperty.MP_OPACITY_MASK)
    errors = lib.recompile_material(hidden)
    if errors:
        raise RuntimeError(str(errors))
    assert ASSETS.save_loaded_asset(hidden)
    return [mat.get_path_name(), hidden.get_path_name()]


def mask_visor():
    lib = unreal.MaterialEditingLibrary
    name = 'M_MaskVisor'
    mat = unreal.load_asset(BASE+'/Materials/'+name) or unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, BASE+'/Materials', unreal.Material, unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(mat)
    mat.set_editor_property('material_domain', unreal.MaterialDomain.MD_POST_PROCESS)
    mat.set_editor_property('blendable_location', unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)
    scene = lib.create_material_expression(mat, unreal.MaterialExpressionSceneTexture)
    scene.set_editor_property('scene_texture_id', unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    screen = lib.create_material_expression(mat, unreal.MaterialExpressionScreenPosition)
    effect = lib.create_material_expression(mat, unreal.MaterialExpressionCustom)
    effect.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs = []
    for name in ['Scene', 'UV']:
        item = unreal.CustomInput()
        item.set_editor_property('input_name', name)
        inputs.append(item)
    effect.set_editor_property('inputs', inputs)
    effect.set_editor_property('description', 'Subtle mask glass and peripheral rubber seal')
    effect.set_editor_property('code', r'''
float2 p = UV * 2.0 - 1.0;
float r = length(p * float2(0.83, 1.02));
float edge = smoothstep(0.60, 1.14, r);
float seal = smoothstep(1.10, 1.19, r);
float rim = exp(-pow((r - 1.08) * 65.0, 2.0));
float upperSheen = exp(-pow((p.y + 0.76 + 0.13*p.x*p.x)*35.0, 2.0))
                 * (1.0-smoothstep(0.42,0.90,abs(p.x)));
float lowerMist = smoothstep(0.60,1.02,p.y) * 0.026;
float scratch1 = exp(-pow((p.x + 0.71 + 0.10*p.y)*850.0,2.0))
               * smoothstep(-0.5,-0.3,p.y) * (1.0-smoothstep(0.18,0.35,p.y));
float scratch2 = exp(-pow((p.x - 0.79 - 0.08*p.y)*1100.0,2.0))
               * smoothstep(-0.2,0.0,p.y) * (1.0-smoothstep(0.4,0.5,p.y));
float3 color = Scene.rgb * lerp(float3(0.99,1.0,0.998),float3(0.86,0.93,0.94),edge);
float luminance = dot(Scene.rgb,float3(0.2126,0.7152,0.0722));
float3 glass = float3(0.60,0.77,0.79) * (0.12 + 0.35*luminance);
color += glass * (0.06*upperSheen + 0.025*rim + lowerMist + 0.035*(scratch1+scratch2));
return lerp(color,float3(0.008,0.010,0.011),seal*0.94);
''')
    assert lib.connect_material_expressions(scene, 'Color', effect, 'Scene')
    assert lib.connect_material_expressions(screen, 'ViewportUV', effect, 'UV')
    assert lib.connect_material_property(effect, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.layout_material_expressions(mat)
    errors = lib.recompile_material(mat)
    if errors:
        raise RuntimeError(str(errors))
    assert ASSETS.save_loaded_asset(mat)
    return mat.get_path_name()
