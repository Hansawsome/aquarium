#!/bin/bash
# Renders a deterministic 30 fps, 20 s capture of the M3 arrow-key control work:
# the player's fish is driven by the dev-only -AquariumAutoInput replay so the
# clip shows steering, wall sliding along the swim-plane edge and the coast to a
# stop when no key is held. Uses the dev-only -AquariumCaptureUI frame capture
# (one UI-inclusive screenshot per fixed-timestep tick; -dumpmovie is NOT used
# because the engine drops the Slate/UMG layer while dumping a movie), encodes
# it with ffmpeg into docs/reviews/<date>-m3-control.mp4 and pulls two review
# stills out of it.
#
# Timeline: the entry widget is up for the first 2 s, then the dev-only auto
# replay submits the nickname and the input pattern starts driving the fish.
#   t=5   mid steering leg          -> <date>-m3-steer.png
#   t=12  at/near the plane edge    -> <date>-m3-wall.png
#
# The input pattern is a comma-separated list of <DIR><seconds> with
# DIR in R L U D 0 (0 = no keys held); it loops for the whole session.
# 3 s of one direction at MaxSpeed 90 cm/s is enough to reach the swim-plane
# wall (half extents ~130x65 cm), which is deliberate: that is the slide demo.
#
# -AquariumAssignmentSeed pins the species pick for the player's fish and its
# swim seed; without it the assignment differs run to run and two captures of
# the same commit cannot be compared.
#
# The nickname passed via -AquariumAutoNickname and the pattern passed via
# -AquariumAutoInput are TEST DATA ONLY: the engine echoes the whole command
# line into its log, so never put a real user's nickname or session here.
#
# Notes (verified on UE 5.8.2 / macOS), same as render_m2b_video.sh:
#   - Builds the editor target twice first (this machine lags one build on
#     UnrealEditor.modules), then uses the EDITOR binary in -game mode.
#   - -benchmark -fps=30 fixes the timestep, -seconds=N exits by itself.
#   - -ForceRes is required or GameUserSettings.ini overrides the resolution.
#   - Frames land in Saved/UiFrames/UiFrame%05d.png.
#   - A watchdog kills the engine if it has not exited after WATCHDOG_SEC.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UE_ROOT="/Users/Shared/Epic Games/UE_5.8"
UE="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor"
FFMPEG="${FFMPEG:-/opt/homebrew/bin/ffmpeg}"
PROJ="$ROOT/unreal/Aquarium/Aquarium.uproject"
FRAMES="$ROOT/unreal/Aquarium/Saved/UiFrames"
MAP="${MAP:-ReefM1}"
FPS="${FPS:-30}"
SECONDS_TO_RUN="${SECONDS_TO_RUN:-20}"
WATCHDOG_SEC="${WATCHDOG_SEC:-300}"
# Test-only nickname; no auto exit, the session stays up for the whole clip.
AUTO_NICKNAME="${AUTO_NICKNAME:-니모}"
# Test-only scripted input: right, up, left, down, coast, right, up, coast.
AUTO_INPUT="${AUTO_INPUT:-R3,U2,L3,D2,0 2,R2,U2,0 2}"
ASSIGNMENT_SEED="${ASSIGNMENT_SEED:-1}"
DATE="$(date +%F)"
OUT="$ROOT/docs/reviews/$DATE-m3-control.mp4"

if [[ "${SKIP_BUILD:-0}" != "1" ]]; then
  for pass in 1 2; do
    echo "build pass $pass"
    "$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development \
      -Project="$PROJ" -WaitMutex 2>&1 | grep -E "Result:|error:|Error:" || true
  done
fi

rm -rf "$FRAMES"
mkdir -p "$ROOT/docs/reviews"

START=$(date +%s)
"$UE" "$PROJ" "$MAP" -game -windowed -ResX=1920 -ResY=1080 -ForceRes \
  -benchmark -fps="$FPS" -seconds="$SECONDS_TO_RUN" \
  -notexturestreaming -unattended -nosplash -log \
  -AquariumAutoNickname="$AUTO_NICKNAME" \
  -AquariumAutoInput="$AUTO_INPUT" \
  -AquariumCaptureUI="$FRAMES" -AquariumAssignmentSeed="$ASSIGNMENT_SEED" \
  >/dev/null 2>&1 &
PID=$!

# Watchdog: poll until the engine exits on its own or the deadline passes.
while kill -0 "$PID" 2>/dev/null; do
  if (( $(date +%s) - START > WATCHDOG_SEC )); then
    echo "watchdog: engine still running after ${WATCHDOG_SEC}s, killing pid $PID" >&2
    kill "$PID" 2>/dev/null || true
    sleep 5
    kill -9 "$PID" 2>/dev/null || true
    break
  fi
  sleep 2
done
wait "$PID" || true
echo "engine wall time: $(( $(date +%s) - START ))s"

# Surface the interesting engine log lines (the real log is under ~/Library/Logs).
ENGINE_LOG="$HOME/Library/Logs/Aquarium/Aquarium.log"
if [[ -f "$ENGINE_LOG" ]]; then
  grep -E "LogInit: Engine Version|Fatal|LogInit: Command Line|systemresolution" "$ENGINE_LOG" | head -10 || true
  grep -E "LogTemp: (Warning|Error)" "$ENGINE_LOG" | head -20 || true
fi

COUNT="$(ls "$FRAMES"/UiFrame*.png | wc -l | tr -d ' ')"
echo "frames=$COUNT in $FRAMES"
if (( COUNT < FPS * SECONDS_TO_RUN - FPS )); then
  echo "ERROR: expected about $(( FPS * SECONDS_TO_RUN )) frames, got $COUNT" >&2
  exit 1
fi

# Skip the first SKIP_FRAMES frames: exposure and streaming settle during the
# first couple of ticks, so the head of the clip would otherwise be blown out.
SKIP_FRAMES="${SKIP_FRAMES:-15}"
"$FFMPEG" -y -loglevel error -framerate "$FPS" -start_number "$SKIP_FRAMES" -i "$FRAMES/UiFrame%05d.png" \
  -c:v libx264 -pix_fmt yuv420p -crf 18 "$OUT"
echo "VIDEO_OK $OUT frames=$(( COUNT - SKIP_FRAMES )) (skipped first $SKIP_FRAMES of $COUNT)"

# Review stills: mid-steering leg and the fish riding the swim-plane edge.
"$FFMPEG" -y -loglevel error -ss 5  -i "$OUT" -frames:v 1 "$ROOT/docs/reviews/$DATE-m3-steer.png"
"$FFMPEG" -y -loglevel error -ss 12 -i "$OUT" -frames:v 1 "$ROOT/docs/reviews/$DATE-m3-wall.png"
echo "STILLS_OK $ROOT/docs/reviews/$DATE-m3-{steer,wall}.png"
