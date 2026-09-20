#!/bin/bash
# Renders a deterministic 30 fps, 35 s run of ReefM1 to PNG frames with the
# engine's own -dumpmovie path (no macOS screen recording needed), then
# encodes them with ffmpeg into docs/reviews/<date>-m1-swim.mp4.
#
# Notes (verified on UE 5.8.2 / macOS):
#   - Uses the EDITOR binary in -game mode, so uncooked content works.
#   - -benchmark -fps=30 fixes the timestep (one frame per tick), and
#     -seconds=35 exits the process by itself (~105 s wall time).
#   - -ForceRes is required: otherwise Saved/Config/.../GameUserSettings.ini
#     overrides -ResX/-ResY and frames come out at 1280x720.
#   - Frames land in Saved/Screenshots/MacEditor/MovieFrame%05d.png.
#   - A watchdog kills the engine if it has not exited after WATCHDOG_SEC.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UE="/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor"
FFMPEG="${FFMPEG:-/opt/homebrew/bin/ffmpeg}"
PROJ="$ROOT/unreal/Aquarium/Aquarium.uproject"
SHOTS="$ROOT/unreal/Aquarium/Saved/Screenshots"
MAP="${MAP:-ReefM1}"
FPS="${FPS:-30}"
SECONDS_TO_RUN="${SECONDS_TO_RUN:-35}"
WATCHDOG_SEC="${WATCHDOG_SEC:-300}"
DATE="$(date +%F)"
OUT="$ROOT/docs/reviews/$DATE-m1-swim.mp4"

rm -rf "$SHOTS"
mkdir -p "$ROOT/docs/reviews"

START=$(date +%s)
"$UE" "$PROJ" "$MAP" -game -windowed -ResX=1920 -ResY=1080 -ForceRes \
  -benchmark -fps="$FPS" -seconds="$SECONDS_TO_RUN" -dumpmovie \
  -notexturestreaming -unattended -nosplash -log >/dev/null 2>&1 &
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
fi

FRAMES="$(ls -d "$SHOTS"/*/ | head -1)"
COUNT="$(ls "$FRAMES"/MovieFrame*.png | wc -l | tr -d ' ')"
echo "frames=$COUNT in $FRAMES"
if (( COUNT < FPS * SECONDS_TO_RUN - FPS )); then
  echo "ERROR: expected about $(( FPS * SECONDS_TO_RUN )) frames, got $COUNT" >&2
  exit 1
fi

"$FFMPEG" -y -loglevel error -framerate "$FPS" -pattern_type glob -i "$FRAMES/MovieFrame*.png" \
  -c:v libx264 -pix_fmt yuv420p -crf 18 "$OUT"
echo "VIDEO_OK $OUT frames=$COUNT"
