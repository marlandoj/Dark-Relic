import json
import traceback
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
result = {'complete': False, 'passed': False}


def asset_path(value):
    return value.get_path_name() if value else None


def vector(value):
    return [value.x, value.y, value.z]


def transform(actor):
    rotation = actor.get_actor_rotation()
    return vector(actor.get_actor_location()) + [rotation.pitch, rotation.yaw, rotation.roll] + vector(actor.get_actor_scale3d())


def visual(binding):
    paths = {key: asset_path(binding.get_editor_property(key)) for key in ('mesh', 'idle', 'move', 'locomotion', 'light_attack', 'heavy_attack', 'dodge', 'death')}
    paths.update({key: binding.get_editor_property(key) for key in ('locomotion_max_speed', 'mesh_scale')})
    paths['locomotion_axes'] = vector(binding.get_editor_property('locomotion_axes'))
    return paths


def signature(map_name):
    if not levels.load_level('/Game/WidowfenPrep/' + map_name):
        raise RuntimeError('Cannot load ' + map_name)
    encounters = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.DarkRelicEncounter)]
    if len(encounters) != 1:
        raise RuntimeError('Expected exactly one encounter')
    encounter = encounters[0]
    state = {key: encounter.get_editor_property(key) for key in ('require_character_visuals', 'require_hero_voices', 'require_enemy_voices', 'require_fury_visuals', 'voice_volume', 'hit_recoil_distance')}
    state.update({key: vector(encounter.get_editor_property(key)) for key in ('extraction_center', 'player_start')})
    state['hero_visuals'] = visual(encounter.get_editor_property('hero_visuals'))
    state['enemy_visuals'] = [visual(v) for v in encounter.get_editor_property('enemy_visuals')]
    state['hero_voices'] = [{'event': str(v.get_editor_property('event')), 'sound': asset_path(v.get_editor_property('sound'))} for v in encounter.get_editor_property('hero_voices')]
    state['enemy_voices'] = [{'role': v.get_editor_property('role'), 'sounds': [asset_path(s) for s in v.get_editor_property('pain_sounds')], 'pitch': v.get_editor_property('pitch'), 'volume': v.get_editor_property('volume')} for v in encounter.get_editor_property('enemy_voices')]
    state['fury'] = {key: asset_path(encounter.get_editor_property(key)) for key in ('fury_animation', 'fury_material')}
    replaced = ('Playable Polish Deadwood ', 'Playable Polish Branch ', 'Review Polish Shallow Water ')
    state['colliders'] = sorted((a.get_actor_label(), transform(a), asset_path(a.static_mesh_component.static_mesh)) for a in actors.get_all_level_actors() if isinstance(a, unreal.StaticMeshActor) and not a.get_actor_label().startswith(replaced) and a.static_mesh_component.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION)
    return state


try:
    source = signature('LVL_DarkRelicEnhanced')
    target = signature('LVL_DarkRelicRealistic')
    if source != target:
        raise RuntimeError('Preserved gameplay differs: ' + ', '.join(k for k in source if source[k] != target[k]))
    result['preserved'] = source
    decor = [a for a in actors.get_all_level_actors() if a.get_actor_label().startswith('Realistic Fen ')]
    if len(decor) < 500 or any(a.static_mesh_component.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION for a in decor):
        raise RuntimeError('Missing decoration or decoration collision')
    result['decor_actors'] = len(decor)
    result['passed'] = True
except Exception:
    result['error'] = traceback.format_exc()
finally:
    result['complete'] = True
    (root / 'IntegrationEvidence/realistic-parity.json').write_text(json.dumps(result, indent=2))
if not result['passed']:
    raise RuntimeError(result['error'])
