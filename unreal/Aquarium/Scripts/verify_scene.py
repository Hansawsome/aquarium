# Verifies the ReefM1 level has the actors and settings the M1 underwater
# scene needs. Prints SCENE_OK on success; raises AssertionError otherwise.
#
# Run headless:
#   UnrealEditor-Cmd Aquarium.uproject -run=pythonscript -script=Scripts/verify_scene.py
import unreal

MAP = "/Game/Maps/ReefM1"
SAND_MATERIAL = "/Game/Env/M_Sand"

# Must mirror the tuning block in build_reef_m1.py.
SCHOOL_COUNT = 36
SCHOOL_SPECIES_MIN = 5
SCHOOL_SCALE = (0.75, 1.3)
PROP_COUNT = 14
PROP_CLEAR_RADIUS_Y = 60.0
PROP_CLEAR_TAPER = 0.10
PROP_CLEAR_NEAR_X = 150.0
PROP_MESH_NAMES = {"SM_BranchCoral", "SM_PlateCoral", "SM_BrainCoral",
                   "SM_boulder_01", "SM_rock_07", "SM_rock_09"}

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
        "ExponentialHeightFog": 1, "StaticMeshActor": 1 + PROP_COUNT,
        "FishActor": SCHOOL_COUNT}
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

# Background school: the full seeded count, several species, none flagged as the
# player's, all behind the player lane (game mode spawns the player at X = 220).
fishes = by_class["FishActor"]
assert len(fishes) == SCHOOL_COUNT, \
    "expected exactly %d background FishActors, got %d" % (SCHOOL_COUNT, len(fishes))
mesh_names = set()
for fish in fishes:
    label = fish.get_actor_label()
    assert not fish.get_editor_property("is_player_fish"), "%s is flagged is_player_fish" % label
    mesh = fish.get_editor_property("fish_mesh")
    assert mesh is not None, "%s has no mesh" % label
    mesh_names.add(mesh.get_path_name().rsplit("/", 1)[-1].split(".")[-1])
    body = fish.get_component_by_class(unreal.PoseableMeshComponent)
    assert body is not None, "%s has no PoseableMeshComponent" % label
    assert body.get_skinned_asset() is not None, "%s body has no skinned asset (invisible in editor)" % label
    ox = fish.get_editor_property("plane_origin").x
    assert ox >= 330.0, "%s plane_origin.x=%.1f < 330 (must sit behind the player lane)" % (label, ox)
    ax = fish.get_actor_location().x
    assert ax >= 300.0, "%s actor at x=%.1f < 300 (player lane must be free)" % (label, ax)
    s = fish.get_actor_scale3d()
    for axis, v in (("x", s.x), ("y", s.y), ("z", s.z)):
        assert SCHOOL_SCALE[0] - 1e-3 <= v <= SCHOOL_SCALE[1] + 1e-3, \
            "%s scale.%s=%.3f outside %s" % (label, axis, v, SCHOOL_SCALE)
assert len(mesh_names) >= SCHOOL_SPECIES_MIN, \
    "expected at least %d distinct fish meshes, got %s" % (SCHOOL_SPECIES_MIN, sorted(mesh_names))

# Floor: engine plane with the sand material
smas = by_class["StaticMeshActor"]
floors = [a for a in smas if a.get_actor_label() == "SandFloor"]
assert len(floors) == 1, "expected exactly one SandFloor, got %d" % len(floors)
floor = floors[0]
smc = floor.static_mesh_component
assert smc.static_mesh is not None, "floor has no static mesh"
floor_mat = smc.get_material(0)
assert floor_mat is not None, "floor has no material"
assert floor_mat.get_path_name().startswith(SAND_MATERIAL), "floor material is not M_Sand: %s" % floor_mat.get_path_name()

# Reef props: every other StaticMeshActor. Known meshes, clear of the camera lane.
props = [a for a in smas if a is not floor]
assert len(props) == PROP_COUNT, "expected %d reef props, got %d" % (PROP_COUNT, len(props))
prop_mesh_names = set()
for prop in props:
    label = prop.get_actor_label()
    mesh = prop.static_mesh_component.static_mesh
    assert mesh is not None, "%s has no static mesh" % label
    mesh_name = mesh.get_path_name().rsplit("/", 1)[-1].split(".")[-1]
    assert mesh_name in PROP_MESH_NAMES, "%s uses unexpected mesh %s" % (label, mesh_name)
    prop_mesh_names.add(mesh_name)
    loc = prop.get_actor_location()
    lane = PROP_CLEAR_RADIUS_Y + (loc.x - PROP_CLEAR_NEAR_X) * PROP_CLEAR_TAPER
    assert abs(loc.y) >= lane - 1.0, \
        "%s at (%.1f, %.1f) blocks the camera lane (half-width %.1f)" % (label, loc.x, loc.y, lane)
assert prop_mesh_names == PROP_MESH_NAMES, \
    "not every prop mesh is represented: %s" % sorted(PROP_MESH_NAMES - prop_mesh_names)

# World settings: no per-map game mode override (project default AAquariumGameMode applies)
world = ues.get_editor_world()
ws_list = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.WorldSettings)
assert len(ws_list) == 1, "expected exactly one WorldSettings, got %d" % len(ws_list)
assert ws_list[0].get_editor_property("default_game_mode") is None, "map overrides the default game mode"

print("SCENE_OK actors=%d fish=%d props=%d species=%d"
      % (len(actors), len(fishes), len(props), len(mesh_names)))
