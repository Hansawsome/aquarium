#!/usr/bin/env python3
# 반응음 다섯 개를 WAV로 합성한다. 이 스크립트 자체가 출처다(docs/ASSETS.md 참고).
#
# 의존성: 파이썬 표준 라이브러리뿐이다. numpy를 쓰지 않는다 -- 이 기계의 system
# python3에도 UE 번들 python3에도 numpy가 없고(2026-09-21 확인), 소리 다섯 개
# 때문에 의존성을 늘리는 것은 재현성 제1원칙에 손해다.
#
# 결정성: 난수를 쓰되 반드시 고정 시드의 random.Random 인스턴스만 쓴다.
# 전역 random 모듈을 쓰면 실행 순서에 따라 파형이 달라진다.
#
# 실행:  python3 assets/audio/make_sounds.py
# 판정:  마지막 SOUNDS_OK 줄. 종료 코드가 아니라 그 줄을 본다.
import array
import hashlib
import math
import os
import random
import sys
import wave

RATE = 44100
HERE = os.path.dirname(os.path.abspath(__file__))


def clamp(v, lo=-1.0, hi=1.0):
    return lo if v < lo else (hi if v > hi else v)


def write_wav(path, samples, rate=RATE):
    """16비트 모노 PCM. 피크를 0.89로 정규화해 어떤 소리도 클리핑하지 않게 한다."""
    peak = max(abs(s) for s in samples) if samples else 0.0
    gain = (0.89 / peak) if peak > 1e-9 else 0.0
    data = array.array("h", (int(round(clamp(s * gain) * 32767.0)) for s in samples))
    if sys.byteorder == "big":
        data.byteswap()
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(data.tobytes())
    return len(samples)


def fade(samples, in_s=0.004, out_s=0.010, rate=RATE):
    """양 끝을 0으로 내린다. 끝이 0이 아니면 재생 시작·끝에 「딱」 소리가 난다.
    S_Swim은 루프이므로 이 함수가 곧 이음매를 안 들리게 하는 장치이기도 하다."""
    n = len(samples)
    ni = max(1, int(in_s * rate))
    no = max(1, int(out_s * rate))
    for i in range(min(ni, n)):
        samples[i] *= i / ni
    for i in range(min(no, n)):
        samples[n - 1 - i] *= i / no
    return samples


def swim(rate=RATE):
    """1초 루프. 넓은 대역 잡음을 한 옥타브 폭으로 흔드는 필터에 통과시킨 물살 소리.
    음높이 변화는 여기서 굽지 않는다 -- 엔진이 재생 피치로 준다(aquarium::SwimPitch)."""
    rnd = random.Random(20260921)
    n = int(1.0 * rate)
    out = [0.0] * n
    lp1 = lp2 = 0.0
    for i in range(n):
        t = i / n                      # 0..1, 루프 위상
        white = rnd.uniform(-1.0, 1.0)
        # 시간에 따라 컷오프가 오르내리는 2극 저역통과. 물이 밀리는 「쉬—」.
        cut = 0.10 + 0.05 * math.sin(2.0 * math.pi * t)
        lp1 += cut * (white - lp1)
        lp2 += cut * (lp1 - lp2)
        # 아주 낮은 웅— 하나를 섞어 물속의 두께를 만든다.
        hum = 0.18 * math.sin(2.0 * math.pi * 3.0 * t)
        out[i] = 0.85 * lp2 + hum * 0.25
    return fade(out, in_s=0.05, out_s=0.05, rate=rate)


def startle(rate=RATE):
    """「꺅」 0.22초. 700Hz에서 2200Hz로 미끄러져 올라가는 연속음 + 짧은 잡음 숨.
    위로 올라가는 글라이드가 '놀랐다'를 만든다(내려가면 '시무룩'이 된다)."""
    rnd = random.Random(7)
    n = int(0.220 * rate)
    out = [0.0] * n
    phase = 0.0
    for i in range(n):
        t = i / n
        f = 700.0 + (2200.0 - 700.0) * (t ** 0.7)
        phase += 2.0 * math.pi * f / rate
        env = math.exp(-3.2 * t)
        body = math.sin(phase) + 0.30 * math.sin(2.0 * phase)
        breath = 0.22 * rnd.uniform(-1.0, 1.0) * math.exp(-11.0 * t)
        out[i] = env * body * 0.8 + breath
    return fade(out, rate=rate)


def nibble(rate=RATE):
    """「뽁」 0.12초. 320Hz에서 140Hz로 빠르게 떨어지는 타격음. 꺅과 반대 방향·
    반대 길이·반대 대역이라 스피커가 나빠도 구분된다(시나리오 장면 3 조건 1)."""
    n = int(0.120 * rate)
    out = [0.0] * n
    phase = 0.0
    for i in range(n):
        t = i / n
        f = 320.0 * math.exp(-2.1 * t) + 120.0
        phase += 2.0 * math.pi * f / rate
        env = math.exp(-16.0 * t)
        out[i] = env * (math.sin(phase) + 0.18 * math.sin(3.0 * phase))
    return fade(out, in_s=0.001, out_s=0.006, rate=rate)


def split(rate=RATE):
    """「촤악」 0.5초. 넓은 대역 잡음이 확 열렸다가 닫힌다 -- 물이 양쪽으로
    갈라지는 소리. 물고기 한 마리가 아니라 스무 마리가 내는 소리라 길다."""
    rnd = random.Random(31337)
    n = int(0.500 * rate)
    out = [0.0] * n
    hp = prev = 0.0
    for i in range(n):
        t = i / n
        white = rnd.uniform(-1.0, 1.0)
        # 1극 고역통과: 시작은 밝게 「촤」, 뒤로 갈수록 어둡게 「악」.
        a = 0.62 - 0.45 * t
        hp = a * (hp + white - prev)
        prev = white
        env = (1.0 - math.exp(-40.0 * t)) * math.exp(-4.3 * t)
        out[i] = hp * env
    return fade(out, rate=rate)


def bubble(rate=RATE):
    """「뽀글」 0.3초. 서로 다른 크기의 기포 세 개. 내 물고기 재롱에 쓰인다 --
    도망 소리와 절대 같으면 안 된다(시나리오: 내 물고기는 놀라지 않는다)."""
    n = int(0.300 * rate)
    out = [0.0] * n
    for start, f0, f1, amp in (
        (0.00, 480.0, 900.0, 1.00), (0.09, 620.0, 1180.0, 0.72), (0.18, 380.0, 760.0, 0.55)
    ):
        phase = 0.0
        i0 = int(start * rate)
        for i in range(i0, n):
            t = (i - i0) / rate
            # 기포는 튀어 오르며 음이 올라간다(수면으로 갈수록 작아지는 소리).
            f = f0 + (f1 - f0) * (1.0 - math.exp(-24.0 * t))
            phase += 2.0 * math.pi * f / rate
            out[i] += amp * math.exp(-26.0 * t) * math.sin(phase)
    return fade(out, in_s=0.002, out_s=0.010, rate=rate)


SOUNDS = (
    ("S_Swim.wav", swim),
    ("S_Startle.wav", startle),
    ("S_Nibble.wav", nibble),
    ("S_Split.wav", split),
    ("S_Bubble.wav", bubble),
)


def main():
    os.makedirs(HERE, exist_ok=True)
    rows = []
    for name, fn in SOUNDS:
        path = os.path.join(HERE, name)
        samples = fn()
        count = write_wav(path, samples)
        with open(path, "rb") as f:
            digest = hashlib.sha256(f.read()).hexdigest()[:16]
        peak = max(abs(s) for s in samples)
        assert count > 0, name
        assert peak > 1e-6, "%s is silent" % name      # 무음 WAV는 조용히 통과하는 실패다
        rows.append("%s:%d:%s" % (name, count, digest))
        print("  %-14s frames=%6d sha256=%s" % (name, count, digest))
    print("SOUNDS_OK count=%d rate=%d [%s]" % (len(rows), RATE, " ".join(rows)))


if __name__ == "__main__":
    main()
