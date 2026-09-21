#!/bin/bash
# Checks that a student nickname never reaches disk. SRS line 52 / AGENTS.md.
#
# ---------------------------------------------------------------------------
# WHERE A PACKAGED BUILD ACTUALLY WRITES ITS LOG
# ---------------------------------------------------------------------------
# The plan (and an earlier diagnosis) said the packaged build writes no log:
# nothing ever appeared at ~/Library/Logs/Aquarium even with -log. That was the
# wrong conclusion from a true observation. The packaged .app is App-Sandboxed,
# so every one of its writes is redirected into its container:
#
#   ~/Library/Containers/com.YourCompany.Aquarium/Data/Library/Logs/Aquarium/Aquarium.log
#
# Two flags are needed, not one: measured on this build, `-log` alone produces
# no file at all, while `-stdout -FullStdOutLogOutput` does write it. So the log
# is both in an unexpected place AND behind a different flag.
# Any privacy scan aimed only at ~/Library/Logs/Aquarium would find nothing --
# and "found nothing" is exactly what a passing privacy check looks like. That
# is why AUTO mode below fails on ZERO hits as well as on wrong-place hits.
#
# ---------------------------------------------------------------------------
# TWO MODES, AND THE DISTINCTION IS THE WHOLE POINT
# ---------------------------------------------------------------------------
#   auto   -- runs the packaged build with -AquariumAutoNickname=<token>.
#             The engine echoes its WHOLE COMMAND LINE into the log (LogInit:
#             Command Line: ... and LogCsvProfiler metadata), so the token WILL
#             be found. Demanding "0 hits" here would be a check that can only
#             ever be red, and a permanently red check gets weakened until it
#             checks nothing. So this mode asserts something else, and true:
#             EVERY hit must sit on a command-line echo line, AND there must be
#             at least one hit. A hit anywhere else means the GAME wrote the
#             nickname -- the defect. Zero hits means the scan is blind.
#
#   manual -- the user typed the nickname into the packaged app by hand, so the
#             command line never carried it. Here 0 hits is the correct and
#             meaningful assertion. THIS is the honest answer to SRS line 52.
set -euo pipefail

MODE="${1:?usage: verify_privacy.sh auto <Aquarium.app> | manual <token>}"
TOKEN_DEFAULT="별명검사토큰QX7"
CONTAINER="$HOME/Library/Containers/com.YourCompany.Aquarium/Data"
RUN_SEC="${RUN_SEC:-50}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FAIL=0

# Every place a run of this game -- packaged (sandboxed) or editor -- can write.
#
# EXCLUSION, with its reason: Epic/UnrealEngine/Common is the shared Derived Data
# Cache (1.0 GB here). It holds compiled shaders and cooked asset blobs keyed by
# content hash; no gameplay string is ever written there, and grepping it took
# the scan past ten minutes -- a check too slow to run is a check that stops
# being run. The project's own Saved/ is narrowed the same way and for the same
# reason: Saved/{UiFrames,StagedBuilds,Screenshots,Cooked,Shaders} hold ~5 GB of
# build and capture artefacts, while only Saved/{Logs,Config,SaveGames,Crashes}
# are session output.
#
# ~/Library/Preferences is NOT scanned wholesale either: `grep -r` over it never
# returns on this machine (measured: still running after 400 s on 4.6 MB -- some
# entries there are TCC-guarded and block the reader indefinitely). A check that
# hangs is worse than one that finds nothing, because it gets commented out. The
# app's own preference domains are scanned by name instead, and the sandboxed
# build can only write into its container's Preferences anyway.
# Everything a session actually writes (logs, Saved/Config,
# GameUserSettings, preferences) is inside the roots below.
scan_roots() {
  printf '%s\n' \
    "$CONTAINER/Library/Logs" \
    "$CONTAINER/Library/Application Support" \
    "$CONTAINER/Library/Preferences" \
    "$HOME/Library/Logs/Aquarium" \
    "$HOME/Library/Application Support/Epic/Aquarium" \
    "$HOME/Library/Application Support/Epic/Epic Games" \
    "$HOME/Library/Application Support/Epic/UnrealEngine/5.8" \
    "$HOME/Library/Application Support/Epic/UnrealEngine/Editor" \
    "$ROOT/unreal/Aquarium/Saved/Logs" \
    "$ROOT/unreal/Aquarium/Saved/Config" \
    "$ROOT/unreal/Aquarium/Saved/SaveGames" \
    "$ROOT/unreal/Aquarium/Saved/Crashes"
}

# the app's own preference domains, scanned by name (see note above)
pref_files() {
  ls -1 "$HOME/Library/Preferences"/*Aquarium*.plist \
        "$HOME/Library/Preferences"/*YourCompany*.plist \
        "$HOME/Library/Preferences"/*epicgames*.plist 2>/dev/null || true
}

scan() {  # $1 = token -> prints matching file paths
  local t="$1" r f
  {
    while read -r r; do
      [[ -e "$r" ]] || continue
      grep -rl -a -- "$t" "$r" 2>/dev/null || true
    done < <(scan_roots)
    while read -r f; do
      [[ -f "$f" ]] && grep -l -a -- "$t" "$f" 2>/dev/null || true
    done < <(pref_files)
  } | sort -u
}

case "$MODE" in
auto)
  APP="${2:?usage: verify_privacy.sh auto <Aquarium.app>}"
  BIN="$APP/Contents/MacOS/Aquarium"
  TOKEN="${TOKEN:-$TOKEN_DEFAULT}"
  [[ -x "$BIN" ]] || { echo "no executable at $BIN" >&2; exit 1; }
  # A second instance cannot take the log file, segfaults, and leaves this check
  # reporting AUTO_TOTAL=0 -- a false red that looks exactly like a blind scan.
  # Observed once during M6. Refuse rather than measure a contended run.
  if pgrep -f 'MacOS/Aquarium' >/dev/null 2>&1; then
    echo "ERROR: another Aquarium instance is already running; kill it first" >&2
    pgrep -fl 'MacOS/Aquarium' >&2; exit 1
  fi
  # Clear the log FILES only. Never rm -rf or mkdir the sandbox container's
  # Logs/Aquarium directory: a directory recreated by hand gets ordinary
  # ownership and the sandbox then refuses every write SILENTLY -- the app runs,
  # prints nothing at all, and this check reports a false AUTO_TOTAL=0 that looks
  # exactly like a blind scan. Cost an hour in M6. Let the app create it.
  for d in "$CONTAINER/Library/Logs/Aquarium" "$HOME/Library/Logs/Aquarium"; do
    [[ -d "$d" ]] && find "$d" -type f -name '*.log' -delete 2>/dev/null || true
  done
  "$BIN" ReefM1 -windowed -ResX=1280 -ResY=720 -ForceRes \
    -unattended -nosplash -stdout -FullStdOutLogOutput -AquariumAssignmentSeed=1 \
    -AquariumAutoNickname="$TOKEN" -AquariumAutoExitAfter=10 >/dev/null 2>&1 &
  GP=$!
  for (( i = 0; i < RUN_SEC; i++ )); do kill -0 "$GP" 2>/dev/null || break; sleep 1; done
  kill "$GP" 2>/dev/null || true; sleep 3; kill -9 "$GP" 2>/dev/null || true

  FILES="$(scan "$TOKEN")"
  echo "files containing the token:"; echo "${FILES:-  (none)}"
  TOTAL=0; ECHO_LINES=0
  while read -r f; do
    [[ -n "$f" ]] || continue
    n=$(grep -a -c -- "$TOKEN" "$f" || true)
    e=$(grep -a -- "$TOKEN" "$f" | grep -c -- '-AquariumAutoNickname=' || true)
    TOTAL=$((TOTAL+n)); ECHO_LINES=$((ECHO_LINES+e))
    echo "  $f: $n hits, $e on command-line echo lines"
    # || true: with set -e, a grep that finds no offending line would abort the
    # script right before the verdict -- and a check that never prints its
    # verdict is a check that is not being read.
    grep -a -n -- "$TOKEN" "$f" | grep -v -- '-AquariumAutoNickname=' | head -5 || true
  done <<< "$FILES"
  echo "AUTO_TOTAL=$TOTAL AUTO_ECHO=$ECHO_LINES"
  if [[ $TOTAL -eq 0 ]]; then
    echo "FAIL: the token was not found even on the command-line echo line."
    echo "      The scan is looking in the wrong place (see the sandbox note in"
    echo "      this file's header), so a clean manual run would prove nothing."
    FAIL=1
  elif [[ $TOTAL -ne $ECHO_LINES ]]; then
    echo "FAIL: the nickname appears OUTSIDE the command-line echo -- the game wrote it."
    FAIL=1
  else
    echo "PRIVACY_AUTO_OK: every occurrence is the engine echoing its own command line."
  fi
  ;;
manual)
  TOKEN="${2:?usage: verify_privacy.sh manual <token-the-user-typed>}"
  FILES="$(scan "$TOKEN")"
  if [[ -z "$FILES" ]]; then
    echo "PRIVACY_OK: '$TOKEN' appears in no log, config or preference file."
    echo "  (scanned: $(scan_roots | tr '\n' ' '))"
  else
    echo "FAIL: the hand-typed nickname reached disk:"; echo "$FILES"
    while read -r f; do [[ -n "$f" ]] && grep -a -n -- "$TOKEN" "$f" | head -5; done <<< "$FILES"
    FAIL=1
  fi
  ;;
*) echo "unknown mode $MODE" >&2; exit 1;;
esac

[[ $FAIL -eq 0 ]] || exit 1
