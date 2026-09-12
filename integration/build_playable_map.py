import json
import traceback
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
result = {'complete': False, 'passed': False}
try:
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    source = '/Game/WidowfenPrep/LVL_WidowfenLookdev'
    target = '/Game/WidowfenPrep/LVL_DarkRelicPlayable'
    if not unreal.EditorAssetLibrary.does_asset_exist(target):
        if not unreal.EditorAssetLibrary.duplicate_asset(source, target):
            raise RuntimeError('Cannot duplicate visual baseline map')
    if not levels.load_level(target):
        raise RuntimeError('Cannot open playable map')
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property('default_game_mode', unreal.EditorAssetLibrary.load_blueprint_class('/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode'))
    for actor in actors.get_all_level_actors():
        if isinstance(actor, unreal.CameraActor):
            actor.set_editor_property('auto_activate_for_player', unreal.AutoReceiveInput.DISABLED)
        if actor.get_actor_label() == 'Blackbell Reeve Chain':
            actor.set_actor_hidden_in_game(True)
        if isinstance(actor, unreal.DarkRelicEncounter):
            actors.destroy_actor(actor)
    encounter = actors.spawn_actor_from_class(unreal.DarkRelicEncounter, unreal.Vector(0,0,0))
    encounter.set_actor_label('Dark Relic Playable Encounter')
    levels.save_current_level()
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
    result.update({'passed':True,'map':target,'encounter':encounter.get_path_name(),'actors':len(actors.get_all_level_actors())})
except Exception:
    result['error']=traceback.format_exc()
finally:
    result['complete']=True
    (root/'IntegrationEvidence/playable-map.json').write_text(json.dumps(result,indent=2))
if not result['passed']:
    raise RuntimeError(result['error'])
