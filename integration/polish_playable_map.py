import json
import math
import random
import traceback
from pathlib import Path
import unreal

result={'complete':False,'passed':False}
try:
    levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not levels.load_level('/Game/WidowfenPrep/LVL_DarkRelicPlayable'):
        raise RuntimeError('Playable map missing')
    assets=unreal.EditorAssetLibrary
    for actor in actors.get_all_level_actors():
        if actor.get_actor_label().startswith('Playable Polish'):
            actors.destroy_actor(actor)
    post=actors.spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector(0,0,0))
    post.set_actor_label('Playable Polish Exposure')
    post.set_editor_property('unbound',True)
    settings=post.get_editor_property('settings')
    settings.set_editor_property('override_auto_exposure_bias',True)
    settings.set_editor_property('auto_exposure_bias',-1.7)
    settings.set_editor_property('override_vignette_intensity',True)
    settings.set_editor_property('vignette_intensity',0.35)
    settings.set_editor_property('override_motion_blur_amount',True)
    settings.set_editor_property('motion_blur_amount',0.0)
    post.set_editor_property('settings',settings)
    for actor in actors.get_all_level_actors():
        if isinstance(actor,unreal.DirectionalLight):
            actor.modify()
            actor.light_component.set_editor_property('intensity',1.2)
            actor.light_component.set_editor_property('light_color',unreal.Color(175,195,210,255))
        if isinstance(actor,unreal.ExponentialHeightFog):
            actor.modify()
            actor.component.set_editor_property('fog_density',0.055)
        if isinstance(actor,unreal.PointLight):
            actor.modify()
            actor.point_light_component.set_editor_property('intensity',60.0)
    material=assets.load_asset('/Game/WidowfenPrep/Materials/M_dark_wooden_planks')
    cylinder=assets.load_asset('/Engine/BasicShapes/Cylinder')
    random.seed(1193)
    for i in range(38):
        angle=i*math.tau/38
        radius=random.uniform(2450,3200)
        x,y=math.cos(angle)*radius,math.sin(angle)*radius+400
        height=random.uniform(500,1100)
        tree=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(x,y,height/2-40))
        tree.set_actor_label('Playable Polish Deadwood '+str(i))
        tree.static_mesh_component.set_static_mesh(cylinder)
        tree.static_mesh_component.set_material(0,material)
        tree.set_actor_scale3d(unreal.Vector(0.28,0.28,height/100))
        for side in [-1,1]:
            branch=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(x+side*80,y,height*0.66),unreal.Rotator(side*45,i*27,0))
            branch.set_actor_label('Playable Polish Branch '+str(i))
            branch.static_mesh_component.set_static_mesh(cylinder)
            branch.static_mesh_component.set_material(0,material)
            branch.set_actor_scale3d(unreal.Vector(0.11,0.11,height/240))
    result['actors']=len(actors.get_all_level_actors())
    if not levels.save_current_level(): raise RuntimeError('Map save failed')
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
    result['passed']=True
except Exception:
    result['error']=traceback.format_exc()
finally:
    result['complete']=True
    Path(unreal.Paths.project_dir(),'IntegrationEvidence','playable-polish.json').write_text(json.dumps(result,indent=2))
if not result['passed']: raise RuntimeError(result['error'])
