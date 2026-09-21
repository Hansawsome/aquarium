#!/bin/bash
# THE official SRS performance measurement: a PACKAGED DEVELOPMENT build,
# 30 s warm-up then 3 minutes (SRS line 38). Every earlier figure in this repo
# came from an editor build and is NOT an SRS verdict.
#
# WHY A SEPARATE SCRIPT: measure_m2b_perf.sh defaults to RUN_SEC=95, which leaves
# only 65 s of samples after the warm-up -- barely a third of the SRS three
# minutes. It could never satisfy the SRS no matter how it was invoked, so this
# script bakes in RUN_SEC=210 and REFUSES anything shorter than warm-up + 180 s.
# Without that guard the official verdict silently becomes a 65-second one.
#
# -benchmark is deliberately NOT passed: it fixes the timestep and would make
# DeltaSeconds a useless constant.
#
# The nickname is TEST DATA ONLY -- the engine echoes the command line to its log.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP="${APP:?set APP=/path/to/Aquarium.app}"
BIN="$APP/Contents/MacOS/Aquarium"
MAP="${MAP:-ReefM1}"
RUN_SEC="${RUN_SEC:-210}"          # 30 s warm-up + 180 s measured
WARMUP_SEC="${WARMUP_SEC:-30}"
RESX="${RESX:-1920}"; RESY="${RESY:-1080}"
LABEL="${LABEL:-m6}"
DATE="$(date +%F)"
CSV="$ROOT/docs/reviews/$DATE-$LABEL-frametimes.csv"

# The guard comes FIRST, before the existence check, so that a too-short run is
# refused whether or not the app path happens to be valid.
[[ $RUN_SEC -ge $((WARMUP_SEC + 180)) ]] || {
  echo "ERROR: RUN_SEC=$RUN_SEC is shorter than the SRS warm-up(${WARMUP_SEC}s)+3min" >&2; exit 1; }
[[ -x "$BIN" ]] || { echo "no executable at $BIN" >&2; exit 1; }
# A second instance contends for the GPU and would quietly depress the numbers.
if pgrep -f 'MacOS/Aquarium' >/dev/null 2>&1; then
  echo "ERROR: another Aquarium instance is running; kill it first" >&2
  pgrep -fl 'MacOS/Aquarium' >&2; exit 1
fi

# WHERE THE CSV IS WRITTEN, AND WHY NOT STRAIGHT INTO THE REPO:
# the packaged .app is signed with com.apple.security.app-sandbox and no
# file-access entitlement, so it can only write inside its own container.
# -AquariumFrameStats=<path in the repo> fails SILENTLY -- the run completes
# normally and simply leaves no file, which looks exactly like a crash. So the
# game writes into the container and we move the result out afterwards.
CONTAINER_DOCS="$HOME/Library/Containers/com.YourCompany.Aquarium/Data/Documents"
if [[ -d "$CONTAINER_DOCS" ]]; then
  RAW="$CONTAINER_DOCS/$(basename "$CSV")"
else
  RAW="$CSV"
fi
mkdir -p "$ROOT/docs/reviews"; rm -f "$CSV" "$RAW"
"$BIN" "$MAP" -windowed -ResX="$RESX" -ResY="$RESY" -ForceRes \
  -notexturestreaming -unattended -nosplash -stdout -FullStdOutLogOutput \
  -AquariumAutoNickname="측정" -AquariumAssignmentSeed=1 \
  -AquariumFrameStats="$RAW" ${EXTRA_ARGS:-} >/dev/null 2>&1 &
PID=$!
echo "measuring ${RUN_SEC}s (pid $PID) -> $CSV"
for (( i = 0; i < RUN_SEC; i++ )); do kill -0 "$PID" 2>/dev/null || break; sleep 1; done
# EndPlay writes the CSV, so quit gracefully and give it time.
kill "$PID" 2>/dev/null || true
for (( i = 0; i < 30; i++ )); do kill -0 "$PID" 2>/dev/null || break; sleep 1; done
kill -9 "$PID" 2>/dev/null || true; wait "$PID" 2>/dev/null || true

[[ -f "$RAW" ]] || { echo "ERROR: no frame-time CSV at $RAW (sandbox write denied?)" >&2; exit 1; }
[[ "$RAW" == "$CSV" ]] || mv "$RAW" "$CSV"

CSV="$CSV" WARMUP_SEC="$WARMUP_SEC" LABEL="$LABEL" python3 - <<'PY'
import csv, os
warm = float(os.environ['WARMUP_SEC'])
with open(os.environ['CSV'], newline='') as fh:
    deltas = [float(r['delta_seconds']) for r in csv.DictReader(fh)]
# Discard the warm-up by accumulated wall time, not by sample index: the early
# frames are the slow ones, so an index cut would drop too few of them.
t, kept = 0.0, []
for d in deltas:
    t += d
    if t >= warm:
        kept.append(d)
if not kept:
    raise SystemExit('ERROR: no samples after warm-up -- the run was too short')
measured = sum(kept)
ms = sorted(d * 1000.0 for d in kept)
p95 = ms[min(len(ms) - 1, int(round(0.95 * (len(ms) - 1))))]
avg = len(kept) / measured
print(f"LABEL={os.environ['LABEL']} SAMPLES={len(kept)} MEASURED_SEC={measured:.1f} "
      f"AVG_FPS={avg:.2f} P95_MS={p95:.2f}")
if measured < 180.0:
    print('SRS_GATE=INVALID  measured window is under the SRS 180 s -- re-run')
else:
    print('SRS_GATE=' + ('PASS' if avg >= 60.0 and p95 <= 22.0 else 'FAIL'))
PY
