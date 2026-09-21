# Builds the ReefM1 underwater scene from scratch: sand textures + M_Sand,
# a procedural caustics light function (M_Caustics), and the level itself
# (sand floor, sun with caustics, sky light, dense blue-teal height fog with
# volumetric light shafts, the DiverCamera, a seeded background fish school and
# a seeded scatter of reef props).
# Idempotent: re-running replaces every asset in place (no *_1 duplicates).
#
# Run headless:
#   UnrealEditor-Cmd Aquarium.uproject -run=pythonscript -script=Scripts/build_reef_m1.py
import glob
import math
import os
import random
import unreal

ROOT = os.path.abspath(os.path.join(unreal.Paths.project_dir(), "..", ".."))
SAND_DIR = os.path.join(ROOT, "assets", "textures", "sand")
ENV = "/Game/Env"
MAP = "/Game/Maps/ReefM1"
FISH_CLASS = "/Script/Aquarium.FishActor"
ENGINE_PLANE = "/Engine/BasicShapes/Plane"                          # 100 x 100 cm, +Z normal
ENGINE_SKY_CUBEMAP = "/Engine/MapTemplates/Sky/DaylightAmbientCubemap"

# --- tuning ------------------------------------------------------------------
# Units: cm, degrees, lux (sun) / cd/m^2 (sky). Auto exposure is disabled in
# this project (r.DefaultFeature.AutoExposure=False), so brightness is absolute.
#
# Camera at diver eye height looking along +X; fish swim on YZ planes in
# front of it so they are seen side-on.
CAM_LOC = (0.0, 0.0, 130.0)
CAM_ROT = (-4.0, 0.0)                   # pitch, yaw
CAM_FOV = 75.0
# Background school: generated from a fixed seed so the layout is reproducible.
# All fish sit at X >= 330 so the player's fish (spawned by the game mode at
# X = 220) is nearer the camera and reads larger.
SCHOOL_SEED = 20260920
SCHOOL_COUNT = 36
# (mesh path, weight) — the small damselfish fills the background
SCHOOL_SPECIES = [
    ("/Game/Fish/Damselfish/SK_Damselfish",       0.40),
    ("/Game/Fish/BlueTang/SK_BlueTang",           0.15),
    ("/Game/Fish/Clownfish/SK_Clownfish",         0.15),
    ("/Game/Fish/YellowTang/SK_YellowTang",       0.15),
    ("/Game/Fish/Butterflyfish/SK_Butterflyfish", 0.15),
]
SCHOOL_X = (330.0, 700.0)      # behind the player's plane at X = 220
SCHOOL_Y = (-250.0, 250.0)
SCHOOL_Z = (110.0, 200.0)
SCHOOL_HALF_W = (120.0, 220.0)
SCHOOL_HALF_H = (40.0, 60.0)
SCHOOL_SCALE = (0.75, 1.3)
SCHOOL_SPEED = (25.0, 55.0)

PROP_SEED = 77
PROP_COUNT = 14
# (mesh path, scale range) — rock_09 is a 15 cm pebble, so it is scaled up a lot
PROP_MESHES = [
    ("/Game/Props/SM_BranchCoral", (0.7, 1.4)),
    ("/Game/Props/SM_PlateCoral",  (0.6, 1.2)),
    ("/Game/Props/SM_BrainCoral",  (0.7, 1.4)),
    ("/Game/Props/SM_boulder_01",  (0.8, 1.5)),
    ("/Game/Props/SM_rock_07",     (2.0, 4.0)),
    ("/Game/Props/SM_rock_09",     (4.0, 8.0)),
]
# Unscaled XY half-extents (cm) from the import log; the placement radius is
# max(x, y) * actor scale.
PROP_HALF_EXTENTS = {
    "SM_BranchCoral": (28.3, 22.2),
    "SM_PlateCoral":  (52.6, 52.8),
    "SM_BrainCoral":  (31.4, 31.4),
    "SM_boulder_01":  (63.6, 91.5),
    "SM_rock_07":     (8.4, 16.0),
    "SM_rock_09":     (3.7, 7.2),
}
PROP_X = (150.0, 800.0)
PROP_Y = (-400.0, 400.0)
PROP_SEPARATION = 0.9          # required gap as a fraction of the summed radii
PROP_SEPARATION_RELAXED = 0.75 # fallback when a prop cannot be placed
PROP_PLACE_TRIES = 200
# The camera's forward lane stays clear of props; it widens with distance so a
# far prop does not occlude the school either.
PROP_CLEAR_RADIUS_Y = 60.0     # lane half-width at PROP_X[0]
PROP_CLEAR_TAPER = 0.10        # extra half-width per cm of X
PROP_CLEAR_MARGIN = 5.0        # pushed props clear the boundary by this much

FISH_EDITOR_YAW = 90.0                  # side-on in editor stills; runtime facing follows velocity

FLOOR_Z = 1.0                           # above the editor grid (z=0) so captures do not z-fight
FLOOR_SCALE = 40.0                      # 100 cm plane -> 40 m
SAND_TILING = 25.0                      # tiles across the floor -> one tile per ~1.6 m

SUN_ROT = (-65.0, 30.0)                 # pitch, yaw
SUN_INTENSITY = 14.0                   # light function peaks at 1.0, so the sun carries brightness
SUN_COLOR = dict(r=0.55, g=0.85, b=1.0)
SUN_VOLUMETRIC_SCATTERING = 14.0        # shafts need scattering AND an occluder (SurfaceGobo)

SKY_INTENSITY = 0.8
SKY_COLOR = dict(r=0.35, g=0.65, b=0.9)
SKY_VOLUMETRIC_SCATTERING = 0.5

# M4b: the M4a capture lost every colour by ~10 m, which is why the 36-fish school read as
# flat silhouettes. Density 2.2 with extinction 0.7 keeps species colour to about 20 m while
# still fogging the 40 m floor edge away. start_distance clears the nearest 1.5 m so the
# player's own fish stays crisp. Falloff 0 = uniform density (we are inside the water).
FOG_DENSITY = 2.2
FOG_HEIGHT_FALLOFF = 0.0
FOG_START_DISTANCE = 150.0
FOG_INSCATTER = dict(r=0.015, g=0.105, b=0.175)         # darker: far = deep water, not bright sky
FOG_DIRECTIONAL_INSCATTER = dict(r=0.12, g=0.38, b=0.48)
FOG_DIRECTIONAL_EXPONENT = 6.0                          # wider sun glow
FOG_ALBEDO = dict(r=30, g=140, b=200, a=255)            # volumetric scattering tint (8-bit)
FOG_SCATTERING_DISTRIBUTION = 0.6
FOG_EXTINCTION_SCALE = 0.7
FOG_VOLUMETRIC_DISTANCE = 6000.0

# Surface gobo: an invisible shadow caster at the waterline. Volumetric god rays are
# scattering intensity TIMES shadow contrast, and this scene had nothing above the camera to
# cast a shadow at all -- which is why "빛줄기 약함" survived every intensity increase in M1.
GOBO_Z = 1100.0
GOBO_SCALE = 60.0                       # 100 cm plane -> 60 m, wider than the 40 m floor
GOBO_THRESHOLD = 0.42                   # opacity-mask cutoff; higher = narrower, sharper shafts

# Post-process. Auto exposure is already off project-wide (r.DefaultFeature.AutoExposure=False),
# so these are absolute. Lowering the fog costs contrast; the grade puts it back.
PP_SATURATION = 1.15
PP_CONTRAST = 1.06
PP_TEMPERATURE = 5200.0                 # slightly warm, to offset the all-over blue cast
PP_BLOOM_INTENSITY = 0.5
PP_BLOOM_THRESHOLD = 1.0
PP_AO_INTENSITY = 0.5
PP_AO_RADIUS = 80.0

# Depth of field is OFF BY DEFAULT and explicitly overridden off, so a change to the project
# renderer settings cannot quietly turn it on. The camera is fixed and the thing the child is
# looking at -- their own fish -- sits close to the camera, so DoF risks blurring exactly the
# subject, and it costs frame time. To try it, set DOF to e.g.
# dict(fstop=2.8, focal_distance=220.0) and re-run this script; nothing else changes.
DOF = None

# Caustics: (period cm, direction deg, drift turns/s). Directions are chosen so
# no pair is within 30 deg of parallel or anti-parallel; periods are
# non-commensurate. Each wave's phase is warped by the previous wave's sine.
CAUSTIC_WAVES = [(52.0, 12.0, 0.11), (67.0, 71.0, -0.08), (89.0, 128.0, 0.14), (113.0, 160.0, -0.06)]
CAUSTIC_WARP = 2.0                      # phase warp amplitude, radians
CAUSTIC_SHARPEN = 6.0                   # power applied to the normalised sum
CAUSTIC_GAIN = 0.7                      # output = pow * GAIN + LIFT, kept within [0, 1]
CAUSTIC_LIFT = 0.3
# -----------------------------------------------------------------------------

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


# Sand: tiled diffuse / normal / roughness.
m_sand = material("M_Sand")
m_sand.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
coord = mel.create_material_expression(m_sand, unreal.MaterialExpressionTextureCoordinate, -900, 0)
coord.set_editor_property("u_tiling", SAND_TILING)
coord.set_editor_property("v_tiling", SAND_TILING)


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
    """Light function: four drifting sine waves over world XY, each phase-warped
    by the previous wave (sin(k2.p + d2.t + A*sin(k1.p + d1.t))), summed,
    normalised to [0, 1] and sharpened with a power curve. Bright ridges read
    as wobbly caustic patches; the lift keeps shadows from going fully black.
    Output range is [CAUSTIC_LIFT, CAUSTIC_LIFT + CAUSTIC_GAIN] = [0.3, 1.0];
    the sun intensity carries the brightness."""
    def node(cls, x, y):
        return mel.create_material_expression(m, cls, x, y)

    def const(x, y, v):
        n = node(unreal.MaterialExpressionConstant, x, y)
        n.set_editor_property("r", v)
        return n

    def mask(src, x, y, r, g):
        n = node(unreal.MaterialExpressionComponentMask, x, y)
        n.set_editor_property("r", r)
        n.set_editor_property("g", g)
        n.set_editor_property("b", False)
        n.set_editor_property("a", False)
        mel.connect_material_expressions(src, "", n, "")
        return n

    def mul(a, b, x, y):
        n = node(unreal.MaterialExpressionMultiply, x, y)
        mel.connect_material_expressions(a, "", n, "A")
        mel.connect_material_expressions(b, "", n, "B")
        return n

    def add(a, b, x, y):
        n = node(unreal.MaterialExpressionAdd, x, y)
        mel.connect_material_expressions(a, "", n, "A")
        mel.connect_material_expressions(b, "", n, "B")
        return n

    def sine(src, x, y):
        n = node(unreal.MaterialExpressionSine, x, y)   # period 1 -> sin(2*pi*x)
        mel.connect_material_expressions(src, "", n, "")
        return n

    wpos = node(unreal.MaterialExpressionWorldPosition, -2000, 0)
    px = mask(wpos, -1800, -100, True, False)   # world X (cm)
    py = mask(wpos, -1800, 100, False, True)    # world Y (cm)
    t = node(unreal.MaterialExpressionTime, -1800, 300)
    # Sine input is in turns, so k = 1/period_cm and the warp amplitude is
    # converted from radians to turns.
    warp_turns = CAUSTIC_WARP / (2.0 * math.pi)
    prev = None
    waves = []
    for i, (period, angle, drift) in enumerate(CAUSTIC_WAVES):
        y = -450 + i * 220
        kx = math.cos(math.radians(angle)) / period
        ky = math.sin(math.radians(angle)) / period
        ax = mul(px, const(-1600, y - 60, kx), -1450, y - 60)
        ay = mul(py, const(-1600, y, ky), -1450, y)
        at = mul(t, const(-1600, y + 60, drift), -1450, y + 60)
        phase = add(add(ax, ay, -1300, y), at, -1150, y)
        if prev is not None:
            phase = add(phase, mul(prev, const(-1150, y + 90, warp_turns), -1000, y + 60), -850, y)
        prev = sine(phase, -700, y)
        waves.append(prev)
    s = waves[0]
    for i, w in enumerate(waves[1:]):
        s = add(s, w, -550 + i * 100, -100)
    # Normalise the sum of N sines (-N..N) to 0..1, clamp, then sharpen.
    n_waves = float(len(waves))
    norm = add(mul(s, const(-250, 100, 1.0 / (2.0 * n_waves)), -150, -100), const(-150, 100, 0.5), -50, -100)
    sat = node(unreal.MaterialExpressionSaturate, 50, -100)
    mel.connect_material_expressions(norm, "", sat, "")
    pw = node(unreal.MaterialExpressionPower, 150, -100)
    mel.connect_material_expressions(sat, "", pw, "Base")
    mel.connect_material_expressions(const(50, 100, CAUSTIC_SHARPEN), "", pw, "Exp")
    out = add(mul(pw, const(150, 100, CAUSTIC_GAIN), 300, -100), const(300, 100, CAUSTIC_LIFT), 450, -100)
    mel.connect_material_property(out, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)


m_caus = material("M_Caustics")
m_caus.set_editor_property("material_domain", unreal.MaterialDomain.MD_LIGHT_FUNCTION)
build_caustics(m_caus)
mel.recompile_material(m_caus)
eal.save_loaded_asset(m_caus)



def build_surface_gobo(m):
    """Opacity-mask material for the invisible waterline plane. Reuses the caustics wave sum:
    where the sum is above GOBO_THRESHOLD the plane is solid and blocks the sun, elsewhere it is
    cut away and the sun gets through. The holes therefore drift with the same rhythm as the
    floor caustics, so the shafts and the floor patches move together. Unlit and masked; the
    plane is never drawn (bCastHiddenShadow), so its colour does not matter."""
    def node(cls, x, y):
        return mel.create_material_expression(m, cls, x, y)

    def const(x, y, v):
        n = node(unreal.MaterialExpressionConstant, x, y)
        n.set_editor_property("r", v)
        return n

    def mask(src, x, y, r, g):
        n = node(unreal.MaterialExpressionComponentMask, x, y)
        n.set_editor_property("r", r)
        n.set_editor_property("g", g)
        n.set_editor_property("b", False)
        n.set_editor_property("a", False)
        mel.connect_material_expressions(src, "", n, "")
        return n

    def mul(a, b, x, y):
        n = node(unreal.MaterialExpressionMultiply, x, y)
        mel.connect_material_expressions(a, "", n, "A")
        mel.connect_material_expressions(b, "", n, "B")
        return n

    def add(a, b, x, y):
        n = node(unreal.MaterialExpressionAdd, x, y)
        mel.connect_material_expressions(a, "", n, "A")
        mel.connect_material_expressions(b, "", n, "B")
        return n

    def sine(src, x, y):
        n = node(unreal.MaterialExpressionSine, x, y)
        mel.connect_material_expressions(src, "", n, "")
        return n

    wpos = node(unreal.MaterialExpressionWorldPosition, -2000, 0)
    px = mask(wpos, -1800, -100, True, False)
    py = mask(wpos, -1800, 100, False, True)
    t = node(unreal.MaterialExpressionTime, -1800, 300)
    waves = []
    for i, (period, angle, drift) in enumerate(CAUSTIC_WAVES[:3]):
        y = -450 + i * 220
        kx = math.cos(math.radians(angle)) / (period * 3.0)   # 3x coarser than the floor pattern
        ky = math.sin(math.radians(angle)) / (period * 3.0)
        ax = mul(px, const(-1600, y - 60, kx), -1450, y - 60)
        ay = mul(py, const(-1600, y, ky), -1450, y)
        at = mul(t, const(-1600, y + 60, drift * 0.5), -1450, y + 60)
        waves.append(sine(add(add(ax, ay, -1300, y), at, -1150, y), -700, y))
    s = waves[0]
    for i, w in enumerate(waves[1:]):
        s = add(s, w, -550 + i * 100, -100)
    n_waves = float(len(waves))
    norm = add(mul(s, const(-250, 100, 1.0 / (2.0 * n_waves)), -150, -100),
               const(-150, 100, 0.5), -50, -100)
    # opacity mask = saturate((norm - threshold) * 8): a hard-ish edge so the shafts have edges
    off = node(unreal.MaterialExpressionSubtract, 50, -100)
    mel.connect_material_expressions(norm, "", off, "A")
    mel.connect_material_expressions(const(50, 100, GOBO_THRESHOLD), "", off, "B")
    gain = mul(off, const(200, 100, 8.0), 300, -100)
    sat = node(unreal.MaterialExpressionSaturate, 450, -100)
    mel.connect_material_expressions(gain, "", sat, "")
    mel.connect_material_property(sat, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    mel.connect_material_property(const(450, 200, 0.0), "",
                                  unreal.MaterialProperty.MP_EMISSIVE_COLOR)


m_gobo = material("M_SurfaceGobo")
m_gobo.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
m_gobo.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
m_gobo.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
m_gobo.set_editor_property("two_sided", True)
build_surface_gobo(m_gobo)
mel.recompile_material(m_gobo)
eal.save_loaded_asset(m_gobo)


# --- level -------------------------------------------------------------------
# Reuse the existing map (keeps the asset path stable, no ReefM1_1 duplicates)
# but clear every actor so stale ones never accumulate across runs. Note this
# only resets actors: WorldSettings, sublevels and other per-map state persist
# from the previous run.
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


floor = spawn(unreal.StaticMeshActor, (0, 0, FLOOR_Z), label="SandFloor")
smc = floor.static_mesh_component
smc.set_mobility(unreal.ComponentMobility.STATIC)
assert smc.set_static_mesh(unreal.load_asset(ENGINE_PLANE)), "floor mesh missing: %s" % ENGINE_PLANE
smc.set_material(0, m_sand)
floor.set_actor_scale3d(unreal.Vector(FLOOR_SCALE, FLOOR_SCALE, 1))

sun = spawn(unreal.DirectionalLight, (0, 0, 1000), SUN_ROT, label="Sun")
sc = sun.light_component
sc.set_mobility(unreal.ComponentMobility.MOVABLE)   # light function + volumetric shafts
sc.set_intensity(SUN_INTENSITY)
sc.set_light_color(unreal.LinearColor(**SUN_COLOR))
sc.set_editor_property("volumetric_scattering_intensity", SUN_VOLUMETRIC_SCATTERING)
sc.set_editor_property("light_function_material", m_caus)   # graph reads WorldPosition; scale is irrelevant
sc.set_editor_property("cast_volumetric_shadow", True)

sky = spawn(unreal.SkyLight, (0, 0, 500), label="Sky")
kc = sky.light_component
kc.set_mobility(unreal.ComponentMobility.MOVABLE)
# A specified cubemap gives stable blue ambient without any sky geometry to
# capture (there is no atmosphere in this scene; the far background is fog).
kc.set_editor_property("source_type", unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
cube = unreal.load_asset(ENGINE_SKY_CUBEMAP)
assert isinstance(cube, unreal.TextureCube), "sky cubemap missing: %s" % ENGINE_SKY_CUBEMAP
kc.set_editor_property("cubemap", cube)
kc.set_intensity(SKY_INTENSITY)
kc.set_light_color(unreal.LinearColor(**SKY_COLOR))
kc.set_editor_property("volumetric_scattering_intensity", SKY_VOLUMETRIC_SCATTERING)
kc.recapture_sky()

fog = spawn(unreal.ExponentialHeightFog, (0, 0, 0), label="Water")
fc = fog.component
fc.set_editor_property("fog_density", FOG_DENSITY)
fc.set_editor_property("fog_height_falloff", FOG_HEIGHT_FALLOFF)
fc.set_editor_property("fog_max_opacity", 1.0)
fc.set_editor_property("start_distance", FOG_START_DISTANCE)
fc.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(**FOG_INSCATTER))
fc.set_editor_property("directional_inscattering_luminance", unreal.LinearColor(**FOG_DIRECTIONAL_INSCATTER))
fc.set_editor_property("directional_inscattering_exponent", FOG_DIRECTIONAL_EXPONENT)
fc.set_editor_property("enable_volumetric_fog", True)
fc.set_editor_property("volumetric_fog_scattering_distribution", FOG_SCATTERING_DISTRIBUTION)
# NB: unreal.Color positional order is (b, g, r, a); use keywords.
fc.set_editor_property("volumetric_fog_albedo", unreal.Color(**FOG_ALBEDO))
fc.set_editor_property("volumetric_fog_extinction_scale", FOG_EXTINCTION_SCALE)
fc.set_editor_property("volumetric_fog_distance", FOG_VOLUMETRIC_DISTANCE)

# Invisible shadow caster: set_visibility(False) + cast_hidden_shadow is Unreal's supported way
# to have geometry that is never drawn but still occludes light, so nothing appears in the sky
# above the diver while the volumetric fog gains the shadow contrast it needs for shafts.
gobo = spawn(unreal.StaticMeshActor, (400, 0, GOBO_Z), label="SurfaceGobo")
gc = gobo.static_mesh_component
gc.set_mobility(unreal.ComponentMobility.STATIC)
assert gc.set_static_mesh(unreal.load_asset(ENGINE_PLANE)), "gobo mesh missing: %s" % ENGINE_PLANE
gc.set_material(0, m_gobo)
gobo.set_actor_scale3d(unreal.Vector(GOBO_SCALE, GOBO_SCALE, 1))
gc.set_visibility(False)
gc.set_cast_hidden_shadow(True)
gc.set_editor_property("cast_shadow", True)

cam = spawn(unreal.CameraActor, CAM_LOC, CAM_ROT, label="DiverCamera")
cam.tags = [unreal.Name("DiverCamera")]
cam.camera_component.set_field_of_view(CAM_FOV)

pp = spawn(unreal.PostProcessVolume, (0, 0, 200), label="Grade")
pp.set_editor_property("unbound", True)
pp.set_editor_property("priority", 1.0)
pp.set_editor_property("blend_weight", 1.0)
pp_settings = pp.get_editor_property("settings")


def pp_set(name, value):
    """Set one post-process property and its override_ flag. A PostProcessSettings field with
    its override flag left False is simply ignored, which is the classic way a grade 'does
    nothing' while looking correct in the details panel."""
    pp_settings.set_editor_property("override_" + name, True)
    pp_settings.set_editor_property(name, value)


pp_set("color_saturation", unreal.Vector4(PP_SATURATION, PP_SATURATION, PP_SATURATION, 1.0))
pp_set("color_contrast", unreal.Vector4(PP_CONTRAST, PP_CONTRAST, PP_CONTRAST, 1.0))
pp_set("white_temp", PP_TEMPERATURE)
pp_set("bloom_intensity", PP_BLOOM_INTENSITY)
pp_set("bloom_threshold", PP_BLOOM_THRESHOLD)
pp_set("ambient_occlusion_intensity", PP_AO_INTENSITY)
pp_set("ambient_occlusion_radius", PP_AO_RADIUS)
if DOF is None:
    # fstop 32 + focal distance 0 = no circle of confusion anywhere in the frame
    pp_set("depth_of_field_fstop", 32.0)
    pp_set("depth_of_field_focal_distance", 0.0)
else:
    pp_set("depth_of_field_fstop", DOF["fstop"])
    pp_set("depth_of_field_focal_distance", DOF["focal_distance"])
pp.set_editor_property("settings", pp_settings)

fish_cls = unreal.load_class(None, FISH_CLASS)
assert fish_cls is not None, "FishActor class not found (is the C++ module built?)"
rng = random.Random(SCHOOL_SEED)
species_paths = [p for p, _ in SCHOOL_SPECIES]
species_weights = [w for _, w in SCHOOL_SPECIES]
fish_count = 0
for i in range(SCHOOL_COUNT):
    mesh_path = rng.choices(species_paths, weights=species_weights, k=1)[0]
    origin = (rng.uniform(*SCHOOL_X), rng.uniform(*SCHOOL_Y), rng.uniform(*SCHOOL_Z))
    half_w = rng.uniform(*SCHOOL_HALF_W)
    half_h = rng.uniform(*SCHOOL_HALF_H)
    scale = rng.uniform(*SCHOOL_SCALE)
    speed = rng.uniform(*SCHOOL_SPEED)
    seed = rng.randrange(1, 100000)
    name = mesh_path.rsplit("/", 1)[-1]
    label = "Fish_%02d_%s" % (i, name)
    fish = spawn(fish_cls, origin, (0.0, FISH_EDITOR_YAW), label=label)
    sk = unreal.load_asset(mesh_path)
    assert isinstance(sk, unreal.SkeletalMesh), "fish mesh missing: %s" % mesh_path
    fish.set_editor_property("fish_mesh", sk)
    fish.set_editor_property("plane_origin", unreal.Vector(*origin))
    fish.set_editor_property("seed", seed)
    fish.set_editor_property("plane_half_width", half_w)
    fish.set_editor_property("plane_half_height", half_h)
    fish.set_editor_property("max_speed", speed)
    fish.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    # The actor applies FishMesh to its body only at BeginPlay; push it now so the
    # fish is visible in the editor / review captures too.
    body = fish.get_component_by_class(unreal.PoseableMeshComponent)
    assert body is not None, "%s has no PoseableMeshComponent" % label
    body.set_skinned_asset_and_update(sk)
    body.set_visibility(True)
    fish_count += 1

# Props: scattered static meshes on the sand floor, seeded separately so
# changing the school does not reshuffle the reef.
def lane_half_width(x):
    """Half-width of the camera's clear forward lane at distance x."""
    return PROP_CLEAR_RADIUS_Y + (x - PROP_X[0]) * PROP_CLEAR_TAPER


prng = random.Random(PROP_SEED)
prop_count = 0
placed = []   # (x, y, radius) of the props already down
# One of each mesh first so every prop type is represented, then random fills.
mesh_order = list(PROP_MESHES) + [prng.choice(PROP_MESHES)
                                  for _ in range(PROP_COUNT - len(PROP_MESHES))]
for i, (mesh_path, scale_range) in enumerate(mesh_order):
    name = mesh_path.rsplit("/", 1)[-1]
    half = PROP_HALF_EXTENTS[name]

    def draw():
        """One seeded candidate placement, pushed clear of the camera lane."""
        x = prng.uniform(*PROP_X)
        y = prng.uniform(*PROP_Y)
        lane = lane_half_width(x)
        if abs(y) < lane:
            y = math.copysign(lane + PROP_CLEAR_MARGIN, y if y != 0.0 else 1.0)
        scale = prng.uniform(*scale_range)
        return x, y, scale, max(half) * scale

    def fits(cand, factor):
        cx, cy, _, cr = cand
        return all(math.hypot(cx - px, cy - py) >= (cr + pr) * factor
                   for px, py, pr in placed)

    chosen = None
    for factor in (PROP_SEPARATION, PROP_SEPARATION_RELAXED):
        for _ in range(PROP_PLACE_TRIES):
            cand = draw()
            if fits(cand, factor):
                chosen = cand
                break
        if chosen is not None:
            break
    if chosen is None:
        chosen = draw()
        print("REEF_WARN prop %d (%s) placed without clearance" % (i, name))
    x, y, scale, radius = chosen
    placed.append((x, y, radius))
    yaw = prng.uniform(0.0, 360.0)
    prop = spawn(unreal.StaticMeshActor, (x, y, FLOOR_Z), (0.0, yaw),
                 label="Prop_%02d_%s" % (i, name))
    pc = prop.static_mesh_component
    pc.set_mobility(unreal.ComponentMobility.STATIC)
    sm = unreal.load_asset(mesh_path)
    assert isinstance(sm, unreal.StaticMesh), "prop mesh missing: %s" % mesh_path
    assert pc.set_static_mesh(sm), "failed to set prop mesh: %s" % mesh_path
    prop.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    prop_count += 1

assert les.save_current_level(), "save_current_level failed"
print("REEF_OK actors=%d fish=%d props=%d dof=%s map=%s"
      % (len(eas.get_all_level_actors()), fish_count, prop_count,
         "off" if DOF is None else "on", MAP))
