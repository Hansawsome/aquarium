# Builds the ReefM1 underwater scene from scratch: sand textures + M_Sand,
# a procedural caustics light function (M_Caustics), and the level itself
# (sand floor, sun with caustics, sky light, dense blue-teal height fog with
# volumetric light shafts, the DiverCamera and one BlueTang FishActor).
# Idempotent: re-running replaces every asset in place (no *_1 duplicates).
#
# Run headless:
#   UnrealEditor-Cmd Aquarium.uproject -run=pythonscript -script=Scripts/build_reef_m1.py
import glob
import math
import os
import unreal

ROOT = os.path.abspath(os.path.join(unreal.Paths.project_dir(), "..", ".."))
SAND_DIR = os.path.join(ROOT, "assets", "textures", "sand")
ENV = "/Game/Env"
MAP = "/Game/Maps/ReefM1"
FISH_MESH = "/Game/Fish/BlueTang/SK_BlueTang"

# Scene layout (cm). Camera at diver eye height looking along +X; the fish
# swims on a YZ plane in front of it so it is seen side-on.
CAM_LOC = (0.0, 0.0, 130.0)
CAM_ROT = (-4.0, 0.0)           # pitch, yaw
SUN_ROT = (-65.0, 30.0)         # pitch, yaw
FISH_ORIGIN = (330.0, 0.0, 110.0)
FISH_HALF_W, FISH_HALF_H = 200.0, 100.0   # wander extents stay inside the 75 deg FOV

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


# --- textures ----------------------------------------------------------------
def import_tex(path, name):
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = ENV
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    tools.import_asset_tasks([task])
    paths = [str(p) for p in task.imported_object_paths]
    assert paths, "texture import failed: %s" % path
    tex = unreal.load_asset(paths[0])
    assert isinstance(tex, unreal.Texture2D), "not a Texture2D: %s" % paths
    return tex


def one(pattern):
    hits = glob.glob(os.path.join(SAND_DIR, pattern))
    assert len(hits) == 1, "expected one match for %s, got %s" % (pattern, hits)
    return hits[0]


diff = import_tex(one("*_diff_2k.png"), "T_Sand_D")
nor = import_tex(one("*_nor_gl_2k.png"), "T_Sand_N")
rough = import_tex(one("*_rough_2k.png"), "T_Sand_R")
diff.set_editor_property("srgb", True)
nor.set_editor_property("srgb", False)
nor.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
rough.set_editor_property("srgb", False)
rough.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
for t in (diff, nor, rough):
    eal.save_loaded_asset(t)


# --- materials ---------------------------------------------------------------
def material(name):
    """Load-and-clear if it exists, else create. Keeps the asset path stable."""
    path = ENV + "/" + name
    if eal.does_asset_exist(path):
        m = unreal.load_asset(path)
        mel.delete_all_material_expressions(m)
    else:
        m = tools.create_asset(name, ENV, unreal.Material, unreal.MaterialFactoryNew())
    return m


def const(m, x, y, value):
    n = mel.create_material_expression(m, unreal.MaterialExpressionConstant, x, y)
    n.set_editor_property("r", value)
    return n


# Sand: tiled diffuse / normal / roughness.
m_sand = material("M_Sand")
m_sand.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
coord = mel.create_material_expression(m_sand, unreal.MaterialExpressionTextureCoordinate, -900, 0)
# The floor plane is 100 cm scaled x40 = 40 m; one sand tile per ~1.6 m.
coord.set_editor_property("u_tiling", 25.0)
coord.set_editor_property("v_tiling", 25.0)


def sample(tex, y, sampler=None):
    n = mel.create_material_expression(m_sand, unreal.MaterialExpressionTextureSample, -600, y)
    n.set_editor_property("texture", tex)
    if sampler is not None:
        n.set_editor_property("sampler_type", sampler)
    mel.connect_material_expressions(coord, "", n, "UVs")
    return n


mel.connect_material_property(sample(diff, -200), "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(sample(nor, 100, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL), "RGB",
                              unreal.MaterialProperty.MP_NORMAL)
mel.connect_material_property(sample(rough, 400, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS), "R",
                              unreal.MaterialProperty.MP_ROUGHNESS)
mel.recompile_material(m_sand)
eal.save_loaded_asset(m_sand)

# Caustics light function. Note: MaterialExpressionNoise produced a flat result
# in the light-function domain on this setup, so the pattern is analytic.
def build_caustics(m):
    """Light function: three drifting sine waves over world XY, summed, sharpened.
    Bright ridges read as moving caustic patches; the lift keeps shadows from
    going fully black. Output roughly 0.25 .. 2.25."""
    E = unreal.MaterialProperty
    def node(cls, x, y):
        return mel.create_material_expression(m, cls, x, y)
    def const(x, y, v):
        n = node(unreal.MaterialExpressionConstant, x, y); n.set_editor_property("r", v); return n
    def mask(src, x, y, r, g):
        n = node(unreal.MaterialExpressionComponentMask, x, y)
        n.set_editor_property("r", r); n.set_editor_property("g", g)
        n.set_editor_property("b", False); n.set_editor_property("a", False)
        mel.connect_material_expressions(src, "", n, ""); return n
    def mul(a, b, x, y, a_out="", b_out=""):
        n = node(unreal.MaterialExpressionMultiply, x, y)
        mel.connect_material_expressions(a, a_out, n, "A"); mel.connect_material_expressions(b, b_out, n, "B"); return n
    def add(a, b, x, y):
        n = node(unreal.MaterialExpressionAdd, x, y)
        mel.connect_material_expressions(a, "", n, "A"); mel.connect_material_expressions(b, "", n, "B"); return n
    def sine(src, x, y):
        n = node(unreal.MaterialExpressionSine, x, y)
        mel.connect_material_expressions(src, "", n, ""); return n

    wpos = node(unreal.MaterialExpressionWorldPosition, -1800, 0)
    px = mask(wpos, -1600, -100, True, False)   # world X (cm)
    py = mask(wpos, -1600, 100, False, True)    # world Y (cm)
    t = node(unreal.MaterialExpressionTime, -1600, 300)
    # Wave phases: k * axis + drift * time. Sine node period=1 -> sin(2*pi*x),
    # so k = 1/period_cm. Periods ~45-70 cm give hand-sized patches.
    waves = []
    # Four waves with non-commensurate wavelengths and directions so the sum
    # never settles into a visible lattice.
    for i, (period, angle, drift) in enumerate([(52.0, 12.0, 0.11), (67.0, 71.0, -0.08), (89.0, 137.0, 0.14), (113.0, 203.0, -0.06)]):
        y = -400 + i * 220
        kx = math.cos(math.radians(angle)) / period
        ky = math.sin(math.radians(angle)) / period
        ax = mul(px, const(-1400, y - 60, kx), -1250, y - 60)
        ay = mul(py, const(-1400, y, ky), -1250, y)
        at = mul(t, const(-1400, y + 60, drift), -1250, y + 60)
        waves.append(sine(add(add(ax, ay, -1100, y), at, -950, y), -800, y))
    s = add(add(add(waves[0], waves[1], -700, -100), waves[2], -600, -100), waves[3], -500, -100)
    # Normalize sum of four sines (-4..4) to 0..1, then sharpen.
    n = add(mul(s, const(-500, 100, 1 / 8.0), -350, -100), const(-350, 100, 0.5), -200, -100)
    sat = node(unreal.MaterialExpressionSaturate, -50, -100)
    mel.connect_material_expressions(n, "", sat, "")
    pw = node(unreal.MaterialExpressionPower, 100, -100)
    mel.connect_material_expressions(sat, "", pw, "Base"); mel.connect_material_expressions(const(-50, 100, 6.0), "", pw, "Exp")
    out = add(mul(pw, const(100, 100, 2.5), 250, -100), const(250, 100, 0.3), 400, -100)
    mel.connect_material_property(out, "", E.MP_EMISSIVE_COLOR)


m_caus = material("M_Caustics")
m_caus.set_editor_property("material_domain", unreal.MaterialDomain.MD_LIGHT_FUNCTION)
build_caustics(m_caus)
mel.recompile_material(m_caus)
eal.save_loaded_asset(m_caus)


# --- level -------------------------------------------------------------------
# Reuse the existing map (keeps the asset path stable, no ReefM1_1 duplicates)
# but clear every actor so stale ones never accumulate across runs.
if eal.does_asset_exist(MAP):
    assert les.load_level(MAP), "load_level failed"
    old = eas.get_all_level_actors()
    if old:
        eas.destroy_actors(old)
else:
    assert les.new_level(MAP), "new_level failed"


def spawn(cls, loc, rot=(0.0, 0.0), label=None):
    # NB: unreal.Rotator's positional order is (roll, pitch, yaw); use keywords.
    rotator = unreal.Rotator(roll=0.0, pitch=rot[0], yaw=rot[1])
    a = eas.spawn_actor_from_class(cls, unreal.Vector(*loc), rotator)
    assert a is not None, "spawn failed: %s" % cls
    if label:
        a.set_actor_label(label)
    return a


# z=+1 keeps the editor grid (drawn at z=0) from z-fighting the sand in captures.
floor = spawn(unreal.StaticMeshActor, (0, 0, 1), label="SandFloor")
smc = floor.static_mesh_component
smc.set_mobility(unreal.ComponentMobility.STATIC)
assert smc.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane")), "floor mesh"
smc.set_material(0, m_sand)
floor.set_actor_scale3d(unreal.Vector(40, 40, 1))

sun = spawn(unreal.DirectionalLight, (0, 0, 1000), SUN_ROT, label="Sun")
sc = sun.light_component
sc.set_mobility(unreal.ComponentMobility.MOVABLE)   # light function + volumetric shafts
sc.set_intensity(8.0)
sc.set_light_color(unreal.LinearColor(0.55, 0.85, 1.0))
sc.set_editor_property("volumetric_scattering_intensity", 8.0)
sc.set_editor_property("light_function_material", m_caus)
sc.set_editor_property("light_function_scale", unreal.Vector(400, 400, 400))
sc.set_editor_property("cast_volumetric_shadow", True)

sky = spawn(unreal.SkyLight, (0, 0, 500), label="Sky")
kc = sky.light_component
kc.set_mobility(unreal.ComponentMobility.MOVABLE)
# A specified cubemap gives stable blue ambient without any sky geometry to
# capture (there is no atmosphere in this scene; the far background is fog).
kc.set_editor_property("source_type", unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
cube = unreal.load_asset("/Engine/MapTemplates/Sky/DaylightAmbientCubemap")
assert isinstance(cube, unreal.TextureCube), "sky cubemap missing"
kc.set_editor_property("cubemap", cube)
kc.set_intensity(0.8)
kc.set_light_color(unreal.LinearColor(0.35, 0.65, 0.9))
kc.set_editor_property("volumetric_scattering_intensity", 0.5)
kc.recapture_sky()

fog = spawn(unreal.ExponentialHeightFog, (0, 0, 0), label="Water")
fc = fog.component
# Measured in-editor (auto exposure is off in this project): density 1.5 barely
# tints the floor at 20 m; 5.0 gives ~40% haze on the fish at 3.3 m and a fully
# fogged background by ~15 m. Falloff 0 = uniform density (we are inside water).
fc.set_editor_property("fog_density", 5.0)
fc.set_editor_property("fog_height_falloff", 0.0)
fc.set_editor_property("fog_max_opacity", 1.0)
fc.set_editor_property("start_distance", 0.0)
fc.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.02, 0.18, 0.30))
fc.set_editor_property("directional_inscattering_luminance", unreal.LinearColor(0.10, 0.35, 0.45))
fc.set_editor_property("enable_volumetric_fog", True)
fc.set_editor_property("volumetric_fog_scattering_distribution", 0.6)
# NB: unreal.Color positional order is (b, g, r, a); use keywords.
fc.set_editor_property("volumetric_fog_albedo", unreal.Color(r=30, g=140, b=200, a=255))
fc.set_editor_property("volumetric_fog_extinction_scale", 1.0)
fc.set_editor_property("volumetric_fog_distance", 6000.0)

cam = spawn(unreal.CameraActor, CAM_LOC, CAM_ROT, label="DiverCamera")
cam.tags = [unreal.Name("DiverCamera")]
cam.camera_component.set_field_of_view(75.0)

fish_cls = unreal.load_class(None, "/Script/Aquarium.FishActor")
assert fish_cls is not None, "FishActor class not found (is the C++ module built?)"
# Yaw 90 shows the fish side-on in editor stills; at runtime facing follows velocity.
fish = spawn(fish_cls, FISH_ORIGIN, (0.0, 90.0), label="BlueTang")
sk = unreal.load_asset(FISH_MESH)
assert isinstance(sk, unreal.SkeletalMesh), "fish mesh missing: %s" % FISH_MESH
fish.set_editor_property("fish_mesh", sk)
fish.set_editor_property("plane_origin", unreal.Vector(*FISH_ORIGIN))
fish.set_editor_property("seed", 7)
fish.set_editor_property("plane_half_width", FISH_HALF_W)
fish.set_editor_property("plane_half_height", FISH_HALF_H)
# The actor applies FishMesh to its body only at BeginPlay; push it now so the
# fish is visible in the editor / review captures too.
body = fish.get_component_by_class(unreal.PoseableMeshComponent)
assert body is not None, "fish has no PoseableMeshComponent"
body.set_skinned_asset_and_update(sk)
body.set_visibility(True)

assert les.save_current_level(), "save_current_level failed"
print("REEF_OK actors=%d map=%s" % (len(eas.get_all_level_actors()), MAP))
