import json
import traceback
from pathlib import Path
import unreal

result = {'complete': False, 'passed': False, 'props': []}
root = Path(unreal.Paths.project_dir())
target = '/Game/WidowfenPrep/LVL_DarkRelicEnhanced'
try:
    assets = unreal.EditorAssetLibrary
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not assets.does_asset_exist(target):
        if not assets.duplicate_asset('/Game/WidowfenPrep/LVL_DarkRelicCharacters', target):
            raise RuntimeError('Character map could not be copied')
    if not levels.load_level(target):
        raise RuntimeError('Enhancement map could not load')
    for actor in actors.get_all_level_actors():
        if actor.get_actor_label().startswith('Final Day '):
            actors.destroy_actor(actor)
    for actor in actors.get_all_level_actors():
        actor.modify()
        if isinstance(actor, unreal.DirectionalLight):
            light = actor.light_component
            light.modify()
            light.set_editor_property('intensity', 1.0)
            light.set_light_color(unreal.LinearColor(0.90, 0.86, 0.76))
            light.set_editor_property('light_source_angle', 4.0)
            light.set_editor_property('forward_shading_priority', 1)
        elif isinstance(actor, unreal.SkyLight):
            actor.light_component.set_editor_property('intensity', 1.1)
            actor.light_component.set_editor_property('light_color', unreal.Color(205, 211, 208, 255))
        elif isinstance(actor, unreal.PointLight):
            light = actor.point_light_component
            light.modify()
            light.set_light_color(unreal.LinearColor(1.0, 0.56, 0.24))
            light.set_editor_property('use_temperature', True)
            light.set_editor_property('temperature', 3800.0)
            light.set_editor_property('intensity', 45.0)
            light.set_editor_property('source_radius', 35.0)
        elif isinstance(actor, unreal.ExponentialHeightFog):
            actor.component.set_editor_property('fog_density', 0.025)
        elif isinstance(actor, unreal.PostProcessVolume):
            settings = actor.get_editor_property('settings')
            settings.set_editor_property('override_auto_exposure_bias', True)
            settings.set_editor_property('auto_exposure_bias', -1.3)
            settings.set_editor_property('override_vignette_intensity', True)
            settings.set_editor_property('vignette_intensity', 0.22)
            settings.set_editor_property('override_motion_blur_amount', True)
            settings.set_editor_property('motion_blur_amount', 0.0)
            actor.set_editor_property('settings', settings)
    fill = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 1100), unreal.Rotator(-28, 135, 0))
    fill.set_actor_label('Final Day Soft Character Fill')
    fill.light_component.set_editor_property('intensity', 0.8)
    fill.light_component.set_light_color(unreal.LinearColor(0.80, 0.84, 0.80))
    fill.light_component.set_editor_property('cast_shadows', False)
    fill.light_component.set_editor_property('forward_shading_priority', 0)
    fill.light_component.set_editor_property('atmosphere_sun_light', False)
    placements = [
        ('barrel_03', -1050, -250, 110), ('wooden_crate_01', 850, 100, 100),
        ('tree_stump_01', -1000, 450, 125), ('rock_moss_set_01', 850, 900, 135),
        ('wooden_crate_01', -900, 1300, 95), ('barrel_03', 700, 1450, 110),
        ('wooden_lantern_01', -450, 1450, 75), ('wooden_lantern_01', 450, 1450, 75),
    ]
    for index, (asset_id, x, y, length) in enumerate(placements):
        candidates = [assets.load_asset(p) for p in assets.list_assets('/Game/WidowfenPrep/Sources/' + asset_id, recursive=True, include_folder=False)]
        meshes = [m for m in candidates if isinstance(m, unreal.StaticMesh)]
        if not meshes:
            raise RuntimeError('Verified environment mesh missing: ' + asset_id)
        mesh = max(meshes, key=lambda m: (m.get_bounding_box().max - m.get_bounding_box().min).length())
        bounds = mesh.get_bounding_box()
        size = bounds.max - bounds.min
        scale = length / max(size.x, size.y, size.z)
        actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, -bounds.min.z * scale))
        actor.set_actor_label('Final Day Path Prop ' + str(index))
        actor.static_mesh_component.set_static_mesh(mesh)
        actor.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
        result['props'].append({'mesh': mesh.get_path_name(), 'location': [x, y], 'collision': False})
    for x in [-450, 450]:
        light = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x, 1450, 160))
        light.set_actor_label('Final Day Ward Lantern ' + str(x))
        light.point_light_component.set_light_color(unreal.LinearColor(1.0, 0.56, 0.24))
        light.point_light_component.set_editor_property('intensity', 220.0)
        light.point_light_component.set_editor_property('attenuation_radius', 600.0)
        light.point_light_component.set_editor_property('cast_shadows', False)
    if len([a for a in actors.get_all_level_actors() if isinstance(a, unreal.DarkRelicEncounter)]) != 1:
        raise RuntimeError('Expected exactly one runtime encounter')
    if not levels.save_current_level() or not levels.load_level(target):
        raise RuntimeError('Enhanced map save/readback failed')
    result['lighting'] = []
    for actor in actors.get_all_level_actors():
        if isinstance(actor, (unreal.DirectionalLight, unreal.PointLight)):
            component = actor.light_component if isinstance(actor, unreal.DirectionalLight) else actor.point_light_component
            color = component.get_editor_property('light_color')
            result['lighting'].append({'label': actor.get_actor_label(), 'intensity': component.get_editor_property('intensity'), 'rgb': [color.r, color.g, color.b]})
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    result.update(passed=True, map=target, actors=len(actors.get_all_level_actors()))
except Exception:
    result['error'] = traceback.format_exc()
finally:
    result['complete'] = True
    (root / 'IntegrationEvidence/enhancement-polish.json').write_text(json.dumps(result, indent=2))
if not result['passed']:
    raise RuntimeError(result['error'])
