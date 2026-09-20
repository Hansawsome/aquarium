# Verifies the ReefM1 level has the actors and settings the M1 underwater
# scene needs. Prints SCENE_OK on success; raises AssertionError otherwise.
#
# Run headless:
#   UnrealEditor-Cmd Aquarium.uproject -run=pythonscript -script=Scripts/verify_scene.py
import unreal

MAP = "/Game/Maps/ReefM1"
SAND_MATERIAL = "/Game/Env/M_Sand"

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

assert unreal.EditorAssetLibrary.does_asset_exist(MAP), "ReefM1 does not load"
assert les.load_level(MAP), "ReefM1 does not load"

actors = eas.get_all_level_actors()
by_class = {}
for a in actors:
    by_class.setdefault(a.get_class().get_name(), []).append(a)

need = {"CameraActor": 1, "DirectionalLight": 1, "SkyLight": 1,
        "ExponentialHeightFog": 1, "StaticMeshActor": 1, "FishActor": 1}
missing = [k for k, n in need.items() if len(by_class.get(k, [])) < n]
assert not missing, "missing actors: %s" % missing

# Camera
cam = by_class["CameraActor"][0]
assert "DiverCamera" in [str(t) for t in cam.tags], "camera lacks DiverCamera tag"

# Sun: movable, caustics light function, drives volumetric shafts
sun = by_class["DirectionalLight"][0].light_component
assert sun.get_editor_property("mobility") == unreal.ComponentMobility.MOVABLE, "sun is not Movable"
assert sun.get_editor_property("light_function_material") is not None, "sun has no light function (caustics)"
assert sun.get_editor_property("volumetric_scattering_intensity") > 0, "sun volumetric scattering is 0 (no light shafts)"

# Sky light: specified cubemap must actually have one (else ambient is black)
sky = by_class["SkyLight"][0].light_component
if sky.get_editor_property("source_type") == unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP:
    assert sky.get_editor_property("cubemap") is not None, "sky light uses a specified cubemap but none is set"

# Fog: dense enough for an underwater look, volumetric for shafts
fog = by_class["ExponentialHeightFog"][0].get_editor_property("component")
assert fog.get_editor_property("fog_density") >= 2.0, "fog too thin for underwater look (density < 2.0)"
assert fog.get_editor_property("enable_volumetric_fog"), "volumetric fog disabled (no light shafts)"

# Fish: mesh property set and actually pushed onto the poseable body
fish = by_class["FishActor"][0]
assert fish.get_editor_property("fish_mesh") is not None, "fish has no mesh"
body = fish.get_component_by_class(unreal.PoseableMeshComponent)
assert body is not None, "fish has no PoseableMeshComponent"
assert body.get_skinned_asset() is not None, "fish body has no skinned asset (invisible in editor)"

# Floor: engine plane with the sand material
floor = by_class["StaticMeshActor"][0]
smc = floor.static_mesh_component
assert smc.static_mesh is not None, "floor has no static mesh"
floor_mat = smc.get_material(0)
assert floor_mat is not None, "floor has no material"
assert floor_mat.get_path_name().startswith(SAND_MATERIAL), "floor material is not M_Sand: %s" % floor_mat.get_path_name()

# World settings: no per-map game mode override (project default AAquariumGameMode applies)
world = ues.get_editor_world()
ws_list = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.WorldSettings)
assert len(ws_list) == 1, "expected exactly one WorldSettings, got %d" % len(ws_list)
assert ws_list[0].get_editor_property("default_game_mode") is None, "map overrides the default game mode"

print("SCENE_OK actors=%d classes=%s" % (len(actors), sorted(by_class.keys())))
