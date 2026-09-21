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
PROP_COUNT = 22
# (mesh path, scale range) — rock_09 is a 15 cm pebble, so it is scaled up a lot
PROP_MESHES = [
    ("/Game/Props/SM_BranchCoral", (0.7, 1.4)),
    ("/Game/Props/SM_PlateCoral",  (0.6, 1.2)),
    ("/Game/Props/SM_BrainCoral",  (0.7, 1.4)),
    ("/Game/Props/SM_FanCoral",    (0.7, 1.5)),
    ("/Game/Props/SM_TubeCoral",   (0.8, 1.6)),
    ("/Game/Props/SM_boulder_01",  (0.8, 1.5)),
    ("/Game/Props/SM_rock_07",     (2.0, 4.0)),
    ("/Game/Props/SM_rock_09",     (4.0, 8.0)),
]
# Unscaled XY half-extents (cm) read from the import_props.py PROPS_OK log, not guessed; the
# placement radius is max(x, y) * actor scale.
PROP_HALF_EXTENTS = {
    "SM_BranchCoral": (28.3, 22.2),
    "SM_PlateCoral":  (52.6, 52.8),
    "SM_BrainCoral":  (31.4, 31.4),
    "SM_FanCoral":    (27.6, 3.4),
    "SM_TubeCoral":   (19.2, 22.5),
    "SM_boulder_01":  (63.6, 91.5),
    "SM_rock_07":     (8.4, 16.0),
    "SM_rock_09":     (3.7, 7.2),
}
PROP_X = (150.0, 900.0)
PROP_Y = (-450.0, 450.0)
# Per-instance variation on top of the existing random yaw: a small tilt, a non-uniform Z
# stretch and one of the tint material instances. Without these, eight meshes across 22 props
# read as stamped copies no matter how good each mesh is.
PROP_TILT_DEG = 7.0
PROP_Z_STRETCH = (0.85, 1.20)
PROP_TINT_COUNT = {"coral": 3, "rock": 2}
PROP_SEPARATION = 0.9          # required gap as a fraction of the summed radii
PROP_SEPARATION_RELAXED = 0.75 # fallback when a prop cannot be placed
PROP_PLACE_TRIES = 200
# The camera's forward lane stays clear of props; it widens with distance so a
# far prop does not occlude the school either.
#
# The lane is cleared by the prop's OUTER EDGE, not by its centre. Until M4b the test was
# abs(y) >= lane, which only keeps a prop's pivot out of the lane: a boulder at X=150 with
# radius 137 cm sitting at y = 65 passes that test while its body spans y = -72 .. 202, i.e.
# straight across the camera axis. That is exactly how the t=12 review frame ended up with the
# player's fish hidden behind a foreground rock. The player fish is pinned to X=220 and the
# camera's 75 deg FOV spans only +-169 cm there, so a prop whose edge clears
# lane(x) + radius can never occlude the player's swimming plane.
# The widest case is a boulder_01 at x=900: 135 + 137 + 5 = 277 cm, still inside PROP_Y,
# so this rule never pushes a prop off the reef.
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
# A masked occluder gives a binary shadow, so the ONLY way to soften the gobo pattern's edges on
# the sand is the light's own penumbra. Source angle is the sun's angular diameter in degrees;
# the gobo sits ~11 m above the floor, so 4 deg spreads each edge over roughly 11 m * tan(4 deg)
# ~= 77 cm. Real sunlight through a water surface is diffused exactly this way.
SUN_SOURCE_ANGLE = 7.0

# The sky light is the ONLY fill in this scene: the sun is pitched -65 deg and the two gobo
# planes shadow most of what it would otherwise reach, so anything the sun misses falls to the
# ambient term. At 0.8 the M4b capture turned the left and right foreground rocks into
# near-black silhouettes across about a third of the frame -- and the player's own fish, pinned
# at X=220, sits inside that zone. A child who cannot see their own fish is a gameplay defect,
# not a grading preference, so the fill goes up. It is raised rather than the gobo dimmed or
# the grade lifted because ambient adds light without touching the sun's volumetric scattering,
# which is what the god rays are made of -- the shafts and the depth survive the change.
SKY_INTENSITY = 2.2
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
# One plane cannot do both jobs. Coarse apertures give the volumetric fog the shadow contrast
# that makes god rays; fine apertures give the sand a ripple pattern that matches M_Caustics.
# Tuning one wrecks the other (105e4c8 had strong shafts and oil-slick blobs on the sand;
# 60c3f4b had a correct sand pattern and almost no shafts), so M4b uses TWO stacked planes.
#
# Fine layer: the 60c3f4b tuning, low over the reef, so its shadow edges stay crisp on the sand.
GOBO_Z = 1100.0
GOBO_SCALE = 60.0                       # 100 cm plane -> 60 m, wider than the 40 m floor
GOBO_THRESHOLD = 0.56                   # opacity-mask cutoff; higher = narrower, sharper shafts
# Ripple cell size relative to the floor caustics periods (52..89 cm). The first M4b pass used
# 3.0 -- three times COARSER than the floor pattern -- and the result on the sand was a handful
# of hard-edged puddles several metres across that read as an oil slick, not as water. 0.9 makes
# the gobo cells slightly FINER than the floor caustics, so many ripples cross the visible floor
# and the two patterns read as coming from the same water surface.
GOBO_PERIOD_SCALE = 0.9
GOBO_EDGE_GAIN = 2.5                    # mask ramp; the blend mode still clips, so this only
                                        # shifts where the edge lands, not how soft it is

# Coarse layer: the 105e4c8 tuning (3 unwarped waves, 3x the floor period, threshold 0.42,
# edge gain 8) whose ONLY job is volumetric contrast. It is parked far above the fine plane for
# two reasons. (1) No z-fighting. (2) A masked occluder casts a binary shadow, so the only
# softening available is the sun's penumbra, which grows with height: at COARSE_Z the plane is
# ~39 m over the sand and 39 m * tan(SUN_SOURCE_ANGLE) blurs each edge by ~4.8 m, about the size
# of a coarse aperture -- so the coarse pattern washes out on the sand (no oil slick) while it
# still modulates the fog it passes through on the way down.
# The sun is pitched -65 deg / yaw 30 deg, so a shadow drops by (0.40, 0.23) x height in XY;
# at 39 m that is (15.8, 9.1) m, which is why the coarse plane is offset back along -X/-Y and
# made much wider than the floor.
GOBO_COARSE_Z = 7000.0
GOBO_COARSE_XY = (-2400.0, -1600.0)
GOBO_COARSE_SCALE = 200.0
GOBO_COARSE_PERIOD_SCALE = 3.0
GOBO_COARSE_THRESHOLD = 0.44
GOBO_COARSE_EDGE_GAIN = 8.0
GOBO_COARSE_WAVES = 3                   # unwarped: the warp is what makes fine ripples organic

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

# Marine snow. NOT Niagara: an emitter graph cannot be built from editor Python (only an empty
# NiagaraSystem can), so a Niagara version would mean committing a hand-made .uasset, which this
# project does not do. The camera is fixed, so translucent curtains standing across the view do
# the job with a fully scripted material graph.
# Brightness was raised from 0.55/0.40/0.28 after a 1920x1080 capture: at the plan defaults the
# specks were technically present but read as almost nothing on screen. That pass left the cell
# sizes and dot radii alone, so only the specks' luminance went up.
#
# The near and mid specks read as white bokeh blobs -- dust on a camera lens -- not as marine
# snow, in the 1920x1080 capture. The cause is APPARENT SIZE, not density or brightness. The
# camera is fixed at 75 deg FOV, so a curtain at distance X is 2*X*tan(37.5) = 1.535*X cm wide
# across 1920 px; a speck's diameter is 2 * DotRadius * CellSize cm, and the visible part is
# roughly 60 % of that (the material's falloff is saturate(1 - d/radius), so the outer rim is
# already transparent). That put the near speck at ~15 px and the mid at ~14 px measured off
# the capture, where a few px is what reads as suspended matter.
#
# So the CELL SIZE is left alone -- it sets the density, which was already tuned against a
# capture -- and only the RADIUS comes down, near 0.11 -> 0.05 and mid 0.13 -> 0.07, which
# lands both layers near 12 px of full extent, i.e. ~7 px visible. The far curtain was already
# at ~9 px extent and is unchanged.
#
# Dropping the near curtain entirely was the other option on the table. The per-item
# attribution measurement (docs/reviews/2026-09-21-m4b-perf.md) says it is worth 0.6 fps --
# below the run-to-run noise floor -- so removing it would have bought no frame budget at all,
# and it is the layer that carries the parallax. It stays.
#
# (tag, distance X, uniform scale, cell size cm, dot radius, brightness, drift cm/s)
SNOW_CURTAINS = [
    ("near", 90.0,  3.0, 26.0, 0.05, 0.80, -7.0),
    ("mid",  240.0, 7.0, 17.0, 0.07, 0.60, -5.0),
    ("far",  480.0, 13.0, 11.0, 0.15, 0.42, -3.5),
]
SNOW_COLOR = dict(r=0.72, g=0.86, b=0.92, a=1.0)
SNOW_Z = 130.0

# Caustics: (period cm, direction deg, drift turns/s). Directions are chosen so
# no pair is within 30 deg of parallel or anti-parallel; periods are
# non-commensurate. Each wave's phase is warped by the previous wave's sine.
CAUSTIC_WAVES = [(52.0, 12.0, 0.11), (67.0, 71.0, -0.08), (89.0, 128.0, 0.14), (113.0, 160.0, -0.06)]
CAUSTIC_WARP = 2.0                      # phase warp amplitude, radians
CAUSTIC_SHARPEN = 6.0                   # power applied to the normalised sum
CAUSTIC_GAIN = 0.7                      # output = pow * GAIN + LIFT, kept within [0, 1]
CAUSTIC_LIFT = 0.3

# --- dev-only attribution switches -------------------------------------------
# Per-item performance attribution needs each M4b addition switched off ONE AT A TIME with
# everything else byte-identical; eyeballing a cost from a total is guessing. With none of
# these set (the normal case, and the only case the committed level is ever built with) the
# shipped configuration is produced. Used by docs/reviews/2026-09-21-m4b-perf.md.
#   AQ_DROP_CURTAINS=near        drop just the near curtain
#   AQ_DROP_CURTAINS=all         drop all three
#   AQ_DROP_GOBO=coarse|fine     drop one gobo plane
#   AQ_NO_SSAO=1                 post-process AO intensity 0
#   AQ_PROP_COUNT=14             fewer props
_drop_curtains = os.environ.get("AQ_DROP_CURTAINS", "")
if _drop_curtains == "all":
    SNOW_CURTAINS = []
elif _drop_curtains:
    SNOW_CURTAINS = [c for c in SNOW_CURTAINS if c[0] != _drop_curtains]
DROP_GOBO = os.environ.get("AQ_DROP_GOBO", "")
if os.environ.get("AQ_NO_SSAO") == "1":
    PP_AO_INTENSITY = 0.0
PROP_COUNT = int(os.environ.get("AQ_PROP_COUNT", PROP_COUNT))
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



def build_surface_gobo(m, period_scale, threshold, edge_gain, n_waves, warped):
    """Opacity-mask material for an invisible waterline plane. Reuses the caustics wave sum:
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
    # All four caustic waves, each phase-warped by the previous one exactly as build_caustics
    # does. Three unwarped sines at a fine period tile into a regular polka-dot lattice on the
    # sand; the warp is what turns that lattice into irregular, organic ripples.
    warp_turns = CAUSTIC_WARP / (2.0 * math.pi)
    prev = None
    waves = []
    for i, (period, angle, drift) in enumerate(CAUSTIC_WAVES[:n_waves]):
        y = -450 + i * 220
        kx = math.cos(math.radians(angle)) / (period * period_scale)
        ky = math.sin(math.radians(angle)) / (period * period_scale)
        ax = mul(px, const(-1600, y - 60, kx), -1450, y - 60)
        ay = mul(py, const(-1600, y, ky), -1450, y)
        at = mul(t, const(-1600, y + 60, drift * 0.5), -1450, y + 60)
        phase = add(add(ax, ay, -1300, y), at, -1150, y)
        if warped and prev is not None:
            phase = add(phase, mul(prev, const(-1150, y + 90, warp_turns), -1000, y + 60), -850, y)
        prev = sine(phase, -700, y)
        waves.append(prev)
    s = waves[0]
    for i, w in enumerate(waves[1:]):
        s = add(s, w, -550 + i * 100, -100)
    n = float(len(waves))
    norm = add(mul(s, const(-250, 100, 1.0 / (2.0 * n)), -150, -100),
               const(-150, 100, 0.5), -50, -100)
    # opacity mask = saturate((norm - threshold) * edge_gain)
    off = node(unreal.MaterialExpressionSubtract, 550, -100)
    mel.connect_material_expressions(norm, "", off, "A")
    mel.connect_material_expressions(const(550, 100, threshold), "", off, "B")
    gain = mul(off, const(700, 100, edge_gain), 800, -100)
    sat = node(unreal.MaterialExpressionSaturate, 950, -100)
    mel.connect_material_expressions(gain, "", sat, "")
    mel.connect_material_property(sat, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    mel.connect_material_property(const(950, 200, 0.0), "",
                                  unreal.MaterialProperty.MP_EMISSIVE_COLOR)


def make_gobo_material(name, period_scale, threshold, edge_gain, n_waves, warped):
    m = material(name)
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    build_surface_gobo(m, period_scale, threshold, edge_gain, n_waves, warped)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


m_gobo = make_gobo_material("M_SurfaceGobo", GOBO_PERIOD_SCALE, GOBO_THRESHOLD,
                            GOBO_EDGE_GAIN, len(CAUSTIC_WAVES), True)
m_gobo_coarse = make_gobo_material("M_SurfaceGoboCoarse", GOBO_COARSE_PERIOD_SCALE,
                                   GOBO_COARSE_THRESHOLD, GOBO_COARSE_EDGE_GAIN,
                                   GOBO_COARSE_WAVES, False)



def build_marine_snow(m):
    """Sparse drifting dots from an analytic cell hash -- no texture, no particle system.

    World Y and Z are divided into cells of CellSize cm. Each cell gets one pseudo-random
    centre from frac(sin(dot(cellId, (12.9898, 78.233))) * 43758.5453), and the pixel's
    distance to that centre becomes the dot. The whole grid slides along Z at Drift cm/s, so the
    specks sink the way marine snow does. Unlit + translucent + two-sided (two-sided removes any
    doubt about which way the plane's normal ended up pointing)."""
    def node(cls, x, y):
        return mel.create_material_expression(m, cls, x, y)

    def const(x, y, v):
        n = node(unreal.MaterialExpressionConstant, x, y)
        n.set_editor_property("r", v)
        return n

    def const2(x, y, a, b):
        n = node(unreal.MaterialExpressionConstant2Vector, x, y)
        n.set_editor_property("r", a)
        n.set_editor_property("g", b)
        return n

    def scalar(x, y, name, default):
        n = node(unreal.MaterialExpressionScalarParameter, x, y)
        n.set_editor_property("parameter_name", name)
        n.set_editor_property("default_value", default)
        return n

    def mask(src, x, y, r, g, b=False):
        n = node(unreal.MaterialExpressionComponentMask, x, y)
        n.set_editor_property("r", r)
        n.set_editor_property("g", g)
        n.set_editor_property("b", b)
        n.set_editor_property("a", False)
        mel.connect_material_expressions(src, "", n, "")
        return n

    def binop(cls, a, b, x, y):
        n = node(cls, x, y)
        mel.connect_material_expressions(a, "", n, "A")
        mel.connect_material_expressions(b, "", n, "B")
        return n

    def unop(cls, a, x, y):
        n = node(cls, x, y)
        mel.connect_material_expressions(a, "", n, "")
        return n

    Mul, Add, Sub, Div = (unreal.MaterialExpressionMultiply, unreal.MaterialExpressionAdd,
                          unreal.MaterialExpressionSubtract, unreal.MaterialExpressionDivide)
    Frac, Floor, Sine = (unreal.MaterialExpressionFrac, unreal.MaterialExpressionFloor,
                         unreal.MaterialExpressionSine)
    Sat = unreal.MaterialExpressionSaturate

    wpos = node(unreal.MaterialExpressionWorldPosition, -2400, 0)
    yz = mask(wpos, -2200, 0, False, True, True)          # world (Y, Z)
    cell = scalar(-2200, 250, "CellSize", 16.0)
    drift = scalar(-2200, 400, "Drift", -5.0)
    radius = scalar(-2200, 550, "DotRadius", 0.13)
    bright = scalar(-2200, 700, "Brightness", 0.4)
    t = node(unreal.MaterialExpressionTime, -2200, 850)

    # slide the field along Z: offset = (0, drift * t)
    slide = binop(Mul, drift, t, -2000, 400)
    offset = node(unreal.MaterialExpressionAppendVector, -1850, 400)
    mel.connect_material_expressions(const(-2000, 500, 0.0), "", offset, "A")
    mel.connect_material_expressions(slide, "", offset, "B")
    moved = binop(Add, yz, offset, -1700, 0)
    p = binop(Div, moved, cell, -1550, 0)                 # position in cell units
    cid = unop(Floor, p, -1400, -150)                     # cell id
    f = unop(Frac, p, -1400, 150)                         # position inside the cell

    # hash: frac(sin(dot(cid, (12.9898, 78.233))) * 43758.5453)
    dot = node(unreal.MaterialExpressionDotProduct, -1200, -150)
    mel.connect_material_expressions(cid, "", dot, "A")
    mel.connect_material_expressions(const2(-1400, -300, 12.9898, 78.233), "", dot, "B")
    h = unop(Frac, binop(Mul, unop(Sine, dot, -1050, -150),
                         const(-1050, -50, 43758.5453), -900, -150), -750, -150)
    # second hash for the other axis, offset so the two are not correlated
    h2 = unop(Frac, binop(Mul, unop(Sine, binop(Add, dot, const(-1050, 50, 3.7), -1050, 30),
                                    -900, 30),
                          const(-900, 130, 24634.6345), -750, 30), -600, 30)
    # clamp the centres into 0.25..0.75 so dots never straddle a cell edge
    centre = node(unreal.MaterialExpressionAppendVector, -450, -60)
    mel.connect_material_expressions(binop(Add, binop(Mul, h, const(-600, -250, 0.5), -450, -150),
                                           const(-450, -250, 0.25), -300, -150), "", centre, "A")
    mel.connect_material_expressions(binop(Add, binop(Mul, h2, const(-600, 130, 0.5), -450, 60),
                                           const(-450, 130, 0.25), -300, 60), "", centre, "B")

    d = node(unreal.MaterialExpressionDistance, -100, 0)
    mel.connect_material_expressions(f, "", d, "A")
    mel.connect_material_expressions(centre, "", d, "B")
    # dot = saturate(1 - d / radius); third hash thins the field so not every cell has a speck
    fall = unop(Sat, binop(Sub, const(50, 120, 1.0), binop(Div, d, radius, 50, 0), 200, 0), 350, 0)
    thin = unop(Sat, binop(Mul, binop(Sub, h, const(200, 250, 0.55), 350, 200),
                           const(350, 300, 6.0), 500, 200), 650, 200)
    opacity = binop(Mul, binop(Mul, fall, thin, 500, 0), bright, 650, 0)

    colour = node(unreal.MaterialExpressionConstant3Vector, 500, -200)
    colour.set_editor_property("constant", unreal.LinearColor(**SNOW_COLOR))
    mel.connect_material_property(binop(Mul, colour, opacity, 800, -200), "",
                                  unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)


m_snow = material("M_MarineSnow")
m_snow.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
m_snow.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
m_snow.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
m_snow.set_editor_property("two_sided", True)
build_marine_snow(m_snow)
mel.recompile_material(m_snow)
eal.save_loaded_asset(m_snow)

snow_instances = {}
for tag, _x, _scale, cell_size, dot_r, bright_v, drift_v in SNOW_CURTAINS:
    mi_path = ENV + "/MI_MarineSnow_" + tag
    if eal.does_asset_exist(mi_path):
        mi = unreal.load_asset(mi_path)
    else:
        mi = tools.create_asset("MI_MarineSnow_" + tag, ENV,
                                unreal.MaterialInstanceConstant,
                                unreal.MaterialInstanceConstantFactoryNew())
    assert mi is not None, "snow instance creation failed: %s" % mi_path
    mel.set_material_instance_parent(mi, m_snow)
    for pname, pval in (("CellSize", cell_size), ("DotRadius", dot_r),
                        ("Brightness", bright_v), ("Drift", drift_v)):
        mel.set_material_instance_scalar_parameter_value(mi, pname, pval)
    eal.save_loaded_asset(mi)
    snow_instances[tag] = mi


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
sc.set_editor_property("light_source_angle", SUN_SOURCE_ANGLE)
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
def spawn_gobo(label, loc, scale, mat):
    a = spawn(unreal.StaticMeshActor, loc, label=label)
    c = a.static_mesh_component
    c.set_mobility(unreal.ComponentMobility.STATIC)
    assert c.set_static_mesh(unreal.load_asset(ENGINE_PLANE)), \
        "gobo mesh missing: %s" % ENGINE_PLANE
    c.set_material(0, mat)
    a.set_actor_scale3d(unreal.Vector(scale, scale, 1))
    c.set_visibility(False)
    c.set_cast_hidden_shadow(True)
    c.set_editor_property("cast_shadow", True)
    return a


if DROP_GOBO != "fine":
    spawn_gobo("SurfaceGobo", (400, 0, GOBO_Z), GOBO_SCALE, m_gobo)
if DROP_GOBO != "coarse":
    spawn_gobo("SurfaceGoboCoarse",
               (GOBO_COARSE_XY[0], GOBO_COARSE_XY[1], GOBO_COARSE_Z),
               GOBO_COARSE_SCALE, m_gobo_coarse)

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

# The curtains stand across the fixed camera's view at three depths. They are unlit translucent
# and cast nothing, so they cannot darken the reef behind them.
for tag, curtain_x, curtain_scale, _c, _r, _b, _d in SNOW_CURTAINS:
    curtain = spawn(unreal.StaticMeshActor, (curtain_x, 0.0, SNOW_Z), (90.0, 0.0),
                    label="SnowCurtain_%s" % tag)
    cc = curtain.static_mesh_component
    cc.set_mobility(unreal.ComponentMobility.STATIC)
    assert cc.set_static_mesh(unreal.load_asset(ENGINE_PLANE)), \
        "curtain mesh missing: %s" % ENGINE_PLANE
    cc.set_material(0, snow_instances[tag])
    curtain.set_actor_scale3d(unreal.Vector(curtain_scale, curtain_scale, 1))
    cc.set_editor_property("cast_shadow", False)
    cc.set_editor_property("receives_decals", False)

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
        """One seeded candidate placement, pushed clear of the camera lane.

        The scale is drawn BEFORE the lane push because the push distance depends on the
        prop's own radius: the lane is cleared by the prop's edge, not by its pivot.
        """
        x = prng.uniform(*PROP_X)
        y = prng.uniform(*PROP_Y)
        scale = prng.uniform(*scale_range)
        radius = max(half) * scale
        keep_out = lane_half_width(x) + radius + PROP_CLEAR_MARGIN
        if abs(y) < keep_out:
            y = math.copysign(keep_out, y if y != 0.0 else 1.0)
        return x, y, scale, radius

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
    pitch = prng.uniform(-PROP_TILT_DEG, PROP_TILT_DEG)
    roll = prng.uniform(-PROP_TILT_DEG, PROP_TILT_DEG)
    z_stretch = prng.uniform(*PROP_Z_STRETCH)
    kind = "rock" if name.startswith("SM_rock") or name.startswith("SM_boulder") else "coral"
    tint_index = prng.randrange(PROP_TINT_COUNT[kind])
    mi_path = "/Game/Props/MI_%s_v%d" % (name[len("SM_"):], tint_index)

    # spawn() takes no roll, so this one call goes straight to the subsystem.
    prop = eas.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(x, y, FLOOR_Z),
        unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw))
    assert prop is not None, "prop spawn failed: %s" % mesh_path
    prop.set_actor_label("Prop_%02d_%s" % (i, name))
    pc = prop.static_mesh_component
    pc.set_mobility(unreal.ComponentMobility.STATIC)
    sm = unreal.load_asset(mesh_path)
    assert isinstance(sm, unreal.StaticMesh), "prop mesh missing: %s" % mesh_path
    assert pc.set_static_mesh(sm), "failed to set prop mesh: %s" % mesh_path
    mi = unreal.load_asset(mi_path)
    assert isinstance(mi, unreal.MaterialInstanceConstant), "prop tint missing: %s" % mi_path
    pc.set_material(0, mi)
    prop.set_actor_scale3d(unreal.Vector(scale, scale, scale * z_stretch))
    # Runtime obstacle avoidance finds props by this tag and derives their radius from the actor's
    # own bounds. Actor tags survive cooking (actor LABELS are editor-only), and deriving the
    # radius at runtime is deliberate: PROP_HALF_EXTENTS above is a THIRD place the same number
    # could live, and M4b proved that a rule duplicated in two places is a rule the verifier can
    # never check.
    prop.tags = [unreal.Name("AquariumProp")]
    prop_count += 1

tagged = len([a for a in eas.get_all_level_actors()
              if a.actor_has_tag(unreal.Name("AquariumProp"))])
assert les.save_current_level(), "save_current_level failed"
print("REEF_OK actors=%d fish=%d props=%d tagged=%d dof=%s map=%s"
      % (len(eas.get_all_level_actors()), fish_count, prop_count, tagged,
         "off" if DOF is None else "on", MAP))
