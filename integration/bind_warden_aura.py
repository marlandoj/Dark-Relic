import json
import traceback
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
result = {'complete': False, 'passed': False}
try:
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    target = '/Game/WidowfenPrep/LVL_DarkRelicEnhanced'
    if not levels.load_level(target):
        raise RuntimeError('Enhanced map could not load')
    encounter, = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.DarkRelicEncounter)]
    animation = unreal.load_asset('/Game/ParagonGreystone/Characters/Heroes/Greystone/Animations/Emote_Taunt_BringItOn_T1')
    hero = encounter.get_editor_property('hero_visuals').get_editor_property('mesh')
    if not isinstance(animation, unreal.AnimSequence) or animation.get_editor_property('skeleton') != hero.get_editor_property('skeleton'):
        raise RuntimeError('Fury animation skeleton mismatch')
    if animation.get_editor_property('additive_anim_type') != unreal.AdditiveAnimationType.AAT_NONE:
        raise RuntimeError('Fury requires a full pose sequence')
    path = '/Game/WidowfenPrep/M_WardenFury'
    material = unreal.load_asset(path)
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_WardenFury', '/Game/WidowfenPrep', unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property('two_sided', False)
    lib = unreal.MaterialEditingLibrary
    lib.delete_all_material_expressions(material)
    def node(cls, **properties):
        expression = lib.create_material_expression(material, cls)
        for key, value in properties.items():
            expression.set_editor_property(key, value)
        return expression
    def connect(source, destination, name):
        if not lib.connect_material_expressions(source, '', destination, name):
            raise RuntimeError('Material connection failed: ' + name)
    strength = node(unreal.MaterialExpressionScalarParameter, parameter_name='Strength', default_value=0.0)
    fresnel = node(unreal.MaterialExpressionFresnel, exponent=3.0, base_reflect_fraction=0.015)
    crimson = node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(3.2,0.012,0.008,1))
    gold = node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(5.0,0.65,0.045,1))
    color = node(unreal.MaterialExpressionLinearInterpolate)
    connect(crimson,color,'A')
    connect(gold,color,'B')
    connect(fresnel,color,'Alpha')
    emissive = node(unreal.MaterialExpressionMultiply)
    connect(color,emissive,'A')
    connect(strength,emissive,'B')
    position = node(unreal.MaterialExpressionWorldPosition)
    time = node(unreal.MaterialExpressionTime)
    direction = node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0,0,-65,1))
    drift = node(unreal.MaterialExpressionMultiply)
    connect(time,drift,'A')
    connect(direction,drift,'B')
    moving_position = node(unreal.MaterialExpressionAdd)
    connect(position,moving_position,'A')
    connect(drift,moving_position,'B')
    noise = node(unreal.MaterialExpressionNoise, scale=0.035, levels=2, quality=1, output_min=0.0, output_max=1.0)
    position_inputs = [name for name in lib.get_material_expression_input_names(noise) if 'position' in name.lower()]
    if len(position_inputs) != 1:
        raise RuntimeError('Noise position input could not be resolved')
    connect(moving_position,noise,position_inputs[0])
    wisps = node(unreal.MaterialExpressionPower, const_exponent=3.0)
    connect(noise,wisps,'Base')
    rim = node(unreal.MaterialExpressionMultiply)
    connect(fresnel,rim,'A')
    connect(wisps,rim,'B')
    opacity = node(unreal.MaterialExpressionMultiply, const_b=0.24)
    connect(rim,opacity,'A')
    if not lib.connect_material_property(emissive,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR) or not lib.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY):
        raise RuntimeError('Material output connection failed')
    lib.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError('Material save failed')
    encounter.set_editor_property('fury_animation',animation)
    encounter.set_editor_property('fury_material',material)
    encounter.set_editor_property('require_fury_visuals',True)
    if not levels.save_current_level() or not levels.load_level(target):
        raise RuntimeError('Map save/readback failed')
    encounter, = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.DarkRelicEncounter)]
    if encounter.get_editor_property('fury_animation') != animation or encounter.get_editor_property('fury_material') != material or not encounter.get_editor_property('require_fury_visuals'):
        raise RuntimeError('Fury persisted bindings mismatch')
    result.update(passed=True, animation=animation.get_path_name(), material=material.get_path_name(), sequence_seconds=animation.get_editor_property('sequence_length'))
except Exception:
    result['error'] = traceback.format_exc()
finally:
    result['complete'] = True
    (root / 'IntegrationEvidence/aura-bindings.json').write_text(json.dumps(result,indent=2))
if not result['passed']:
    raise RuntimeError(result['error'])
