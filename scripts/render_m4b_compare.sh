#!/bin/bash
# M4b review artefacts: a close-up still of the five coral types, the reef clip
# and scene still rendered with exactly the same arguments as render_m3_video.sh,
# and a side-by-side against the M4a still shot with the same camera and seed.
#
# Outputs (docs/reviews/<date>-m4b-*):
#   coral.png    the five coral meshes side by side, each with its own tint instance
#   reef.mp4     30 fps, 20 s, same map/seed/auto-input as the M3 control clip
#   scene.png    t=12 frame of that clip = the M3 "-wall.png" framing
#   compare.png  <before> | scene.png  (hstack)
#
# The close-up does NOT add a dev-only flag and does NOT touch build_reef_m1.py.
# It builds a throwaway level (/Game/Maps/_CoralTmp) with editor Python: fog,
# sun and sky copied from the reef tuning, the five coral meshes and a
# CameraActor tagged "DiverCamera" (ADiverPlayerController picks that tag up on
# any map), then renders it through the already-verified -game + -AquariumCaptureUI
# path and deletes the level again. The rig sits at Y = CLOSEUP_Y, far from the
# origin, so the player fish the game mode spawns after the auto nickname is
# nowhere near the frame.
#
# The coral camera sits 3.3 m behind the coral row at 45 deg FOV, which is wide
# enough for all five meshes but still much tighter than the reef's 75 deg, so
# the baked BaseColor/Normal/Roughness detail is actually readable.
#
# The nickname passed via -AquariumAutoNickname and the pattern passed via
# -AquariumAutoInput are TEST DATA ONLY: the engine echoes the whole command
# line into its log, so never put a real user's nickname or session here.
#
# Notes (verified on UE 5.8.2 / macOS), same as render_m3_video.sh:
#   - Builds the editor target twice first, then uses the EDITOR binary.
#   - -benchmark -fps=30 fixes the timestep, -seconds=N exits by itself.
#   - -ForceRes is required or GameUserSettings.ini overrides the resolution.
#   - Frames land in Saved/UiFrames/UiFrame%05d.png.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UE_ROOT="/Users/Shared/Epic Games/UE_5.8"
UE="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor"
UE_CMD="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor-Cmd"
FFMPEG="${FFMPEG:-/opt/homebrew/bin/ffmpeg}"
PROJ="$ROOT/unreal/Aquarium/Aquarium.uproject"
FRAMES="$ROOT/unreal/Aquarium/Saved/UiFrames"
REVIEWS="$ROOT/docs/reviews"
MAP="${MAP:-ReefM1}"
FPS="${FPS:-30}"
SECONDS_TO_RUN="${SECONDS_TO_RUN:-20}"
WATCHDOG_SEC="${WATCHDOG_SEC:-300}"
AUTO_NICKNAME="${AUTO_NICKNAME:-니모}"
AUTO_INPUT="${AUTO_INPUT:-R3,U2,L3,D2,0 2,R2,U2,0 2}"
ASSIGNMENT_SEED="${ASSIGNMENT_SEED:-1}"
SKIP_FRAMES="${SKIP_FRAMES:-15}"
# Pre-M4a still to compare against: same map, same seed, same auto-input pattern
# and the same t=12 frame, produced by render_m3_video.sh before the M4a rebuild.
BEFORE="${BEFORE:-$REVIEWS/2026-09-21-m4a-scene.png}"
CLOSEUP_FOV="${CLOSEUP_FOV:-45}"
DATE="$(date +%F)"
OUT="$REVIEWS/$DATE-m4b-reef.mp4"
TMP_MAP_PKG="$ROOT/unreal/Aquarium/Content/Maps/_CoralTmp.umap"

mkdir -p "$REVIEWS"

if [[ "${SKIP_BUILD:-0}" != "1" ]]; then
  for pass in 1 2; do
    echo "build pass $pass"
    "$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development \
      -Project="$PROJ" -WaitMutex 2>&1 | grep -E "Result:|error:|Error:" || true
  done
fi

run_capture() {  # $1 map, $2 seconds, $3... extra args
  local map="$1" secs="$2"; shift 2
  rm -rf "$FRAMES"
  local start; start=$(date +%s)
  "$UE" "$PROJ" "$map" -game -windowed -ResX=1920 -ResY=1080 -ForceRes \
    -benchmark -fps="$FPS" -seconds="$secs" \
    -notexturestreaming -unattended -nosplash -log \
    -AquariumAutoNickname="$AUTO_NICKNAME" \
    -AquariumCaptureUI="$FRAMES" -AquariumAssignmentSeed="$ASSIGNMENT_SEED" \
    "$@" >/dev/null 2>&1 &
  local pid=$!
  while kill -0 "$pid" 2>/dev/null; do
    if (( $(date +%s) - start > WATCHDOG_SEC )); then
      echo "watchdog: killing pid $pid after ${WATCHDOG_SEC}s" >&2
      kill "$pid" 2>/dev/null || true; sleep 5; kill -9 "$pid" 2>/dev/null || true; break
    fi
    sleep 2
  done
  wait "$pid" || true
  echo "engine wall time ($map): $(( $(date +%s) - start ))s"
  echo "frames=$(ls "$FRAMES"/UiFrame*.png 2>/dev/null | wc -l | tr -d ' ')"
}

# --- Step 1: close-up still --------------------------------------------------
if [[ "${SKIP_CLOSEUP:-0}" != "1" ]]; then
  PYDIR="$(mktemp -d)"; PY="$PYDIR/coral_m4b.py"
  cat > "$PY" <<'PYEOF'
import os, unreal

FOV = float(os.environ.get("CLOSEUP_FOV", "45"))
MAP = "/Game/Maps/_CoralTmp"
Y = 5000.0            # far from the origin: the player fish spawns near X=220,Y=0
Z = 130.0
CAM_X = -180.0        # ~3.3 m behind the coral row

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
eal = unreal.EditorAssetLibrary

if eal.does_asset_exist(MAP):
    assert les.load_level(MAP)
    old = eas.get_all_level_actors()
    if old:
        eas.destroy_actors(old)
else:
    assert les.new_level(MAP), "new_level failed"

def spawn(cls, loc, rot=(0.0, 0.0), label=None):
    a = eas.spawn_actor_from_class(cls, unreal.Vector(*loc),
                                   unreal.Rotator(roll=0.0, pitch=rot[0], yaw=rot[1]))
    assert a is not None, "spawn failed: %s" % cls
    if label:
        a.set_actor_label(label)
    return a

# Same light/fog tuning as build_reef_m1.py so the close-up is lit like the reef.
sun = spawn(unreal.DirectionalLight, (0, Y, 1000), (-65.0, 30.0), label="Sun")
sc = sun.light_component
sc.set_mobility(unreal.ComponentMobility.MOVABLE)
sc.set_intensity(14.0)
sc.set_light_color(unreal.LinearColor(r=0.55, g=0.85, b=1.0))
caustics = unreal.load_asset("/Game/Env/M_Caustics")
if caustics:
    sc.set_editor_property("light_function_material", caustics)

sky = spawn(unreal.SkyLight, (0, Y, 500), label="Sky")
kc = sky.light_component
kc.set_mobility(unreal.ComponentMobility.MOVABLE)
kc.set_editor_property("source_type", unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
kc.set_editor_property("cubemap", unreal.load_asset("/Engine/MapTemplates/Sky/DaylightAmbientCubemap"))
kc.set_intensity(2.2)   # matches build_reef_m1.py SKY_INTENSITY
kc.set_light_color(unreal.LinearColor(r=0.35, g=0.65, b=0.9))
kc.recapture_sky()

fog = spawn(unreal.ExponentialHeightFog, (0, Y, 0), label="Water")
fc = fog.component
fc.set_editor_property("fog_density", 2.2)   # matches build_reef_m1.py FOG_DENSITY
fc.set_editor_property("fog_height_falloff", 0.0)
fc.set_editor_property("fog_max_opacity", 1.0)
fc.set_editor_property("start_distance", 0.0)
fc.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(r=0.02, g=0.18, b=0.30))
fc.set_editor_property("directional_inscattering_luminance", unreal.LinearColor(r=0.10, g=0.35, b=0.45))
fc.set_editor_property("enable_volumetric_fog", True)

cam = spawn(unreal.CameraActor, (CAM_X, Y, 60.0), (-8.0, 0.0), label="DiverCamera")
cam.tags = [unreal.Name("DiverCamera")]
cam.camera_component.set_field_of_view(FOV)

CORALS = ["BranchCoral", "PlateCoral", "BrainCoral", "FanCoral", "TubeCoral"]
for i, name in enumerate(CORALS):
    mesh_path = "/Game/Props/SM_%s" % name
    sm = unreal.load_asset(mesh_path)
    assert isinstance(sm, unreal.StaticMesh), "coral mesh missing: %s" % mesh_path
    prop = spawn(unreal.StaticMeshActor, (150.0, Y - 130.0 + i * 65.0, 0.0),
                 (0.0, 25.0 * i), label="Coral_%s" % name)
    pc = prop.static_mesh_component
    pc.set_mobility(unreal.ComponentMobility.STATIC)
    assert pc.set_static_mesh(sm), "failed to set coral mesh: %s" % mesh_path
    mi = unreal.load_asset("/Game/Props/MI_%s_v0" % name)
    assert mi is not None, "coral tint missing for %s" % name
    pc.set_material(0, mi)

floor = spawn(unreal.StaticMeshActor, (150.0, Y, 0.0), label="SandPatch")
fsc = floor.static_mesh_component
fsc.set_mobility(unreal.ComponentMobility.STATIC)
assert fsc.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane"))
fsc.set_material(0, unreal.load_asset("/Game/Env/M_Sand"))
floor.set_actor_scale3d(unreal.Vector(8.0, 8.0, 1.0))

assert les.save_current_level(), "save_current_level failed"
print("CORAL_LEVEL_OK %s corals=%d fov=%.1f" % (MAP, len(CORALS), FOV))
PYEOF
  CLOSEUP_FOV="$CLOSEUP_FOV" \
    "$UE_CMD" "$PROJ" -run=pythonscript -script="$PY" -unattended -nosplash \
    2>&1 | grep -E "CORAL_LEVEL_OK|Error:|Fatal|AssertionError|Traceback" || true
  rm -rf "$PYDIR"

  run_capture "_CoralTmp" 5
  CLOSEUP_FRAME="$(ls "$FRAMES"/UiFrame*.png | sed -n '90p')"   # ~t=3, after the auto nickname
  [[ -n "$CLOSEUP_FRAME" ]] || { echo "ERROR: no coral close-up frame" >&2; exit 1; }
  cp "$CLOSEUP_FRAME" "$REVIEWS/$DATE-m4b-coral.png"
  echo "CLOSEUP_OK $REVIEWS/$DATE-m4b-coral.png (from $(basename "$CLOSEUP_FRAME"))"
  rm -f "$TMP_MAP_PKG"
fi

# --- Step 2: reef clip + scene still -----------------------------------------
if [[ "${SKIP_SCENE:-0}" != "1" ]]; then
  run_capture "$MAP" "$SECONDS_TO_RUN" -AquariumAutoInput="$AUTO_INPUT"
  COUNT="$(ls "$FRAMES"/UiFrame*.png | wc -l | tr -d ' ')"
  if (( COUNT < FPS * SECONDS_TO_RUN - FPS )); then
    echo "ERROR: expected about $(( FPS * SECONDS_TO_RUN )) frames, got $COUNT" >&2
    exit 1
  fi
  "$FFMPEG" -y -loglevel error -framerate "$FPS" -start_number "$SKIP_FRAMES" \
    -i "$FRAMES/UiFrame%05d.png" -c:v libx264 -pix_fmt yuv420p -crf 18 "$OUT"
  echo "VIDEO_OK $OUT frames=$(( COUNT - SKIP_FRAMES ))"
  "$FFMPEG" -y -loglevel error -ss 12 -i "$OUT" -frames:v 1 "$REVIEWS/$DATE-m4b-scene.png"
  echo "SCENE_OK $REVIEWS/$DATE-m4b-scene.png"
fi

# --- Step 3: side by side ----------------------------------------------------
[[ -f "$BEFORE" ]] || { echo "ERROR: before image not found: $BEFORE" >&2; exit 1; }
"$FFMPEG" -y -loglevel error -i "$BEFORE" -i "$REVIEWS/$DATE-m4b-scene.png" \
  -filter_complex hstack "$REVIEWS/$DATE-m4b-compare.png"
echo "COMPARE_OK $REVIEWS/$DATE-m4b-compare.png (left=$(basename "$BEFORE"), right=$DATE-m4b-scene.png)"

# --- Step 4: material compile guard -------------------------------------------
# A material that fails to compile is drawn as the grey UE Default Material and
# looks merely "a bit flat" in a still, so the log is the only honest check.
ENGINE_LOG="$HOME/Library/Logs/Aquarium/Aquarium.log"
FAILED=0
if [[ -f "$ENGINE_LOG" ]]; then
  FAILED="$(grep -c "Failed to compile Material" "$ENGINE_LOG" || true)"
fi
echo "MATERIAL_COMPILE_FAILURES=$FAILED"
if [[ "$FAILED" != "0" ]]; then
  echo "ERROR: $FAILED material(s) failed to compile - they are being drawn as the grey Default Material" >&2
  grep "Failed to compile Material" "$ENGINE_LOG" | head -20 >&2
  exit 1
fi
