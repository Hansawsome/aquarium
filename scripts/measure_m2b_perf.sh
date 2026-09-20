#!/bin/bash
# Measures frame times of the M2b reef scene (36 background fish + 14 props) and
# writes docs/reviews/<date>-m2b-perf.md.
#
# The editor target is run in -game mode with the dev-only -AquariumFrameStats flag,
# which records one DeltaSeconds per tick and dumps them as CSV on EndPlay.
#
# NOTE: -benchmark is deliberately NOT passed here. -benchmark fixes the timestep,
# so DeltaSeconds would be a constant 1/fps and completely useless as a measurement.
# -benchmark belongs to the deterministic video capture only; this run uses real time.
#
# The first WARMUP_SEC seconds of samples are discarded (shader compilation, texture
# and level streaming, window creation all happen there).
#
# The nickname passed via -AquariumAutoNickname is TEST DATA ONLY: the engine echoes
# the whole command line into its log, so never put a real user's nickname here.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UE_ROOT="/Users/Shared/Epic Games/UE_5.8"
UE="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor"
PROJ="$ROOT/unreal/Aquarium/Aquarium.uproject"
MAP="${MAP:-ReefM1}"
RUN_SEC="${RUN_SEC:-95}"
WARMUP_SEC="${WARMUP_SEC:-30}"
AUTO_NICKNAME="${AUTO_NICKNAME:-측정}"
ASSIGNMENT_SEED="${ASSIGNMENT_SEED:-1}"
DATE="$(date +%F)"
CSV="$ROOT/docs/reviews/$DATE-m2b-frametimes.csv"
REPORT="$ROOT/docs/reviews/$DATE-m2b-perf.md"

if [[ "${SKIP_BUILD:-0}" != "1" ]]; then
  for pass in 1 2; do
    echo "build pass $pass"
    "$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development \
      -Project="$PROJ" -WaitMutex 2>&1 | grep -E "Result:|error:|Error:" || true
  done
fi

mkdir -p "$ROOT/docs/reviews"
rm -f "$CSV"

"$UE" "$PROJ" "$MAP" -game -windowed -ResX=1920 -ResY=1080 -ForceRes \
  -notexturestreaming -unattended -nosplash -log \
  -AquariumAutoNickname="$AUTO_NICKNAME" -AquariumAssignmentSeed="$ASSIGNMENT_SEED" \
  -AquariumFrameStats="$CSV" \
  >/dev/null 2>&1 &
PID=$!

echo "measuring for ${RUN_SEC}s (pid $PID)"
for (( i = 0; i < RUN_SEC; i++ )); do
  kill -0 "$PID" 2>/dev/null || break
  sleep 1
done

# EndPlay writes the CSV, so ask the engine to quit gracefully and give it time.
kill "$PID" 2>/dev/null || true
for (( i = 0; i < 30; i++ )); do
  kill -0 "$PID" 2>/dev/null || break
  sleep 1
done
kill -9 "$PID" 2>/dev/null || true
wait "$PID" 2>/dev/null || true

if [[ ! -f "$CSV" ]]; then
  echo "ERROR: no frame-time CSV at $CSV" >&2
  exit 1
fi

COMMIT="$(git -C "$ROOT" rev-parse --short HEAD)"
MACHINE="$(sysctl -n machdep.cpu.brand_string)"

CSV="$CSV" REPORT="$REPORT" WARMUP_SEC="$WARMUP_SEC" RUN_SEC="$RUN_SEC" \
COMMIT="$COMMIT" MACHINE="$MACHINE" DATE="$DATE" python3 - <<'PY'
import csv, os, statistics

csv_path, report = os.environ["CSV"], os.environ["REPORT"]
warmup = float(os.environ["WARMUP_SEC"])

deltas = []
with open(csv_path, newline="") as fh:
	for row in csv.DictReader(fh):
		deltas.append(float(row["delta_seconds"]))

# Discard the warm-up window by accumulated wall time, not by sample index:
# the early frames are the slow ones, so an index-based cut would drop too few.
elapsed, kept = 0.0, []
for d in deltas:
	elapsed += d
	if elapsed >= warmup:
		kept.append(d)
if not kept:
	raise SystemExit(f"ERROR: no samples after the {warmup}s warm-up (total {len(deltas)})")

ms = sorted(d * 1000.0 for d in kept)
def pct(p):
	return ms[min(len(ms) - 1, int(round(p / 100.0 * (len(ms) - 1))))]

n = len(kept)
mean_fps = n / sum(kept)
median_fps = 1.0 / statistics.median(kept)
p95, p99, mx = pct(95), pct(99), ms[-1]

with open(report, "w") as fh:
	fh.write(f"""# M2b 성능 측정 ({os.environ['DATE']})

ReefM1 레벨의 프레임 시간을 개발 전용 `-AquariumFrameStats` 플래그로 측정했다.

## 측정 조건

- 해상도: 1920x1080 (`-game -windowed -ForceRes`), 텍스처 스트리밍 끔
- 씬: 배경 물고기 36마리(5종) + 프롭 14개 + 플레이어 물고기 1마리
- 실행 시간 {os.environ['RUN_SEC']}초 중 앞 {int(warmup)}초(워밍업: 셰이더 컴파일·스트리밍)는 버림
- `-benchmark`는 사용하지 않음 — 고정 타임스텝이 되어 DeltaSeconds가 상수가 되므로 측정에 쓸 수 없다
- 커밋: `{os.environ['COMMIT']}`
- 머신: {os.environ['MACHINE']}

## 결과

| 항목 | 값 |
| --- | --- |
| 샘플 수 | {n} |
| 평균 fps | {mean_fps:.1f} |
| 중앙값 fps | {median_fps:.1f} |
| 95백분위 프레임 시간 | {p95:.2f} ms |
| 99백분위 프레임 시간 | {p99:.2f} ms |
| 최대 프레임 시간 | {mx:.2f} ms |

원본 샘플: `{os.path.basename(csv_path)}`

## SRS 목표 대비

SRS 성능 목표는 "평균 60fps, 95백분위 ≤ 22 ms"이며, **판정은 M6**에서 한다.
이 수치는 에디터 바이너리로 측정한 중간 참고값이다.
""")

print(f"PERF_OK samples={n} avg_fps={mean_fps:.1f} p95_ms={p95:.2f}")
PY
