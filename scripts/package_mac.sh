#!/bin/bash
# Packages the macOS game. CONFIG=Development (default) or Shipping.
#
# Output goes OUTSIDE the repository: a packaged build is a multi-GB build
# artefact, and AGENTS.md forbids committing build output. The repo keeps only
# the verification evidence (hashes, logs, the perf report).
#
# KNOWN ISSUE (carried from M0, diagnosed in docs/reviews/2026-09-21-m6-ubt.md):
# UBT's final "App finalization" (xcodebuild PostBuildSync) fails with exit 65
# on this machine, while the identical xcodebuild command succeeds when run
# outside UBT's action executor. If BuildCookRun hits it, this script falls back
# to the supported split: build + finalize by hand + -skipbuild cook/stage.
# The fallback is SCRIPTED on purpose -- a build that needs a human to type a
# command is not reproducible.
#
# The PostBuildSync command is RECOVERED FROM THE BUILD LOG, never from memory.
# Note: the plan pointed at Engine/Programs/UnrealBuildTool/Log.txt, which does
# NOT exist in this UE 5.8.2 install. The real source is Build.sh's own stdout,
# where UBT echoes the invocation on a line starting with "params:".
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
PROJ="$ROOT/unreal/Aquarium/Aquarium.uproject"
CONFIG="${CONFIG:-Development}"
SCRATCH="${SCRATCH:-/private/tmp/claude-501/-Users-hans-dev-aquarium/670aff5b-0935-4cef-86d7-e73d4193a3f3/scratchpad}"
OUT="${OUT:-$SCRATCH/m6_package/$CONFIG}"
LOG="${LOG:-$SCRATCH/m6-package-$CONFIG.log}"
BUILDLOG="$SCRATCH/m6-package-$CONFIG-build.log"

case "$CONFIG" in Development|Shipping) ;; *) echo "CONFIG must be Development or Shipping" >&2; exit 1;; esac

mkdir -p "$OUT" "$(dirname "$LOG")"
echo "packaging $CONFIG -> $OUT (log: $LOG)"

uat() {   # $@ = extra BuildCookRun args
  "$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun \
    -project="$PROJ" \
    -platform=Mac -clientconfig="$CONFIG" -configuration="$CONFIG" \
    -cook -stage -pak -package -archive -archivedirectory="$OUT" \
    -nop4 -utf8output -unattended -nocompileeditor "$@"
}

# ---- recover_postbuildsync_cmd: get the exact xcodebuild line from a log ----
# Never reconstructed by hand. UBT echoes its own invocation on a "params:" line.
recover_postbuildsync_cmd() {
  local src="$1" XC
  XC="$(grep -oE '^params: .*UE_XCODE_BUILD_MODE=PostBuildSync.*' "$src" | tail -1 | sed 's/^params: //')"
  if [[ -z "$XC" ]]; then
    echo "ERROR: could not recover the PostBuildSync command from $src" >&2
    return 1
  fi
  printf '%s' "$XC"
}

set +e
uat -build > "$LOG" 2>&1
RC=$?
set -e

if [[ $RC -ne 0 ]]; then
  echo "BuildCookRun failed (rc=$RC). Checking for the known PostBuildSync wall."
  grep -nE "PostBuildSync|App finalization|Touch UBT generated tiles|ERROR:" "$LOG" | tail -20
  if grep -qE "App finalization|UE_XCODE_BUILD_MODE=PostBuildSync" "$LOG"; then
    echo "FALLBACK: build with PostBuildSync stubbed, finalize by hand, then -skipbuild cook/stage"
    # Recover the real xcodebuild invocation from the failing log BEFORE we stub
    # PostBuildSync out -- once stubbed, UBT never prints it again.
    XC="$(recover_postbuildsync_cmd "$LOG")" || exit 1
    # UE_BUILD_FROM_XCODE=1 makes AppleToolChain's PostBuildSync return 0 without
    # invoking xcodebuild (AppleToolChain.cs: "if xcode is building this, it will
    # also do the Run stuff anyway"). UBT then finishes normally and -- crucially --
    # writes Binaries/Mac/Aquarium.target, the receipt that -skipbuild staging
    # requires. Without this the build aborts before the receipt exists and
    # staging dies with Error_MissingExecutable.
    UE_BUILD_FROM_XCODE=1 "$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" \
      Aquarium Mac "$CONFIG" -Project="$PROJ" -WaitMutex > "$BUILDLOG" 2>&1
    grep -qE '^Result: Succeeded' "$BUILDLOG" || {
      echo "ERROR: the game target failed to build even with PostBuildSync stubbed:" >&2
      tail -30 "$BUILDLOG" >&2; exit 1; }
    RECEIPT="$ROOT/unreal/Aquarium/Binaries/Mac/Aquarium.target"
    [[ -f "$RECEIPT" ]] || { echo "ERROR: no build receipt at $RECEIPT" >&2; exit 1; }
    # Now do the app finalization ourselves. This is the step UBT cannot run.
    echo "recovered PostBuildSync command:" >> "$LOG"
    echo "$XC" >> "$LOG"
    eval "$XC" >> "$LOG" 2>&1
    uat -skipbuild >> "$LOG" 2>&1
  else
    echo "ERROR: BuildCookRun failed for a DIFFERENT reason -- do not assume the known wall" >&2
    tail -30 "$LOG" >&2
    exit 1
  fi
fi

APP="$(find "$OUT" -maxdepth 3 -name 'Aquarium.app' -print -quit)"
[[ -n "$APP" ]] || { echo "ERROR: no Aquarium.app under $OUT" >&2; exit 1; }
BIN="$APP/Contents/MacOS/Aquarium"
[[ -x "$BIN" ]] || { echo "ERROR: no executable at $BIN" >&2; exit 1; }
codesign -dv "$APP" 2>&1 | head -5

echo "APP=$APP"
echo "PACKAGE_OK config=$CONFIG"
