import json
import traceback
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
result = {'complete': False, 'passed': False, 'lights': [], 'moved_decor': []}
try:
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    assets = unreal.EditorAssetLibrary
    target = '/Game/WidowfenPrep/LVL_DarkRelicEnhanced'
    if not levels.load_level(target):
        raise RuntimeError('Verified enhanced map missing')
    for actor in actors.get_all_level_actors():
        if actor.get_actor_label().startswith('Review Polish '):
            actors.destroy_actor(actor)
    for actor in actors.get_all_level_actors():
        label = actor.get_actor_label()
        if isinstance(actor, unreal.PointLight):
            light = actor.point_light_component
            actor.modify()
            light.modify()
            ward = 'Ward Lantern' in label
            light.set_editor_property('intensity', 120.0 if ward else 12.0)
            light.set_editor_property('attenuation_radius', 480.0 if ward else 280.0)
            light.set_editor_property('source_radius', 35.0)
            light.set_editor_property('cast_shadows', False)
            result['lights'].append({'label': label, 'intensity': light.get_editor_property('intensity')})
        if label.startswith('Final Day Path Prop '):
            index = int(label.rsplit(' ', 1)[1])
            if index < 6:
                actor.modify()
                pos = actor.get_actor_location()
                pos.x = -1220.0 if pos.x < 0 else 1220.0
                actor.set_actor_location(pos, False, False)
                result['moved_decor'].append({'label': label, 'x': pos.x, 'y': pos.y})
        if label.startswith(('Playable Polish Deadwood ', 'Playable Polish Branch ')):
            pos = actor.get_actor_location()
            if abs(pos.x) > 1550:
                actor.modify()
                pos.x *= 0.84
                actor.set_actor_location(pos, False, False)
    material_path = '/Game/WidowfenPrep/Materials/M_ReviewShallowWater'
    material = assets.load_asset(material_path)
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_ReviewShallowWater', '/Game/WidowfenPrep/Materials', unreal.Material, unreal.MaterialFactoryNew())
        ml = unreal.MaterialEditingLibrary
        color = ml.create_material_expression(material, unreal.MaterialExpressionConstant3Vector)
        color.set_editor_property('constant', unreal.LinearColor(0.035, 0.045, 0.035))
        ml.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
        roughness = ml.create_material_expression(material, unreal.MaterialExpressionConstant)
        roughness.set_editor_property('r', 0.28)
        ml.connect_material_property(roughness, '', unreal.MaterialProperty.MP_ROUGHNESS)
        ml.recompile_material(material)
    mesh = assets.load_asset('/Engine/BasicShapes/Cylinder')
    for i, (x, y, sx, sy) in enumerate([(-780, -150, 1.9, 0.7), (780, 300, 2.3, 0.9), (-720, 620, 1.4, 0.6), (720, 1040, 2.0, 0.75), (-600, 1360, 1.1, 0.5)]):
        actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, 1.5), unreal.Rotator(0, i*43, 0))
        actor.set_actor_label('Review Polish Shallow Water ' + str(i))
        actor.static_mesh_component.set_static_mesh(mesh)
        actor.static_mesh_component.set_material(0, material)
        actor.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        actor.set_actor_scale3d(unreal.Vector(sx, sy, 0.008))
    encounters = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.DarkRelicEncounter)]
    if len(encounters) != 1:
        raise RuntimeError('Encounter count changed')
    for key in ['require_character_visuals', 'require_fury_visuals', 'require_hero_voices', 'require_enemy_voices']:
        if not encounters[0].get_editor_property(key):
            raise RuntimeError('Required binding lost: ' + key)
    if not levels.save_current_level() or not levels.load_level(target):
        raise RuntimeError('Polished map persistence failed')
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    result.update(passed=True, actors=len(actors.get_all_level_actors()), map=target)
except Exception:
    result['error'] = traceback.format_exc()
finally:
    result['complete'] = True
    (root / 'IntegrationEvidence/review-polish.json').write_text(json.dumps(result, indent=2))
if not result['passed']:
    raise RuntimeError(result['error'])
