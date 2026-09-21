#!/bin/bash
# M4c review artefacts. The three M4c behaviours (schooling, prop avoidance,
# vertical-roll removal) are all TIME-AXIS phenomena: none of them shows up in a
# still. The clips are the primary artefact here; the still exists only to prove
# M4b's lighting/fog/coral look did not regress.
#
# Outputs (docs/reviews/<date>-m4c-*):
#   reef.mp4      30 fps, 45 s, same map/seed/camera/auto-input as the M4b clip
#                 (20 s was too short for schools to actually form)
#   vertical.mp4  60 fps, 12 s, a dedicated up/turn/up/turn script at 1920x1080.
#                 The M2-era pirouette only appears on a vertical transition, so
#                 this clip is the ONLY visual evidence that it is gone.
#   scene.png     t=12 frame of the reef clip = the M3 "-wall.png" framing
#   compare.png   <before=M4b scene still> | scene.png  (hstack)
#
# The coral close-up step from render_m4b_compare.sh is DELETED: M4c does not
# touch the coral meshes or their tints, so it would only re-shoot an M4b still.
#
# AUTO-INPUT TOKEN FORMAT -- read from ADiverPlayerController::BuildAutoInputSteps,
# not from memory. Tokens are comma separated; each is ONE direction character
# (R/L/U/D, or 0 for "no keys") followed IMMEDIATELY by a duration in seconds.
# There is NO colon separator and there are NO diagonal tokens: "UR" parses as
# direction 'U' with rest "R1.5", whose Atof is 0, and the step is dropped with a
# warning. The parser only WARNS on junk, so a typo silently shortens the script.
# Both warning strings are asserted to be absent at the end of this script.
#
# The nickname passed via -AquariumAutoNickname and the pattern passed via
# -AquariumAutoInput are TEST DATA ONLY: the engine echoes the whole command
# line into its log, so never put a real user's nickname or session here.
#
# Notes (verified on UE 5.8.2 / macOS), same as render_m4b_compare.sh:
#   - Builds the editor target twice first, then uses the EDITOR binary.
#   - -benchmark -fps=N fixes the timestep, -seconds=N exits by itself.
#   - -ForceRes is required or GameUserSettings.ini overrides the resolution.
#   - Frames land in Saved/UiFrames/UiFrame%05d.png.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UE_ROOT="/Users/Shared/Epic Games/UE_5.8"
UE="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor"
FFMPEG="${FFMPEG:-/opt/homebrew/bin/ffmpeg}"
PROJ="$ROOT/unreal/Aquarium/Aquarium.uproject"
FRAMES="$ROOT/unreal/Aquarium/Saved/UiFrames"
REVIEWS="$ROOT/docs/reviews"
MAP="${MAP:-ReefM1}"
FPS="${FPS:-30}"
SECONDS_TO_RUN="${SECONDS_TO_RUN:-45}"
WATCHDOG_SEC="${WATCHDOG_SEC:-600}"
AUTO_NICKNAME="${AUTO_NICKNAME:-니모}"
# Same shape as the M4b pattern, extended to fill 45 s so schools have time to form.
AUTO_INPUT="${AUTO_INPUT:-R3,U2,L3,D2,0 2,R2,U2,0 2,L3,D2,R3,U2,0 2,L2,D2,0 2,R3,U2,L3,D2,0 2}"
# Vertical-transition script: alternates straight up with a hard horizontal turn
# so the heading crosses the vertical singularity eight times in 12 s.
VERTICAL_FPS="${VERTICAL_FPS:-60}"
VERTICAL_SECONDS="${VERTICAL_SECONDS:-12}"
VERTICAL_INPUT="${VERTICAL_INPUT:-U1.5,R1.5,U1.5,L1.5,U1.5,R1.5,U1.5,L1.5}"
ASSIGNMENT_SEED="${ASSIGNMENT_SEED:-1}"
SKIP_FRAMES="${SKIP_FRAMES:-15}"
# The "before" reference for M4c is the M4b scene still. Do NOT let a bulk
# m4b -> m4c rename touch this line: in M4b exactly that mistake compared the
# milestone against ITSELF and the hstack looked plausible anyway.
BEFORE="${BEFORE:-$REVIEWS/2026-09-21-m4b-scene.png}"
DATE="$(date +%F)"
OUT="$REVIEWS/$DATE-m4c-reef.mp4"
OUT_VERTICAL="$REVIEWS/$DATE-m4c-vertical.mp4"
LOG_REEF="/tmp/m4c-capture-reef.log"
LOG_VERTICAL="/tmp/m4c-capture-vertical.log"

mkdir -p "$REVIEWS"

if [[ "${SKIP_BUILD:-0}" != "1" ]]; then
  for pass in 1 2; do
    echo "build pass $pass"
    "$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development \
      -Project="$PROJ" -WaitMutex 2>&1 | grep -E "Result:|error:|Error:" || true
  done
fi

run_capture() {  # $1 map, $2 seconds, $3 fps, $4 logfile, $5... extra args
  local map="$1" secs="$2" fps="$3" logfile="$4"; shift 4
  rm -rf "$FRAMES"
  local start; start=$(date +%s)
  "$UE" "$PROJ" "$map" -game -windowed -ResX=1920 -ResY=1080 -ForceRes \
    -benchmark -fps="$fps" -seconds="$secs" \
    -notexturestreaming -unattended -nosplash -stdout -FullStdOutLogOutput \
    -AquariumAutoNickname="$AUTO_NICKNAME" \
    -AquariumCaptureUI="$FRAMES" -AquariumAssignmentSeed="$ASSIGNMENT_SEED" \
    "$@" > "$logfile" 2>&1 &
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

encode() {  # $1 fps, $2 expected seconds, $3 output
  local fps="$1" secs="$2" out="$3"
  local count; count="$(ls "$FRAMES"/UiFrame*.png | wc -l | tr -d ' ')"
  if (( count < fps * secs - fps )); then
    echo "ERROR: expected about $(( fps * secs )) frames, got $count" >&2
    exit 1
  fi
  "$FFMPEG" -y -loglevel error -framerate "$fps" -start_number "$SKIP_FRAMES" \
    -i "$FRAMES/UiFrame%05d.png" -c:v libx264 -pix_fmt yuv420p -crf 18 "$out"
  echo "VIDEO_OK $out frames=$(( count - SKIP_FRAMES ))"
}

# --- Step 1: 45 s reef clip + scene still ------------------------------------
if [[ "${SKIP_SCENE:-0}" != "1" ]]; then
  run_capture "$MAP" "$SECONDS_TO_RUN" "$FPS" "$LOG_REEF" -AquariumAutoInput="$AUTO_INPUT"
  encode "$FPS" "$SECONDS_TO_RUN" "$OUT"
  "$FFMPEG" -y -loglevel error -ss 12 -i "$OUT" -frames:v 1 "$REVIEWS/$DATE-m4c-scene.png"
  echo "SCENE_OK $REVIEWS/$DATE-m4c-scene.png"
fi

# --- Step 2: dedicated vertical-transition clip ------------------------------
if [[ "${SKIP_VERTICAL:-0}" != "1" ]]; then
  run_capture "$MAP" "$VERTICAL_SECONDS" "$VERTICAL_FPS" "$LOG_VERTICAL" \
    -AquariumAutoInput="$VERTICAL_INPUT"
  encode "$VERTICAL_FPS" "$VERTICAL_SECONDS" "$OUT_VERTICAL"
  echo "VERTICAL_OK $OUT_VERTICAL"
fi

# --- Step 3: side by side against the M4b still ------------------------------
[[ -f "$BEFORE" ]] || { echo "ERROR: before image not found: $BEFORE" >&2; exit 1; }
case "$(basename "$BEFORE")" in
  *m4c*) echo "ERROR: BEFORE points at an M4c artefact ($BEFORE) - that compares M4c against itself" >&2; exit 1 ;;
esac
"$FFMPEG" -y -loglevel error -i "$BEFORE" -i "$REVIEWS/$DATE-m4c-scene.png" \
  -filter_complex hstack "$REVIEWS/$DATE-m4c-compare.png"
echo "COMPARE_OK $REVIEWS/$DATE-m4c-compare.png (left=$(basename "$BEFORE"), right=$DATE-m4c-scene.png)"

# --- Step 4: material compile + auto-input token guards ----------------------
# A material that fails to compile is drawn as the grey UE Default Material and
# looks merely "a bit flat" in a still, so the log is the only honest check.
# An unrecognised auto-input token is only a warning, so the capture would still
# produce a plausible-looking clip of the WRONG swim path.
FAILED=0; UNKNOWN=0; BADDUR=0
for LOG in "$LOG_REEF" "$LOG_VERTICAL"; do
  [[ -f "$LOG" ]] || continue
  FAILED=$(( FAILED + $(grep -c "Failed to compile Material" "$LOG" || true) ))
  UNKNOWN=$(( UNKNOWN + $(grep -c "AquariumAutoInput: unknown direction" "$LOG" || true) ))
  BADDUR=$(( BADDUR + $(grep -c "AquariumAutoInput: bad duration" "$LOG" || true) ))
done
echo "MATERIAL_COMPILE_FAILURES=$FAILED AUTOINPUT_UNKNOWN=$UNKNOWN AUTOINPUT_BAD_DURATION=$BADDUR"
if [[ "$FAILED" != "0" ]]; then
  echo "ERROR: $FAILED material(s) failed to compile - drawn as the grey Default Material" >&2
  exit 1
fi
if [[ "$UNKNOWN" != "0" || "$BADDUR" != "0" ]]; then
  echo "ERROR: auto-input pattern had entries the parser dropped - the swim path is not the one intended" >&2
  grep -h "AquariumAutoInput:" "$LOG_REEF" "$LOG_VERTICAL" 2>/dev/null | head -10 >&2
  exit 1
fi
echo "GUARDS_OK"
