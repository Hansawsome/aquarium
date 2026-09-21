#!/usr/bin/env python3
# 「꺅」이 「퍽」이 되었다는 주장은 귀가 아니라 여기서 증명된다.
#
# 실행:  python3 assets/audio/measure_centroid.py assets/audio/S_Startle.wav ...
# 판정:  S_Startle과 S_Thud의 무게중심이 **900Hz 아래**이고 Thud가 더 낮을 것.
#        개정 전 「꺅」(700->2200Hz 상승 글라이드)은 같은 측정에서 3243.6Hz였다.
# 스펙트럼 무게중심(Hz). 파일 **전체**를 다음 2의 거듭제곱으로 제로패딩해 한 번에
# 잰다. 계획서의 원안은 앞 4096 샘플을 8칸씩 건너뛰며 더했는데, 그 데시메이션은
# 5.5kHz 위를 접어 넣으면서 주파수 라벨은 원래 표본율로 붙여 값이 틀린다.
import wave, array, math, cmath, os, sys

def fft(a):
    n = len(a)
    if n == 1: return a
    e = fft(a[0::2]); o = fft(a[1::2])
    out = [0j]*n
    for k in range(n//2):
        t = cmath.exp(-2j*math.pi*k/n)*o[k]
        out[k] = e[k]+t; out[k+n//2] = e[k]-t
    return out

def centroid(path):
    with wave.open(path) as w:
        n = w.getnframes(); rate = w.getframerate()
        d = array.array("h"); d.frombytes(w.readframes(n))
    xs = [v/32768.0 for v in d]
    N = 1
    while N < len(xs): N *= 2
    xs += [0.0]*(N-len(xs))
    sp = fft([complex(v,0) for v in xs])
    num = den = 0.0
    for k in range(1, N//2):
        m = abs(sp[k]); num += m*(k*rate/N); den += m
    return num/den if den else 0.0

for p in sys.argv[1:]:
    print("%-14s centroid=%7.1f Hz" % (os.path.basename(p), centroid(p)))
