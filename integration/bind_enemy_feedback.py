import json
import traceback
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
result = {'complete': False, 'passed': False, 'voices': []}
try:
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    target = '/Game/WidowfenPrep/LVL_DarkRelicEnhanced'
    if not levels.load_level(target):
        raise RuntimeError('Enhanced map could not load')
    encounters = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.DarkRelicEncounter)]
    if len(encounters) != 1:
        raise RuntimeError('Expected one encounter')
    bindings = []
    for role, kind, pitch, volume in [(0, 'Pain', 0.90, 0.65), (1, 'Pain', 0.82, 0.65), (2, 'PainHeavy', 0.65, 0.85)]:
        sounds = []
        for number in range(10, 15):
            path = '/Game/ParagonGreystone/Audio/Wavs/Greystone_Effort_' + kind + '_' + str(number).zfill(3)
            sound = unreal.load_asset(path)
            if not isinstance(sound, unreal.SoundWave):
                raise RuntimeError('Missing recorded pain wave: ' + path)
            sounds.append(sound)
        binding = unreal.DarkRelicEnemyVoiceBinding()
        binding.set_editor_property('role', role)
        binding.set_editor_property('pain_sounds', sounds)
        binding.set_editor_property('pitch', pitch)
        binding.set_editor_property('volume', volume)
        bindings.append(binding)
        result['voices'].append({'role': role, 'pitch': pitch, 'volume': volume, 'paths': [s.get_path_name() for s in sounds]})
    encounters[0].set_editor_property('enemy_voices', bindings)
    encounters[0].set_editor_property('require_enemy_voices', True)
    if not levels.save_current_level() or not levels.load_level(target):
        raise RuntimeError('Map save/readback failed')
    encounter = next(a for a in actors.get_all_level_actors() if isinstance(a, unreal.DarkRelicEncounter))
    actual = encounter.get_editor_property('enemy_voices')
    if len(actual) != 3 or not encounter.get_editor_property('require_enemy_voices'):
        raise RuntimeError('Persisted enemy binding mismatch')
    for binding, expected in zip(actual, result['voices']):
        if [s.get_path_name() for s in binding.get_editor_property('pain_sounds')] != expected['paths']:
            raise RuntimeError('Persisted pain recordings mismatch')
    if not encounter.get_editor_property('require_fury_visuals') or not encounter.get_editor_property('require_hero_voices'):
        raise RuntimeError('Previous Warden binding requirements lost')
    result.update(passed=True, map=target)
except Exception:
    result['error'] = traceback.format_exc()
finally:
    result['complete'] = True
    (root / 'IntegrationEvidence/enemy-bindings.json').write_text(json.dumps(result, indent=2))
if not result['passed']:
    raise RuntimeError(result['error'])
