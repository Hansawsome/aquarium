#!/bin/bash
# M6 review artefacts, rendered from the PACKAGED build rather than the editor.
# Cooking changes shaders, texture compression and LODs, so "does it still look
# right" is a question only a real packaged run can answer -- and materials in
# this project have silently fallen back to grey twice.
#
# Same map, seed, camera and auto-input style as the M5 clip so the comparison
# is fair. Auto-input token syntax: <direction-letter><seconds>, letters
# R/L/U/D/0, no colons, no diagonals. The parser only WARNS on junk, so this
# script asserts both warning counts are zero.
#
# ---------------------------------------------------------------------------
# THE PACKAGED APP IS SANDBOXED -- TWO PATHS THE PLAN GOT WRONG
# ---------------------------------------------------------------------------
# 1. It does NOT log to ~/Library/Logs/Aquarium. It logs inside its container:
#      ~/Library/Containers/com.YourCompany.Aquarium/Data/Library/Logs/Aquarium/
#    Grepping the non-container path returns 0 hits whether or not materials
#    failed -- an absence check that cannot bite (project convention 7).
# 2. -AquariumCaptureUI pointed anywhere outside the container FAILS SILENTLY.
#    Frames simply never appear and the run looks like a crash. So the capture
#    directory lives inside Data/ too.
# Do NOT rm -rf and recreate the container Logs directory: ownership changes and
# the sandbox then denies every write, silently.
#
# Shipping cannot be captured this way at all -- it has no dev flags. That is
# deliberate, and it is why item 5 of the manual hand-over list exists.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP="${APP:?set APP=/path/to/Aquarium.app}"
BIN="$(find "$APP/Contents/MacOS" -maxdepth 1 -type f -perm -u+x -print -quit)"
[[ -n "$BIN" ]] || { echo "ERROR: no executable in $APP/Contents/MacOS" >&2; exit 1; }
FFMPEG="${FFMPEG:-/opt/homebrew/bin/ffmpeg}"
REVIEWS="$ROOT/docs/reviews"
CONTAINER="${CONTAINER:-$HOME/Library/Containers/com.YourCompany.Aquarium/Data}"
FRAMES="${FRAMES:-$CONTAINER/AquariumFrames}"
GLOG="${GLOG:-$CONTAINER/Library/Logs/Aquarium/Aquarium.log}"
LOG="${LOG:-/tmp/m6-capture.log}"
MAP="${MAP:-ReefM1}"
FPS="${FPS:-30}"
SECS="${SECS:-30}"
DATE="$(date +%F)"
BEFORE="${BEFORE:-$REVIEWS/2026-09-21-m5-flee.png}"
AUTO_INPUT="${AUTO_INPUT:-R3,U2,L3,D2,0 2,R2,U2,0 2,0 14}"
AUTO_NICKNAME="${AUTO_NICKNAME:-니모}"   # TEST DATA ONLY -- echoed into the log

# M4b compared a milestone against ITSELF and the hstack still looked plausible.
case "$BEFORE" in *m6*) echo "ERROR: BEFORE points at an m6 artefact -- that compares M6 with itself" >&2; exit 1;; esac
[[ -f "$BEFORE" ]] || { echo "ERROR: no BEFORE still at $BEFORE" >&2; exit 1; }

rm -rf "$FRAMES"; mkdir -p "$FRAMES"
rm -f "$GLOG"

"$BIN" "$MAP" -windowed -ResX=1920 -ResY=1080 -ForceRes \
  -benchmark -fps="$FPS" -seconds="$SECS" -notexturestreaming \
  -unattended -nosplash -log \
  -AquariumAutoNickname="$AUTO_NICKNAME" -AquariumAssignmentSeed=1 \
  -AquariumAutoInput="$AUTO_INPUT" -AquariumCaptureUI="$FRAMES" \
  > "$LOG" 2>&1 || true

[[ -f "$GLOG" ]] || { echo "ERROR: no packaged log at $GLOG -- the material check would be blind" >&2; exit 1; }
# The pattern is NARROWER than the obvious one, and deliberately so. Grepping
# for a bare "WorldGridMaterial|Default Material" matches this engine line, which
# is a loader note and not a failure at all:
#   LogStreaming: Display: Partially loaded package /Engine/EngineMaterials/
#   WorldGridMaterial ... recursively flushed by .../DefaultTextMaterialOpaque
# It fired on the very first packaged run. A check that goes red on a healthy
# build gets weakened until it checks nothing, so it is narrowed here to the
# messages UE actually prints when a material falls back.
MATERIAL=$(grep -ciE "Failed to compile Material|Default Material will be used|Fallback material|Material .* failed to compile" "$GLOG" || true)
# ...and narrowing an absence check needs a positive control, or it becomes the
# fourth check in this project that passes while verifying nothing: assert the
# grep is reading a real, populated packaged log.
CONTROL=$(grep -c "LogInit" "$GLOG" || true)
[[ "$CONTROL" -gt 0 ]] || { echo "ERROR: control grep found no LogInit -- the material scan is blind" >&2; exit 1; }
echo "MATERIAL_SCAN_CONTROL_LogInit=$CONTROL"
IN_UNK=$(grep -c "AquariumAutoInput: unknown direction" "$GLOG" || true)
IN_BAD=$(grep -c "AquariumAutoInput: bad duration" "$GLOG" || true)
NFRAMES=$(ls "$FRAMES" | wc -l | tr -d ' ')
echo "MATERIAL_COMPILE_FAILURES=$MATERIAL AUTOINPUT_UNKNOWN=$IN_UNK AUTOINPUT_BAD_DURATION=$IN_BAD FRAMES=$NFRAMES"
[[ "$MATERIAL" == "0" ]] || { echo "ERROR: materials failed to compile in the PACKAGED build" >&2; exit 1; }
[[ "$IN_UNK" == "0" && "$IN_BAD" == "0" ]] || { echo "ERROR: auto-input script was silently truncated" >&2; exit 1; }
[[ "$NFRAMES" -gt $((FPS * SECS / 2)) ]] || { echo "ERROR: only $NFRAMES frames captured" >&2; exit 1; }

mkdir -p "$REVIEWS"
"$FFMPEG" -y -framerate "$FPS" -pattern_type glob -i "$FRAMES/*.png" \
  -c:v libx264 -pix_fmt yuv420p "$REVIEWS/$DATE-m6-package.mp4" >/dev/null 2>&1
echo "VIDEO_OK"
STILL="$(ls "$FRAMES"/*.png | sed -n "$((FPS * 12))p")"
cp "$STILL" "$REVIEWS/$DATE-m6-scene.png"; echo "STILL_OK"
"$FFMPEG" -y -i "$BEFORE" -i "$REVIEWS/$DATE-m6-scene.png" -filter_complex hstack \
  "$REVIEWS/$DATE-m6-compare.png" >/dev/null 2>&1
echo "COMPARE_OK before=$BEFORE"
