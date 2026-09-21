# Verifies the ReefM1 level has the actors and settings the M1 underwater
# scene needs. Prints SCENE_OK on success; raises AssertionError otherwise.
#
# Run headless:
#   UnrealEditor-Cmd Aquarium.uproject -run=pythonscript -script=Scripts/verify_scene.py
import unreal

MAP = "/Game/Maps/ReefM1"
SAND_MATERIAL = "/Game/Env/M_Sand"
GOBO_MATERIAL = "/Game/Env/M_SurfaceGobo"
GOBO_COARSE_MATERIAL = "/Game/Env/M_SurfaceGoboCoarse"
SNOW_MATERIAL = "/Game/Env/M_MarineSnow"
SNOW_CURTAIN_TAGS = ("near", "mid", "far")

# Must mirror the tuning block in build_reef_m1.py.
SCHOOL_COUNT = 36
SCHOOL_SPECIES_MIN = 5
SCHOOL_SCALE = (0.75, 1.3)
PROP_COUNT = 22
PROP_CLEAR_RADIUS_Y = 60.0
PROP_CLEAR_TAPER = 0.10
PROP_CLEAR_NEAR_X = 150.0
PROP_MESH_NAMES = {"SM_BranchCoral", "SM_PlateCoral", "SM_BrainCoral", "SM_FanCoral",
                   "SM_TubeCoral", "SM_boulder_01", "SM_rock_07", "SM_rock_09"}
PROP_TILT_DEG = 7.0
PROP_Z_STRETCH = (0.85, 1.20)

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
        "ExponentialHeightFog": 1,
        # floor + fine gobo + coarse gobo + snow curtains + props
        "StaticMeshActor": 3 + len(SNOW_CURTAIN_TAGS) + PROP_COUNT,
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

# Surface gobo: the invisible waterline shadow caster (M4b). It must never be drawn but must
# still cast a shadow, otherwise the volumetric fog has no contrast and the god rays vanish.
# There are TWO of them: a fine plane low over the reef whose shadow is the sand's ripple
# pattern, and a coarse plane far above it whose only job is volumetric contrast for the god
# rays. Each must carry its OWN material -- pointing both at the same one silently collapses
# the scene back to the single-layer version that could only do one of the two jobs.
GOBOS = {"SurfaceGobo": GOBO_MATERIAL, "SurfaceGoboCoarse": GOBO_COARSE_MATERIAL}
gobos = [a for a in smas if a.get_actor_label() in GOBOS]
assert len(gobos) == len(GOBOS), \
    "expected %d gobo planes %s, got %s" % (len(GOBOS), sorted(GOBOS),
                                            sorted(a.get_actor_label() for a in gobos))
gobo_z = {}
for gobo in gobos:
    label = gobo.get_actor_label()
    ggc = gobo.static_mesh_component
    assert ggc.static_mesh is not None, "%s has no static mesh" % label
    assert not ggc.is_visible(), "%s must be invisible" % label
    assert ggc.get_editor_property("cast_hidden_shadow"), "%s must cast a hidden shadow" % label
    assert ggc.get_editor_property("cast_shadow"), "%s must cast a shadow" % label
    gobo_mat = ggc.get_material(0)
    # Exact asset name, not startswith: "M_SurfaceGobo" is a prefix of "M_SurfaceGoboCoarse".
    assert gobo_mat is not None and gobo_mat.get_path_name().split(".")[0] == GOBOS[label], \
        "%s material is not %s: %s" % (label, GOBOS[label],
                                       gobo_mat and gobo_mat.get_path_name())
    gobo_z[label] = gobo.get_actor_location().z
assert gobo_z["SurfaceGoboCoarse"] > gobo_z["SurfaceGobo"] + 100.0, \
    "the coarse gobo must sit well above the fine one (no z-fighting, wider penumbra): %s" % gobo_z

# Marine snow curtains (M4b Task 7): three translucent unlit planes standing across the fixed
# camera's view. They must be visible (they ARE the effect), must carry a MI_MarineSnow_*
# instance parented to M_MarineSnow, and must cast nothing -- a shadow-casting translucent
# curtain would darken the whole reef behind it.
curtains = [a for a in smas if a.get_actor_label().startswith("SnowCurtain_")]
assert len(curtains) == len(SNOW_CURTAIN_TAGS), \
    "expected %d snow curtains, got %d" % (len(SNOW_CURTAIN_TAGS), len(curtains))
curtain_tags = set()
for curtain in curtains:
    label = curtain.get_actor_label()
    curtain_tags.add(label[len("SnowCurtain_"):])
    cgc = curtain.static_mesh_component
    assert cgc.static_mesh is not None, "%s has no static mesh" % label
    assert cgc.is_visible(), "%s must be visible" % label
    assert not cgc.get_editor_property("cast_shadow"), "%s must not cast a shadow" % label
    cmat = cgc.get_material(0)
    assert isinstance(cmat, unreal.MaterialInstanceConstant), \
        "%s material is not a MaterialInstanceConstant: %s" % (label, cmat)
    parent = cmat.get_editor_property("parent")
    assert parent is not None and parent.get_path_name().startswith(SNOW_MATERIAL), \
        "%s instance is not parented to M_MarineSnow: %s" % (label, parent)
assert curtain_tags == set(SNOW_CURTAIN_TAGS), \
    "snow curtain tags are %s, expected %s" % (sorted(curtain_tags), sorted(SNOW_CURTAIN_TAGS))

# Reef props: every other StaticMeshActor. Known meshes, clear of the camera lane.
props = [a for a in smas if a is not floor and a not in gobos and a not in curtains]
assert len(props) == PROP_COUNT, "expected %d reef props, got %d" % (PROP_COUNT, len(props))
prop_mesh_names = set()
tint_names = set()
for prop in props:
    label = prop.get_actor_label()
    mesh = prop.static_mesh_component.static_mesh
    assert mesh is not None, "%s has no static mesh" % label
    mesh_name = mesh.get_path_name().rsplit("/", 1)[-1].split(".")[-1]
    assert mesh_name in PROP_MESH_NAMES, "%s uses unexpected mesh %s" % (label, mesh_name)
    prop_mesh_names.add(mesh_name)
    loc = prop.get_actor_location()
    lane = PROP_CLEAR_RADIUS_Y + (loc.x - PROP_CLEAR_NEAR_X) * PROP_CLEAR_TAPER
    # The lane must be clear of the prop's BODY, not just its pivot. Checking the pivot alone
    # let a boulder with a 137 cm radius stand centred 65 cm off the axis and swallow the
    # player's fish at X=220 in the M4b review frame. The radius is taken from the mesh's own
    # bounds rather than from a table copied out of build_reef_m1.py, so the two cannot drift
    # apart silently. The tilt (up to PROP_TILT_DEG) widens the footprint by well under the
    # build script's clearance margin, hence the small tolerance.
    extent = mesh.get_bounds().box_extent
    radius = max(extent.x, extent.y) * abs(prop.get_actor_scale3d().x)
    assert abs(loc.y) >= lane + radius - 2.0, \
        "%s at (%.1f, %.1f) r=%.1f blocks the camera lane (half-width %.1f, needs |y| >= %.1f)" \
        % (label, loc.x, loc.y, radius, lane, lane + radius)
    # M4b Task 8: per-instance variation. A tint instance must be assigned (the plain material
    # would mean every prop of a type is the same colour) and the actor must actually be tilted
    # and stretched, within the tuned limits.
    pmat = prop.static_mesh_component.get_material(0)
    assert isinstance(pmat, unreal.MaterialInstanceConstant), \
        "%s has no tint material instance: %s" % (label, pmat)
    tint_names.add(pmat.get_path_name().rsplit("/", 1)[-1].split(".")[-1])
    rot = prop.get_actor_rotation()
    assert abs(rot.pitch) <= PROP_TILT_DEG + 1e-3 and abs(rot.roll) <= PROP_TILT_DEG + 1e-3, \
        "%s tilt out of range: pitch=%.2f roll=%.2f" % (label, rot.pitch, rot.roll)
    sc3 = prop.get_actor_scale3d()
    ratio = sc3.z / sc3.x
    assert PROP_Z_STRETCH[0] - 1e-3 <= ratio <= PROP_Z_STRETCH[1] + 1e-3, \
        "%s Z stretch out of range: %.3f" % (label, ratio)
assert prop_mesh_names == PROP_MESH_NAMES, \
    "not every prop mesh is represented: %s" % sorted(PROP_MESH_NAMES - prop_mesh_names)
# The whole point of the tint instances is variation: if every prop landed on _v0 the extra
# instances exist but nothing in the frame looks different.
tint_suffixes = {n.rsplit("_", 1)[-1] for n in tint_names}
assert len(tint_suffixes) >= 2, "props use only one tint variant: %s" % sorted(tint_names)

# Post-process grade (M4b): unbound, and every field it sets must have its override_ flag on,
# because a PostProcessSettings field with the flag left False is silently ignored.
ppvs = by_class.get("PostProcessVolume", [])
assert len(ppvs) == 1, "expected exactly one PostProcessVolume, got %d" % len(ppvs)
ppv = ppvs[0]
assert ppv.get_editor_property("unbound"), "the grade volume must be unbound"
pps = ppv.get_editor_property("settings")
for field in ("color_saturation", "color_contrast", "white_temp", "bloom_intensity",
              "bloom_threshold", "ambient_occlusion_intensity", "ambient_occlusion_radius",
              "depth_of_field_fstop", "depth_of_field_focal_distance"):
    assert pps.get_editor_property("override_" + field), "grade: override_%s is off" % field
# Depth of field stays off: the camera is fixed and the child's own fish is the nearest thing
# on screen, so a blur would land on exactly the subject.
assert pps.get_editor_property("depth_of_field_fstop") >= 32.0 - 1e-3, "depth of field is on"
assert abs(pps.get_editor_property("depth_of_field_focal_distance")) < 1e-3, "depth of field is on"

# World settings: no per-map game mode override (project default AAquariumGameMode applies)
world = ues.get_editor_world()
ws_list = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.WorldSettings)
assert len(ws_list) == 1, "expected exactly one WorldSettings, got %d" % len(ws_list)
assert ws_list[0].get_editor_property("default_game_mode") is None, "map overrides the default game mode"

print("SCENE_OK actors=%d fish=%d props=%d species=%d curtains=%d tints=%d"
      % (len(actors), len(fishes), len(props), len(mesh_names),
         len(curtains), len(tint_names)))
