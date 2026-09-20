# Verifies the ReefM1 level has the actors the M1 underwater scene needs.
# Prints SCENE_OK on success; raises AssertionError otherwise.
#
# Run headless:
#   UnrealEditor-Cmd Aquarium.uproject -run=pythonscript -script=Scripts/verify_scene.py
import unreal

MAP = "/Game/Maps/ReefM1"

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

assert unreal.EditorAssetLibrary.does_asset_exist(MAP), "ReefM1 does not load"
ok = les.load_level(MAP)
assert ok, "ReefM1 does not load"

actors = eas.get_all_level_actors()
by_class = {}
for a in actors:
    by_class.setdefault(a.get_class().get_name(), []).append(a)

need = {"CameraActor": 1, "DirectionalLight": 1, "SkyLight": 1,
        "ExponentialHeightFog": 1, "StaticMeshActor": 1, "FishActor": 1}
missing = [k for k, n in need.items() if len(by_class.get(k, [])) < n]
assert not missing, "missing actors: %s" % missing

cam = by_class["CameraActor"][0]
assert "DiverCamera" in [str(t) for t in cam.tags], "camera lacks DiverCamera tag"

fog = by_class["ExponentialHeightFog"][0].get_editor_property("component")
assert fog.get_editor_property("fog_density") >= 0.02, "fog too thin for underwater look"

fish = by_class["FishActor"][0]
assert fish.get_editor_property("fish_mesh") is not None, "fish has no mesh"

floor = by_class["StaticMeshActor"][0]
smc = floor.static_mesh_component
assert smc.static_mesh is not None, "floor has no static mesh"
assert smc.get_material(0) is not None, "floor has no material"

print("SCENE_OK actors=%d classes=%s" % (len(actors), sorted(by_class.keys())))
