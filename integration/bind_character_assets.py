import json
import traceback
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
result = {'complete': False, 'passed': False, 'bindings': []}
hero = '/Game/ParagonGreystone/Characters/Heroes/Greystone'
minions = '/Game/ParagonMinions/Characters/Minions'
specs = [
    dict(role='Warden', mesh=hero+'/Meshes/Greystone', folder=hero+'/Animations', idle='Idle', move='Jog_Fwd', light_attack='Attack_A_Fast', heavy_attack='Attack_A_Slow', dodge='Jump_Start', death='Death', locomotion='Blendspaces/Greystone_Locomotion_BS'),
    dict(role='Dreg', mesh=minions+'/Dusk_Minions/Meshes/Minion_Lane_Melee_Dusk', folder=minions+'/Down_Minions/Animations/Melee', idle='NonCombat_Idle', move='Combat_JogFwd', light_attack='Attack_A', death='Death_A', locomotion='Blendspaces/IdleToRun_A_Combat'),
    dict(role='Hexbound', mesh=minions+'/Dusk_Minions/Meshes/Minion_Lane_Ranged_Dusk', folder=minions+'/Down_Minions/Animations/Ranged', idle='Idle_A', move='Jog_Fwd_Combat_A', light_attack='Fire_A', death='Death_Front_A', locomotion='Blendspaces/IdleToRun_A_Combat'),
    dict(role='Bellkeeper', mesh=minions+'/Dusk_Minions/Meshes/Minion_Lane_Super_Dusk', folder=minions+'/Down_Minions/Animations/Super', idle='Idle', move='Combat_Jog_Fwd_Alt', light_attack='Attack_A_Dusk', death='Death_Front', locomotion='Blendspaces/Idle2Run_Super'),
]

def load(path, kind):
    asset = unreal.load_asset(path)
    if not isinstance(asset, kind):
        raise RuntimeError(f'{path} did not load as {kind.__name__}')
    return asset

try:
    profiles = []
    for spec in specs:
        profile = unreal.DarkRelicCharacterVisuals()
        mesh = load(spec['mesh'], unreal.SkeletalMesh)
        skeleton = mesh.get_editor_property('skeleton')
        profile.set_editor_property('mesh', mesh)
        record = {'role': spec['role'], 'mesh': mesh.get_path_name(), 'skeleton': skeleton.get_path_name(), 'animations': {}}
        for prop in ['idle', 'move', 'light_attack', 'heavy_attack', 'dodge', 'death', 'locomotion']:
            if prop not in spec:
                continue
            asset = load(spec['folder']+'/'+spec[prop], unreal.BlendSpace if prop=='locomotion' else unreal.AnimSequence)
            if asset.get_editor_property('skeleton') != skeleton:
                raise RuntimeError(f'Skeleton mismatch: {spec["role"]}/{prop}')
            if isinstance(asset, unreal.AnimSequence):
                if asset.get_editor_property('additive_anim_type') != unreal.AdditiveAnimationType.AAT_NONE:
                    raise RuntimeError(f'Additive sequence cannot provide full-body pose: {asset.get_path_name()}')
                asset.set_editor_property('enable_root_motion', False)
                asset.set_editor_property('force_root_lock', True)
                unreal.EditorAssetLibrary.save_loaded_asset(asset)
            else:
                params = asset.get_editor_property('blend_parameters')
                axes = []
                values = [0.0,0.0,0.0]
                for i, param in enumerate(params):
                    name = str(param.get_editor_property('display_name'))
                    low = float(param.get_editor_property('min'))
                    high = float(param.get_editor_property('max'))
                    axes.append({'name':name, 'min':low, 'max':high})
                    if 'speed' in name.lower():
                        values[i] = 1.0
                        profile.set_editor_property('locomotion_max_speed', high)
                if not any(values):
                    raise RuntimeError(f'No verified speed axis for {asset.get_path_name()}: {axes}')
                profile.set_editor_property('locomotion_axes', unreal.Vector(*values))
                record['blend_axes'] = axes
            profile.set_editor_property(prop, asset)
            record['animations'][prop] = asset.get_path_name()
        profiles.append(profile)
        result['bindings'].append(record)
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    target = '/Game/WidowfenPrep/LVL_DarkRelicCharacters'
    if not unreal.EditorAssetLibrary.does_asset_exist(target):
        if not unreal.EditorAssetLibrary.duplicate_asset('/Game/WidowfenPrep/LVL_DarkRelicPlayable', target):
            raise RuntimeError('Could not duplicate playable level')
    if not levels.load_level(target):
        raise RuntimeError('Could not load character level')
    encounters = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.DarkRelicEncounter)]
    if len(encounters) != 1:
        raise RuntimeError(f'Expected one encounter, found {len(encounters)}')
    encounter = encounters[0]
    encounter.set_editor_property('hero_visuals', profiles[0])
    encounter.set_editor_property('enemy_visuals', profiles[1:])
    encounter.set_editor_property('require_character_visuals', True)
    if not levels.save_current_level():
        raise RuntimeError('Character map save failed')
    if not levels.load_level(target):
        raise RuntimeError('Character map readback failed')
    encounter = next(a for a in actors.get_all_level_actors() if isinstance(a, unreal.DarkRelicEncounter))
    if encounter.get_editor_property('hero_visuals').get_editor_property('mesh') != profiles[0].get_editor_property('mesh') or len(encounter.get_editor_property('enemy_visuals')) != 3 or not encounter.get_editor_property('require_character_visuals'):
        raise RuntimeError('Persisted character bindings do not match')
    result.update(passed=True, map=target, actors=len(actors.get_all_level_actors()))
except Exception:
    result['error'] = traceback.format_exc()
finally:
    result['complete'] = True
    (root/'IntegrationEvidence/character-bindings.json').write_text(json.dumps(result, indent=2))
if not result['passed']:
    raise RuntimeError(result['error'])
