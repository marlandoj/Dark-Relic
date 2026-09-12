import json
import traceback
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
result = {'complete': False, 'passed': False, 'voices': []}
specs = [
    ('PAIN', 'Greystone_Effort_Pain'),
    ('HEAVY_PAIN', 'Greystone_Effort_PainHeavy'),
    ('LIGHT', 'Greystone_Effort_Attack'),
    ('HEAVY', 'Greystone_Effort_Ability_Primary'),
    ('DODGE', 'Greystone_Effort_Jump'),
    ('BURST', 'Greystone_Effort_Ability_Q'),
    ('FURY', 'Greystone_Effort_Ability_Ultimate_Rebirth'),
    ('HEAL', 'Greystone_Effort_BreathingLowHealth'),
    ('HEALED', 'Greystone_Health_Healed'),
    ('DEATH', 'Greystone_Effort_Death'),
    ('CHEER', 'Greystone_Effort_Cheer'),
]
try:
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    target = '/Game/WidowfenPrep/LVL_DarkRelicEnhanced'
    if not levels.load_level(target):
        raise RuntimeError('Enhanced map could not load')
    encounters = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.DarkRelicEncounter)]
    if len(encounters) != 1:
        raise RuntimeError('Expected one live encounter')
    bindings = []
    for event, name in specs:
        sound = unreal.load_asset('/Game/ParagonGreystone/Audio/Cues/' + name)
        if not isinstance(sound, unreal.SoundBase):
            raise RuntimeError('Missing voice cue: ' + name)
        binding = unreal.DarkRelicVoiceBinding()
        binding.set_editor_property('event', getattr(unreal.DarkRelicVoice, event))
        binding.set_editor_property('sound', sound)
        bindings.append(binding)
        result['voices'].append({'event': event, 'path': sound.get_path_name()})
    encounter = encounters[0]
    encounter.set_editor_property('hero_voices', bindings)
    encounter.set_editor_property('require_hero_voices', True)
    encounter.set_editor_property('voice_volume', 0.85)
    encounter.set_editor_property('hit_recoil_distance', 55.0)
    if not levels.save_current_level() or not levels.load_level(target):
        raise RuntimeError('Map save/readback failed')
    encounter = next(a for a in actors.get_all_level_actors() if isinstance(a, unreal.DarkRelicEncounter))
    actual = encounter.get_editor_property('hero_voices')
    if len(actual) != len(specs) or not encounter.get_editor_property('require_hero_voices'):
        raise RuntimeError('Persisted voice bindings mismatch')
    for binding, expected in zip(actual, result['voices']):
        if binding.get_editor_property('sound').get_path_name() != expected['path']:
            raise RuntimeError('Persisted cue mismatch')
    result.update(passed=True, map=target)
except Exception:
    result['error'] = traceback.format_exc()
finally:
    result['complete'] = True
    (root / 'IntegrationEvidence/feedback-bindings.json').write_text(json.dumps(result, indent=2))
if not result['passed']:
    raise RuntimeError(result['error'])
