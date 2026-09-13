import json
import math
import random
import sys
import traceback
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
sys.path.insert(0, str(root/'IntegrationEvidence'))
from widowfen_geometry import build_geometry, planting_plan

target = '/Game/WidowfenPrep/LVL_DarkRelicRealistic'
destination = '/Game/WidowfenPrep/Realistic'
result = {'complete': False, 'passed': False, 'map': target}
assets = unreal.EditorAssetLibrary
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ml = unreal.MaterialEditingLibrary


def material(name, color, roughness, texture=None, world_uv=False, two_sided=False):
    path = destination+'/Materials/'+name
    if assets.does_asset_exist(path):
        mat = assets.load_asset(path)
        ml.delete_all_material_expressions(mat)
    else:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, destination+'/Materials', unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('two_sided', two_sided)
    rgb = ml.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector)
    rgb.set_editor_property('constant', unreal.LinearColor(*color))
    color_node = rgb
    if texture:
        sample = ml.create_material_expression(mat, unreal.MaterialExpressionTextureSample)
        sample.set_editor_property('texture', texture)
        if world_uv:
            pos = ml.create_material_expression(mat, unreal.MaterialExpressionWorldPosition)
            mask = ml.create_material_expression(mat, unreal.MaterialExpressionComponentMask)
            mask.set_editor_property('r', True)
            mask.set_editor_property('g', True)
            mask.set_editor_property('b', False)
            uv = ml.create_material_expression(mat, unreal.MaterialExpressionMultiply)
            uv.set_editor_property('const_b', 0.0025)
            if not ml.connect_material_expressions(pos, '', mask, ''):
                raise RuntimeError('Cannot connect world-position texture coordinates')
            ml.connect_material_expressions(mask, '', uv, 'A')
            ml.connect_material_expressions(uv, '', sample, 'UVs')
        color_node = ml.create_material_expression(mat, unreal.MaterialExpressionMultiply)
        ml.connect_material_expressions(sample, 'RGB', color_node, 'A')
        ml.connect_material_expressions(rgb, '', color_node, 'B')
    ml.connect_material_property(color_node, '', unreal.MaterialProperty.MP_BASE_COLOR)
    rough = ml.create_material_expression(mat, unreal.MaterialExpressionConstant)
    rough.set_editor_property('r', roughness)
    ml.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    ml.recompile_material(mat)
    return mat


def place(mesh, name, position, scale=(1, 1, 1), rotation=(0, 0, 0), mat=None):
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*position), unreal.Rotator(*rotation))
    if not actor:
        raise RuntimeError('Spawn failed: '+name)
    actor.set_actor_label('Realistic Fen '+name)
    component = actor.static_mesh_component
    component.set_static_mesh(mesh)
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    if mat:
        for slot in range(max(1, component.get_num_materials())):
            component.set_material(slot, mat)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    actor.set_actor_enable_collision(False)
    actor.static_mesh_component.set_collision_profile_name('NoCollision')
    actor.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    return actor


def source_meshes(asset_id):
    return [m for m in (assets.load_asset(p) for p in assets.list_assets('/Game/WidowfenPrep/Sources/'+asset_id, recursive=True, include_folder=False)) if isinstance(m, unreal.StaticMesh)]


def fit(mesh, name, x, y, z, length, yaw=0):
    bounds = mesh.get_bounding_box()
    size = bounds.max-bounds.min
    scale = length/max(size.x, size.y, size.z)
    return place(mesh, name, (x, y, z-bounds.min.z*scale), (scale,)*3, (0, yaw, 0))


def encounter_signature():
    encounters = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.DarkRelicEncounter)]
    if len(encounters) != 1:
        raise RuntimeError('Expected one encounter')
    encounter = encounters[0]
    bindings = {key: bool(encounter.get_editor_property(key)) for key in ('require_character_visuals', 'require_fury_visuals', 'require_hero_voices', 'require_enemy_voices')}
    if not all(bindings.values()):
        raise RuntimeError('Required gameplay binding missing')
    location = encounter.get_actor_location()
    return {'bindings': bindings, 'position': [location.x, location.y, location.z]}


try:
    if not assets.does_asset_exist(target):
        if not assets.duplicate_asset('/Game/WidowfenPrep/LVL_DarkRelicEnhanced', target):
            raise RuntimeError('Cannot copy accepted map')
    if not levels.load_level(target):
        raise RuntimeError('Cannot load environment candidate')
    before = encounter_signature()
    for actor in actors.get_all_level_actors():
        if actor.get_actor_label().startswith(('Realistic Fen ', 'Playable Polish Deadwood ', 'Playable Polish Branch ', 'Review Polish Shallow Water ')):
            actors.destroy_actor(actor)
    generated = root/'IntegrationEvidence/GeneratedWidowfen'
    result['mesh_triangles'] = build_geometry(generated)
    meshes = {}
    for name in result['mesh_triangles']:
        folder = destination+'/Meshes/'+name
        if not assets.does_directory_exist(folder):
            task = unreal.AssetImportTask()
            task.set_editor_property('filename', str(generated/(name+'.glb')))
            task.set_editor_property('destination_path', folder)
            task.set_editor_property('automated', True)
            task.set_editor_property('save', True)
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        candidates = [m for m in (assets.load_asset(p) for p in assets.list_assets(folder, recursive=True, include_folder=False)) if isinstance(m, unreal.StaticMesh)]
        if len(candidates) != 1:
            raise RuntimeError('Expected one imported mesh: '+name)
        meshes[name] = candidates[0]
    textures = [assets.load_asset(p) for p in assets.list_assets('/Game/WidowfenPrep/Sources/forrest_ground_01', recursive=True, include_folder=False)]
    diffuse = next(t for t in textures if isinstance(t, unreal.Texture2D) and '_diff_' in t.get_name())
    wet = material('M_FenWetEarth', (0.43, 0.46, 0.40), 0.42, diffuse, True)
    bark = material('M_FenBark', (0.21, 0.23, 0.20), 0.88, diffuse)
    leaf = material('M_FenLeaves', (0.19, 0.28, 0.15), 0.86, diffuse, two_sided=True)
    reed = material('M_FenReeds', (0.24, 0.28, 0.14), 0.84, diffuse, two_sided=True)
    slate = material('M_FenSlate', (0.07, 0.09, 0.095), 0.38)
    water = material('M_FenStillWater', (0.021, 0.030, 0.032), 0.16)
    stone = assets.load_asset('/Game/WidowfenPrep/Materials/M_medieval_blocks_02')
    wood = assets.load_asset('/Game/WidowfenPrep/Materials/M_dark_wooden_planks')
    cube = assets.load_asset('/Engine/BasicShapes/Cube')
    plane = assets.load_asset('/Engine/BasicShapes/Plane')
    for actor in actors.get_all_level_actors():
        label = actor.get_actor_label()
        actor.modify()
        if label in ('Fen Bed', 'Combat Courtyard'):
            actor.static_mesh_component.set_material(0, wet)
        if isinstance(actor, unreal.DirectionalLight):
            light = actor.light_component
            fill = 'Fill' in label
            light.set_editor_property('intensity', 0.7 if fill else 1.3)
            light.set_light_color(unreal.LinearColor(0.66, 0.76, 0.84))
            light.set_editor_property('light_source_angle', 6.0)
        elif isinstance(actor, unreal.ExponentialHeightFog):
            actor.component.set_editor_property('fog_density', 0.032)
            actor.component.set_editor_property('fog_height_falloff', 0.16)
            actor.component.set_editor_property('fog_inscattering_luminance', unreal.LinearColor(0.18, 0.23, 0.26))
            actor.component.set_editor_property('start_distance', 900.0)
        elif isinstance(actor, unreal.PostProcessVolume):
            settings = actor.get_editor_property('settings')
            settings.set_editor_property('override_auto_exposure_bias', True)
            settings.set_editor_property('auto_exposure_bias', -0.65)
            settings.set_editor_property('override_vignette_intensity', True)
            settings.set_editor_property('vignette_intensity', 0.18)
            actor.set_editor_property('settings', settings)
        elif isinstance(actor, unreal.PointLight) and 'Ward Lantern' in label:
            actor.point_light_component.set_editor_property('use_temperature', False)
            actor.point_light_component.set_light_color(unreal.LinearColor(0.70, 0.85, 0.80))
    place(plane, 'Distant Marsh Water', (0, 400, -38), (160, 160, 1), mat=water)
    plan = planting_plan()
    for i, p in enumerate(plan):
        scale = (p['scale'],)*3
        rotation = (0, p['yaw'], 0)
        if p['kind'] == 'tree':
            place(meshes['AlderWood'], 'Alder Wood '+str(i), p['position'], scale, rotation, bark)
            place(meshes['AlderLeaves'], 'Alder Canopy '+str(i), p['position'], scale, rotation, leaf)
        else:
            key = 'ReedClump' if p['kind'] == 'reed' else 'FernClump'
            place(meshes[key], 'Marsh Plant '+str(i), p['position'], scale, rotation, reed if key == 'ReedClump' else leaf)
    rng = random.Random(1666)
    rocks = source_meshes('rock_moss_set_01')
    if not rocks:
        raise RuntimeError('Verified moss rock source missing')
    for i in range(66):
        side = -1 if i%2 else 1
        x, y = side*rng.uniform(1530, 3600), rng.uniform(-2700, 3100)
        fit(rocks[i%len(rocks)], 'Moss Bank '+str(i), x, y, -35, rng.uniform(100, 350), rng.uniform(0, 360))
    for i in range(130):
        x, y = rng.uniform(-1150, 1150), rng.uniform(-580, 1450)
        s = rng.uniform(0.07, 0.30)
        place(meshes['SlatePatch'], 'Ground Slate '+str(i), (x, y, 0.35), (s, s*rng.uniform(0.5, 1), 0.12), (0, rng.uniform(0, 360), 0), slate)
    for i, (x, y, sx, sy) in enumerate([(-790,-190,2.3,0.8),(720,180,1.9,0.65),(-780,520,1.2,0.9),(720,1050,2.5,0.7),(-560,1330,1.5,0.55)]):
        place(meshes['SlatePatch'], 'Irregular Puddle '+str(i), (x, y, 0.55), (sx, sy, 0.04), (0, i*37, 0), water)
    for i in range(20):
        y = 2070+i*92
        place(cube, 'Distant Causeway '+str(i), (0, y, -10), (4.5, 0.86, 0.4), (0, rng.uniform(-2, 2), 0), stone)
        for side in (-1, 1):
            if rng.random() > 0.2:
                place(cube, 'Causeway Parapet '+str(i)+' '+str(side), (side*235, y, 40), (0.4, 0.82, rng.uniform(0.7, 1.2)), mat=stone)
    for side in (-1, 1):
        place(cube, 'Belfry Pier '+str(side), (side*200, 4620, 720), (1.25, 1.9, 15.2), mat=stone)
        place(cube, 'Belfry Side '+str(side), (side*260, 4800, 500), (0.65, 4.2, 10.8), mat=stone)
    place(cube, 'Belfry Lower Wall', (0, 4880, 470), (5.3, 0.7, 10.2), mat=stone)
    place(cube, 'Belfry Lintel', (0, 4620, 1430), (5.4, 1.95, 1.3), mat=stone)
    for i in range(6):
        place(cube, 'Broken Belfry Crown '+str(i), (-240+i*94, 4640, 1510), (0.75, 1.5, rng.uniform(0.5, 1.4)), (0, rng.uniform(-6, 6), rng.uniform(-3, 3)), stone)
    for i in range(24):
        side = -1 if i%2 else 1
        fit(rocks[i%len(rocks)], 'Ruined Wall '+str(i), side*(500+rng.uniform(0, 100)), 2450+i*70, -20, rng.uniform(140, 220), rng.uniform(0, 360))
    for i in range(22):
        side = -1 if i%2 else 1
        x, y = side*rng.uniform(1250, 1430), rng.uniform(300, 1400)
        place(cube, 'Split Timber '+str(i), (x, y, 8), (rng.uniform(0.07, 0.14), rng.uniform(0.7, 1.7), 0.045), (0, rng.uniform(0, 360), 0), wood)
    result['plantings'] = len(plan)
    result['encounter_before'] = before
    if encounter_signature() != before:
        raise RuntimeError('Gameplay encounter changed')
    generated_actors = [a for a in actors.get_all_level_actors() if a.get_actor_label().startswith('Realistic Fen ')]
    for actor in generated_actors:
        actor.set_actor_enable_collision(False)
        actor.static_mesh_component.set_collision_profile_name('NoCollision')
        actor.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    if any(a.static_mesh_component.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION for a in generated_actors):
        raise RuntimeError('Environment dressing gained collision')
    result['decor_actors'] = len(generated_actors)
    assets.save_directory(destination, only_if_is_dirty=False, recursive=True)
    if not levels.save_current_level() or not levels.load_level(target):
        raise RuntimeError('Candidate map persistence failed')
    if encounter_signature() != before:
        raise RuntimeError('Saved encounter bindings changed')
    result['passed'] = True
except Exception:
    result['error'] = traceback.format_exc()
finally:
    result['complete'] = True
    (root/'IntegrationEvidence/realistic-scene.json').write_text(json.dumps(result, indent=2))
if not result['passed']:
    raise RuntimeError(result['error'])
