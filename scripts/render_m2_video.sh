#!/bin/bash
# Renders a deterministic 30 fps, 14 s capture of the M2 flow
# (entry widget -> session with name tag -> exit -> entry widget again)
# with the dev-only -AquariumCaptureUI frame capture (one UI-inclusive screenshot
# per fixed-timestep tick; -dumpmovie is NOT used because the engine drops the
# Slate/UMG layer while dumping a movie), encodes it with ffmpeg into
# docs/reviews/<date>-m2-flow.mp4 and pulls three review stills out of it.
#
# Timeline (driven by the dev-only auto-replay flags of ADiverPlayerController):
#   0-2 s   entry widget (nickname panel)      -> <date>-m2-entry.png      (t=1)
#   2-10 s  session, player fish with name tag -> <date>-m2-nametag.png    (t=6)
#   10-14 s entry widget again after exit      -> <date>-m2-after-exit.png (t=12.5)
#
# The nickname passed via -AquariumAutoNickname is TEST DATA ONLY: the engine
# echoes the whole command line into its log, so never put a real user's
# nickname here.
#
# Notes (verified on UE 5.8.2 / macOS), same as render_m1_video.sh:
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
SECONDS_TO_RUN="${SECONDS_TO_RUN:-14}"
WATCHDOG_SEC="${WATCHDOG_SEC:-300}"
# Test-only nickname and the exit delay (seconds after the auto submit).
AUTO_NICKNAME="${AUTO_NICKNAME:-니모}"
AUTO_EXIT_AFTER="${AUTO_EXIT_AFTER:-8}"
DATE="$(date +%F)"
OUT="$ROOT/docs/reviews/$DATE-m2-flow.mp4"

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
  -AquariumAutoNickname="$AUTO_NICKNAME" -AquariumAutoExitAfter="$AUTO_EXIT_AFTER" \
  -AquariumCaptureUI="$FRAMES" \
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

# Review stills: entry screen, session with name tag, entry screen after exit.
"$FFMPEG" -y -loglevel error -ss 1    -i "$OUT" -frames:v 1 "$ROOT/docs/reviews/$DATE-m2-entry.png"
"$FFMPEG" -y -loglevel error -ss 6    -i "$OUT" -frames:v 1 "$ROOT/docs/reviews/$DATE-m2-nametag.png"
"$FFMPEG" -y -loglevel error -ss 12.5 -i "$OUT" -frames:v 1 "$ROOT/docs/reviews/$DATE-m2-after-exit.png"
echo "STILLS_OK $ROOT/docs/reviews/$DATE-m2-{entry,nametag,after-exit}.png"
