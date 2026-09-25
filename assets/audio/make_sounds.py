#!/usr/bin/env python3
# 반응음 여섯 개를 WAV로 합성한다. 이 스크립트 자체가 출처다(docs/ASSETS.md 참고).
#
# 의존성: 파이썬 표준 라이브러리뿐이다. numpy를 쓰지 않는다 -- 이 기계의 system
# python3에도 UE 번들 python3에도 numpy가 없고(2026-09-21 확인), 소리 여섯 개
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
    """1초 루프. **물방울이다** -- 개정 전에는 넓은 대역 잡음을 흔든 「푸슝」이었고,
    사용자가 실제로 플레이한 뒤 "푹숑보다 물방울 소리가 좋을 듯"이라고 했다.

    물방울 하나는 **짧고 음이 올라가는** 사인이다(공동이 닫히며 주파수가 오른다).
    그것이 잡음과 다른 점이고, 무게중심이 그 차이를 잰다 -- 옛 잡음 글라이드는
    2173Hz였고 물방울은 그 절반 아래다. 밝기를 눌러 둔 이유는 시나리오의 금지
    목록에 '동글동글 귀여운 기포'가 있기 때문이다: 방울은 낮고 짧고 옅다.

    **이음매.** 루프이므로 fade()를 쓰지 않는다 -- 양 끝을 0으로 내리면 1초마다
    소리가 꺼졌다 켜진다. 대신 모든 성분을 **주기적으로** 쓴다: 방울은 버퍼 끝을
    넘으면 앞으로 감아 이어 쓰고(원형 기록), 낮은 웅—은 루프당 정수 주기이며,
    물살 잡음 바닥은 자기 자신과 원형 교차 페이드해 이음매를 지운다.

    음높이 변화는 여기서 굽지 않는다 -- 엔진이 재생 피치로 준다(aquarium::SwimPitch).
    빨리 헤엄칠수록 방울이 높아지고 **동시에 촘촘해진다**(루프 전체가 빨라지므로)."""
    rnd = random.Random(20260922)
    n = int(1.0 * rate)
    out = [0.0] * n

    # --- 물살 바닥. 아주 옅은 저역 잡음 한 겹. 방울이 놓일 물이다.
    m = n + int(0.25 * rate)           # 교차 페이드에 쓸 꼬리만큼 더 만든다
    bed = [0.0] * m
    lp1 = lp2 = 0.0
    for i in range(m):
        w = rnd.uniform(-1.0, 1.0)
        lp1 += 0.045 * (w - lp1)       # 2극 저역통과: 「쉬—」가 아니라 「무—」
        lp2 += 0.045 * (lp1 - lp2)
        bed[i] = lp2
    xf = int(0.25 * rate)
    for i in range(n):
        if i < xf:                      # 앞 xf 구간을 꼬리와 섞어 이음매를 지운다
            a = i / xf
            out[i] += 2.6 * (bed[i] * a + bed[i + n] * (1.0 - a))
        else:
            out[i] += 2.6 * bed[i]

    # --- 물속의 두께. 루프당 정수 주기라 이음매가 없다.
    for i in range(n):
        out[i] += 0.055 * math.sin(2.0 * math.pi * 3.0 * i / n)

    # --- 방울들. 루프 위상, 시작 주파수, 크기. 일정 간격이면 점선으로 들리므로
    #     불규칙하게 놓는다. 여덟 개면 헤엄치는 동안 끊기지 않고 흐른다.
    drops = (
        (0.02, 430.0, 0.95), (0.14, 610.0, 0.62), (0.26, 360.0, 0.80),
        (0.41, 720.0, 0.50), (0.53, 480.0, 0.88), (0.66, 300.0, 0.70),
        (0.78, 560.0, 0.55), (0.90, 400.0, 0.85),
    )
    for phase0, f0, amp in drops:
        i0 = int(phase0 * n)
        dn = int(0.075 * rate)          # 75ms. 이보다 길면 '뽁'이 아니라 '삐'가 된다
        ph = 0.0
        for k in range(dn):
            t = k / rate
            # 공동이 닫히며 주파수가 **오른다**. 이것이 물방울의 서명이다.
            f = f0 * (1.0 + 1.6 * (1.0 - math.exp(-55.0 * t)))
            ph += 2.0 * math.pi * f / rate
            env = math.exp(-42.0 * t) * (1.0 - math.exp(-900.0 * t))
            out[(i0 + k) % n] += amp * 0.30 * env * math.sin(ph)
    return out


def startle(rate=RATE):
    """「퍽」 0.13초. **개정 전에는 700→2200Hz로 올라가는 「꺅」이었다.** 대상 나이가
    초등 5~6학년으로 바뀌면서 그 톤이 시나리오의 금지 목록(만화 비명)에 올랐다.
    이제 방향이 반대다: 240Hz에서 90Hz로 **떨어지는** 짧은 몸통 소리 + 물이 밀리는
    잡음 한 겹. 올라가면 '꺅', 내려가면 '퍽'이다 -- 이 한 줄이 유치함의 분기점이다."""
    rnd = random.Random(7)
    n = int(0.130 * rate)
    out = [0.0] * n
    phase = 0.0
    lp = lp2 = lp3 = 0.0
    for i in range(n):
        t = i / n
        f = 240.0 * math.exp(-5.0 * t) + 90.0
        phase += 2.0 * math.pi * f / rate
        env = math.exp(-13.0 * t)
        body = math.sin(phase) + 0.22 * math.sin(2.0 * phase)
        # 물이 밀리는 저역 잡음. **3극** 저역통과다 -- 계획서 원안의 1극(6dB/oct)은
        # 고역을 그대로 남겨 무게중심이 3.2kHz였다(자기 목표 900Hz를 못 넘겼다).
        # 가중치가 2.0인 이유: 더 키우면 잡음이 무게중심을 혼자 끌어내려, 옛 「꺅」
        # 글라이드로 되돌리는 변이에도 측정이 900Hz 아래로 통과해 버린다(실측 857Hz).
        # **몸통이 지배해야 이 측정이 무언가를 증명한다.**
        w = rnd.uniform(-1.0, 1.0)
        lp += 0.05 * (w - lp)
        lp2 += 0.05 * (lp - lp2)
        lp3 += 0.05 * (lp2 - lp3)
        out[i] = env * (body * 0.85 + lp3 * 2.0)
    return fade(out, in_s=0.001, out_s=0.008, rate=rate)


def thud(rate=RATE):
    """「쿵」 0.18초. 잡았을 때만 난다. startle보다 **한 옥타브 아래에서 시작하고
    더 길다** -- 아이가 '비켰다'와 '잡았다'를 소리만으로 구분해야 하기 때문이다.
    타격감의 나머지 9할은 화면 흔들림이 만든다(Impact.h)."""
    rnd = random.Random(911)
    n = int(0.180 * rate)
    out = [0.0] * n
    phase = 0.0
    lp = lp2 = lp3 = 0.0
    for i in range(n):
        t = i / n
        f = 120.0 * math.exp(-4.0 * t) + 52.0
        phase += 2.0 * math.pi * f / rate
        env = math.exp(-9.0 * t)
        w = rnd.uniform(-1.0, 1.0)
        lp += 0.018 * (w - lp)
        lp2 += 0.018 * (lp - lp2)
        lp3 += 0.018 * (lp2 - lp3)
        # 첫 30ms의 어택이 '맞았다'를 만든다. **잡음이 아니라 낮은 과도음**이다 --
        # 잡음 클릭은 무게중심을 4kHz까지 끌어올려 「쿵」이 아니라 「탁」이 된다.
        click = math.exp(-26.0 * t) * math.sin(phase * 2.2) * 0.15
        # 잡음 가중치가 3.0인 이유는 startle과 같다 -- 몸통이 지배해야 측정이 증명이 된다.
        out[i] = env * (math.sin(phase) * 1.0 + lp3 * 3.0 + click)
    return fade(out, in_s=0.0005, out_s=0.012, rate=rate)


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
    ("S_Thud.wav", thud),
    ("S_Nibble.wav", nibble),
    ("S_Split.wav", split),
    ("S_Bubble.wav", bubble),
)


# S_Swim이 **물방울인지 푸슝인지**를 숫자로 가른다. 귀로는 확인할 수 없으므로
# 이 두 줄이 유일한 자동 증거다.
#
# ① 무게중심 상한. 잡음이 지배하면 무게중심이 올라간다 -- 개정 전 「푸슝」은 같은
#    측정에서 2173.1Hz였고 물방울은 993.6Hz다. 옛 swim()으로 되돌리는 변이에서
#    실제로 빨간불을 확인했다(2173.1 > 1400).
# ② 이음매. 루프의 마지막 표본과 첫 표본 사이의 단차가 보통 단차보다 크면 1초마다
#    「딱」이 난다. fade()로 양 끝을 0으로 내리는 방식은 단차는 없애지만 1초마다
#    소리가 꺼졌다 켜지므로 쓰지 않는다 -- 그래서 파형이 **실제로 주기적**이어야 한다.
SWIM_CENTROID_MAX_HZ = 1400.0


def swim_centroid_hz(path):
    """measure_centroid.py와 같은 계산. 그 스크립트를 import하면 __main__에서
    sys.argv를 훑으므로 여기서 다시 쓴다(같은 식, 같은 제로패딩)."""
    import cmath
    with wave.open(path) as w:
        n = w.getnframes()
        rate = w.getframerate()
        d = array.array("h")
        d.frombytes(w.readframes(n))
    xs = [v / 32768.0 for v in d]
    N = 1
    while N < len(xs):
        N *= 2
    xs += [0.0] * (N - len(xs))

    def fft(a):
        m = len(a)
        if m == 1:
            return a
        e = fft(a[0::2])
        o = fft(a[1::2])
        out = [0j] * m
        for k in range(m // 2):
            t = cmath.exp(-2j * math.pi * k / m) * o[k]
            out[k] = e[k] + t
            out[k + m // 2] = e[k] - t
        return out

    sp = fft([complex(v, 0) for v in xs])
    num = den = 0.0
    for k in range(1, N // 2):
        mag = abs(sp[k])
        num += mag * (k * rate / N)
        den += mag
    return num / den if den else 0.0


def check_swim_loop(path):
    with wave.open(path) as w:
        n = w.getnframes()
        d = array.array("h")
        d.frombytes(w.readframes(n))
    steps = sorted(abs(d[i + 1] - d[i]) for i in range(n - 1))
    seam = abs(d[0] - d[n - 1])
    typical = steps[int(0.99 * len(steps))]
    assert seam <= typical, \
        "S_Swim loop seam jumps %d, above the 99th-percentile step %d" % (seam, typical)
    c = swim_centroid_hz(path)
    assert c < SWIM_CENTROID_MAX_HZ, \
        "S_Swim centroid %.1f Hz >= %.1f Hz -- that is noise, not droplets" % (c, SWIM_CENTROID_MAX_HZ)
    return seam, typical, c


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
    seam, typical, cent = check_swim_loop(os.path.join(HERE, "S_Swim.wav"))
    print("  S_Swim loop seam=%d (p99 step=%d) centroid=%.1f Hz" % (seam, typical, cent))
    print("SOUNDS_OK count=%d rate=%d [%s]" % (len(rows), RATE, " ".join(rows)))


if __name__ == "__main__":
    main()
