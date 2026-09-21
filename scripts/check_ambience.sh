#!/bin/bash
# 앰비언스가 실재하고 출처가 기록돼 있는지 본다. 없으면 무엇을 어디에 두어야
# 하는지 명확히 말하고 실패한다 -- 조용히 앰비언스 없이 출하되는 것이 이
# 마일스톤에서 가장 눈에 안 띄는 실패다.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WAV="$ROOT/assets/audio/ambience/A_Underwater.wav"
SRC="$ROOT/assets/audio/ambience/SOURCE.md"

if [[ ! -f "$WAV" ]]; then
  cat >&2 << 'MSG'
ERROR: 배경 앰비언스가 없다.

  기대 경로 : assets/audio/ambience/A_Underwater.wav
  형식      : WAV(PCM), 16비트, 44.1 또는 48 kHz, 20초 이상, 이음매 없이 반복 가능
  라이선스  : CC0 (다른 라이선스는 쓰지 않는다)
  내용      : 웅- 하는 먹먹한 저음 + 보글보글, 돌고래/파도/음악 없음

  파일을 놓은 뒤 assets/audio/ambience/SOURCE.md의 모든 항목을 채우고
  docs/ASSETS.md의 "도입한 외부 에셋" 표에 행을 추가한다.

  이 검사를 건너뛰고 진행하지 않는다. 앰비언스 없이 나가면 시나리오 장면 1이
  절반만 구현된 채로 출하되고, 그것은 부분 구현이 아니라 잘못된 평가를 낳는다.
MSG
  exit 1
fi

# 길이·형식은 추측하지 않고 실제 헤더에서 읽는다.
python3 - "$WAV" << 'PY'
import sys, wave
with wave.open(sys.argv[1], "rb") as w:
    rate = w.getframerate(); n = w.getnframes(); ch = w.getnchannels(); sw = w.getsampwidth()
secs = n / float(rate)
print("AMBIENCE ch=%d rate=%d width=%d frames=%d seconds=%.2f" % (ch, rate, sw, n, secs))
assert rate in (44100, 48000), "unexpected sample rate: %d" % rate
assert sw == 2, "expected 16-bit PCM, got %d bytes/sample" % sw
assert secs >= 20.0, "ambience is only %.2f s; need >= 20 s" % secs
PY

for field in "저작자:" "원본 URL:" "다운로드일:"; do
  line="$(grep -F "$field" "$SRC" || true)"
  value="${line#*"$field"}"
  if [[ -z "${value// /}" ]]; then
    echo "ERROR: SOURCE.md의 '$field' 항목이 비어 있다 (docs/ASSETS.md 필수 기록)" >&2
    exit 1
  fi
done
grep -qF "A_Underwater" "$ROOT/docs/ASSETS.md" || { echo "ERROR: docs/ASSETS.md에 앰비언스 행이 없다" >&2; exit 1; }
echo "AMBIENCE_OK"
