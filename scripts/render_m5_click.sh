#!/bin/bash
# M5 review artefacts. Fleeing is entirely a TIME-AXIS phenomenon: a still cannot
# tell a fleeing fish from a swimming one. The CLIP is the artefact that gets
# judged; the still only proves M4c's look did not regress.
#
# Outputs (docs/reviews/<date>-m5-*):
#   click.mp4   30 fps, 30 s, same map/seed/camera as the M4c clip, with scripted
#               clicks: a background fish, empty water, the same fish twice fast,
#               and the player's own fish while a key is held.
#   clicks.csv  one row per click attempt (what was aimed at, what was hit)
#   flee.png    t=12 frame of the clip
#   compare.png <before=M4c scene still> | flee.png (hstack)
#
# AUTO-CLICK TOKEN FORMAT -- read from ADiverPlayerController::BuildAutoClicks,
# not from memory. Tokens are comma separated; each is
#     <seconds>@<nx>x<ny>
# with EXACTLY one '@' and one 'x'. There is NO colon form. nx/ny are viewport
# fractions in 0..1. The parser only WARNS on junk, so a typo silently removes a
# click and still produces a convincing clip -- which is why this script asserts
# both warning counts are 0 AND that "armed N" matches CLICK_COUNT below.
#
# AUTO-INPUT TOKEN FORMAT: <direction-letter><seconds>, letters R/L/U/D/0, no
# colons, no diagonals. Same parser caveat, same assertion.
#
# The nickname passed via -AquariumAutoNickname is TEST DATA ONLY: the engine
# echoes the whole command line into its log. Never put a real child's nickname
# here, and note the click CSV itself carries no nickname column by design.
#
# Notes (verified on UE 5.8.2 / macOS), same as render_m4c_compare.sh:
#   - Builds the editor target twice first, then uses the EDITOR binary.
#   - -benchmark -fps=N fixes the timestep, -seconds=N exits by itself.
#   - -ForceRes is required or GameUserSettings.ini overrides the resolution.
#   - Frames land in Saved/UiFrames/UiFrame%05d.png.
#   - This is a REAL (non -nullrhi) run, which is the only run whose log can
#     honestly answer the material-compile question.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UE_ROOT="/Users/Shared/Epic Games/UE_5.8"
UE="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor"
FFMPEG="${FFMPEG:-/opt/homebrew/bin/ffmpeg}"
PROJ="$ROOT/unreal/Aquarium/Aquarium.uproject"
FRAMES="$ROOT/unreal/Aquarium/Saved/UiFrames"
REVIEWS="$ROOT/docs/reviews"
LOG="${LOG:-/tmp/m5-capture-click.log}"
MAP="${MAP:-ReefM1}"
FPS="${FPS:-30}"
SECONDS_TO_RUN="${SECONDS_TO_RUN:-30}"
WATCHDOG_SEC="${WATCHDOG_SEC:-600}"
AUTO_NICKNAME="${AUTO_NICKNAME:-니모}"
ASSIGNMENT_SEED="${ASSIGNMENT_SEED:-1}"
SKIP_FRAMES="${SKIP_FRAMES:-15}"
DATE="$(date +%F)"
# The session starts ~2 s after BeginPlay (kAutoSubmitDelay), so every click is
# scheduled after that. Aiming points are viewport fractions.
# Aim points are NOT guessed. Two probe captures swept grids of clicks and only
# the points that returned a real plane X are used here (592.6, 348.0 and the
# player's own 220.0). A fish is a small, moving target, so a click script built
# from memory produces an all-miss CSV and a clip that still looks fine.
#   5.2  a background fish, upper left    5.6  the SAME point 0.4 s later
#   9.0  empty water, top of frame        13.0 just off a fish (a near miss)
#   13.2 that fish                        16.0 the player's own fish, key held
#   19.0 the player's own fish once more
AUTO_CLICK="${AUTO_CLICK:-5.2@0.30x0.30,5.6@0.30x0.30,9.0@0.50x0.10,13.0@0.40x0.42,13.2@0.44x0.42,16.0@0.50x0.50,19.0@0.50x0.50}"
CLICK_COUNT="${CLICK_COUNT:-7}"
# RIGHT is held from t=15.8, so the t=16.0 click lands while the child really is
# holding a key (F-12), and the flee then has to beat it. The key is pressed only
# 0.2 s before the click on purpose: the player's fish accelerates at 30 cm/s^2,
# so it is still at the centre of the frame where the click aims, instead of
# having wandered off it.
AUTO_INPUT="${AUTO_INPUT:-0 15.8,R1.0,0 13.2}"
# The "before" reference for M5 is the M4c scene still. Do NOT let a bulk rename
# touch this line: in M4b exactly that mistake compared the milestone against
# ITSELF and the hstack looked plausible anyway.
BEFORE="${BEFORE:-$REVIEWS/2026-09-21-m4c-scene.png}"
CSV="$REVIEWS/$DATE-m5-clicks.csv"
OUT="$REVIEWS/$DATE-m5-click.mp4"
STILL="$REVIEWS/$DATE-m5-flee.png"
COMPARE="$REVIEWS/$DATE-m5-compare.png"

# The comparison must be M5-against-M4c, and it must really be the M4c image on
# the left. Both halves of that claim are checked, before and after the hstack.
case "$(basename "$BEFORE")" in
  *m5*) echo "ERROR: BEFORE points at an M5 artefact ($BEFORE) - that compares M5 against itself" >&2; exit 1 ;;
  *m4c*) ;;
  *) echo "ERROR: BEFORE is not the M4c scene still ($BEFORE)" >&2; exit 1 ;;
esac
[[ -f "$BEFORE" ]] || { echo "ERROR: before image not found: $BEFORE" >&2; exit 1; }

mkdir -p "$REVIEWS"

if [[ "${SKIP_BUILD:-0}" != "1" ]]; then
  for pass in 1 2; do   # UBT writes UnrealEditor.modules one build late
    echo "build pass $pass"
    "$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development \
      -Project="$PROJ" -WaitMutex 2>&1 | grep -E "Result:|error:|Error:" || true
  done
fi

if [[ "${SKIP_CAPTURE:-0}" != "1" ]]; then
  rm -rf "$FRAMES"; mkdir -p "$FRAMES"
  rm -f "$CSV"
  START=$(date +%s)
  "$UE" "$PROJ" "$MAP" -game -windowed -ResX=1920 -ResY=1080 -ForceRes \
    -benchmark -fps="$FPS" -seconds="$SECONDS_TO_RUN" \
    -notexturestreaming -unattended -nosplash -stdout -FullStdOutLogOutput \
    -AquariumAutoNickname="$AUTO_NICKNAME" \
    -AquariumAssignmentSeed="$ASSIGNMENT_SEED" \
    -AquariumAutoInput="$AUTO_INPUT" \
    -AquariumAutoClick="$AUTO_CLICK" \
    -AquariumClickLog="$CSV" \
    -AquariumCaptureUI="$FRAMES" > "$LOG" 2>&1 &
  PID=$!
  while kill -0 "$PID" 2>/dev/null; do
    if (( $(date +%s) - START > WATCHDOG_SEC )); then
      echo "watchdog: killing pid $PID after ${WATCHDOG_SEC}s" >&2
      kill "$PID" 2>/dev/null || true; sleep 5; kill -9 "$PID" 2>/dev/null || true; break
    fi
    sleep 2
  done
  wait "$PID" || true
  echo "engine wall time: $(( $(date +%s) - START ))s"
  echo "frames=$(ls "$FRAMES"/UiFrame*.png 2>/dev/null | wc -l | tr -d ' ')"
fi

# --- assertions: a capture that verified nothing is worse than no capture ------
ARMED="$(grep -o 'AquariumAutoClick: armed [0-9]*' "$LOG" | tail -1 | awk '{print $3}')"
CLICK_BAD="$(grep -c 'AquariumAutoClick: bad token' "$LOG" || true)"
CLICK_RANGE="$(grep -c 'AquariumAutoClick: coords out of range' "$LOG" || true)"
INPUT_UNKNOWN="$(grep -c 'AquariumAutoInput: unknown direction' "$LOG" || true)"
INPUT_BAD="$(grep -c 'AquariumAutoInput: bad duration' "$LOG" || true)"
MATERIAL="$(grep -c 'Failed to compile Material' "$LOG" || true)"
echo "ARMED=${ARMED:-none} CLICK_BAD=$CLICK_BAD CLICK_RANGE=$CLICK_RANGE INPUT_UNKNOWN=$INPUT_UNKNOWN INPUT_BAD=$INPUT_BAD MATERIAL_COMPILE_FAILURES=$MATERIAL"
[[ "${ARMED:-none}" == "$CLICK_COUNT" ]] || { echo "ERROR: auto-click script was silently truncated (armed=${ARMED:-none}, wanted $CLICK_COUNT)" >&2; exit 1; }
[[ "$CLICK_BAD" == "0" && "$CLICK_RANGE" == "0" ]] || { echo "ERROR: auto-click parse warnings - clicks were dropped" >&2; grep -h 'AquariumAutoClick:' "$LOG" | head >&2; exit 1; }
[[ "$INPUT_UNKNOWN" == "0" && "$INPUT_BAD" == "0" ]] || { echo "ERROR: auto-input parse warnings - the swim path is not the one intended" >&2; exit 1; }
[[ "$MATERIAL" == "0" ]] || { echo "ERROR: materials fell back to the grey default" >&2; exit 1; }
[[ -f "$CSV" ]] || { echo "ERROR: no click log written" >&2; exit 1; }
ROWS="$(($(wc -l < "$CSV") - 1))"
HITS="$(awk -F, 'NR>1 && $6==1' "$CSV" | wc -l | tr -d ' ')"
echo "CLICK_ROWS=$ROWS CLICK_HITS=$HITS"
[[ "$ROWS" == "$CLICK_COUNT" ]] || { echo "ERROR: not every scripted click reached HandleClickAt ($ROWS of $CLICK_COUNT)" >&2; exit 1; }
# 9.0@0.50x0.10 aims at open water near the top of the frame, so at least one
# miss must be recorded. All-miss would mean the deprojection path is broken
# while the clip still looks perfectly fine; all-hit would mean the ellipse test
# is too generous. Only a MIX is evidence that the picking really ran.
[[ "$HITS" -ge 1 && "$HITS" -lt "$CLICK_COUNT" ]] || { echo "ERROR: click log is implausible: $HITS/$CLICK_COUNT hits" >&2; exit 1; }
# The CSV must never carry a nickname (privacy rule), so the header is pinned.
head -1 "$CSV" | grep -qx 'index,time_s,ndc_x,ndc_y,hit_plane_x,hit,state_before' \
  || { echo "ERROR: unexpected click-log columns" >&2; exit 1; }
grep -qF "$AUTO_NICKNAME" "$CSV" && { echo "ERROR: nickname leaked into the click log" >&2; exit 1; } || true
echo "GUARDS_OK"

# --- artefacts ----------------------------------------------------------------
COUNT="$(ls "$FRAMES"/UiFrame*.png | wc -l | tr -d ' ')"
if (( COUNT < FPS * SECONDS_TO_RUN - FPS )); then
  echo "ERROR: expected about $(( FPS * SECONDS_TO_RUN )) frames, got $COUNT" >&2; exit 1
fi
"$FFMPEG" -y -loglevel error -framerate "$FPS" -start_number "$SKIP_FRAMES" \
  -i "$FRAMES/UiFrame%05d.png" -c:v libx264 -pix_fmt yuv420p -crf 18 "$OUT"
echo "VIDEO_OK $OUT frames=$(( COUNT - SKIP_FRAMES ))"
"$FFMPEG" -y -loglevel error -ss 12 -i "$OUT" -frames:v 1 "$STILL"
echo "STILL_OK $STILL"
"$FFMPEG" -y -loglevel error -i "$BEFORE" -i "$STILL" -filter_complex hstack "$COMPARE"
# Verify the left half REALLY is the BEFORE image rather than a second copy of
# the new still: crop both halves back out and compare them to their sources.
W="$(/opt/homebrew/bin/ffprobe -v error -select_streams v:0 -show_entries stream=width -of csv=p=0 "$COMPARE")"
HALF=$(( W / 2 ))
LDIFF="$($FFMPEG -hide_banner -i "$COMPARE" -i "$BEFORE" -filter_complex \
  "[0:v]crop=$HALF:ih:0:0[l];[l][1:v]psnr" -f null - 2>&1 | sed -n 's/.*average:\([0-9.a-z]*\).*/\1/p' | head -1)"
RDIFF="$($FFMPEG -hide_banner -i "$COMPARE" -i "$STILL" -filter_complex \
  "[0:v]crop=$HALF:ih:$HALF:0[r];[r][1:v]psnr" -f null - 2>&1 | sed -n 's/.*average:\([0-9.a-z]*\).*/\1/p' | head -1)"
echo "HSTACK_LEFT_PSNR_vs_BEFORE=$LDIFF HSTACK_RIGHT_PSNR_vs_AFTER=$RDIFF"
[[ "$LDIFF" == "inf" && "$RDIFF" == "inf" ]] || { echo "ERROR: hstack halves are not (M4c | M5)" >&2; exit 1; }
echo "COMPARE_OK $COMPARE (left=$(basename "$BEFORE"), right=$(basename "$STILL"))"
ls -l "$REVIEWS/$DATE-m5-"*
