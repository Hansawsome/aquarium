#!/usr/bin/env python3
# 스피커 아이콘 두 장(켜짐/꺼짐)을 128x128 RGBA PNG로 굽는다.
# 외부 이미지도, PIL도 쓰지 않는다 -- zlib과 struct만으로 PNG를 직접 쓴다.
# 글자는 한 획도 넣지 않는다(시나리오: 아이는 글자를 읽지 않는다).
#
# 실행: python3 assets/ui/make_icons.py     판정: 마지막 ICONS_OK 줄
import hashlib
import os
import struct
import zlib

SIZE = 128
HERE = os.path.dirname(os.path.abspath(__file__))
WHITE = (250, 250, 252)
RED = (235, 70, 70)


def png(path, pixels):
    """pixels: [ (r,g,b,a) ] * SIZE*SIZE, 행 우선."""
    raw = bytearray()
    for y in range(SIZE):
        raw.append(0)                       # 필터 타입 0 (None) -- 결정적이다
        for x in range(SIZE):
            raw.extend(pixels[y * SIZE + x])

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    ihdr = struct.pack(">IIBBBBB", SIZE, SIZE, 8, 6, 0, 0, 0)   # 8비트 RGBA
    body = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr)
            + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(body)
    return len(body)


def blank():
    return [(0, 0, 0, 0)] * (SIZE * SIZE)


def put(px, x, y, rgb, a=255):
    if 0 <= x < SIZE and 0 <= y < SIZE:
        px[y * SIZE + x] = (rgb[0], rgb[1], rgb[2], a)


def speaker(px):
    """스피커 몸통: 왼쪽 사각형 + 오른쪽으로 벌어지는 사다리꼴."""
    for y in range(SIZE):
        for x in range(SIZE):
            fx, fy = x / SIZE, y / SIZE
            in_box = 0.14 <= fx <= 0.30 and 0.38 <= fy <= 0.62
            # 사다리꼴: x가 커질수록 위아래로 벌어진다
            half = 0.12 + (fx - 0.30) * 1.30
            in_cone = 0.30 <= fx <= 0.52 and abs(fy - 0.50) <= half
            if in_box or in_cone:
                put(px, x, y, WHITE)


def waves(px):
    """오른쪽 음파 두 줄. 원호를 두께 4px로 찍는다."""
    cx, cy = 0.50, 0.50
    for y in range(SIZE):
        for x in range(SIZE):
            fx, fy = x / SIZE, y / SIZE
            if fx < 0.53:
                continue
            d = ((fx - cx) ** 2 + (fy - cy) ** 2) ** 0.5
            for r in (0.16, 0.25):
                if abs(d - r) < 0.022 and abs(fy - cy) < r * 0.80:
                    put(px, x, y, WHITE)


def slash(px):
    """오른쪽 위에서 왼쪽 아래로 긋는 붉은 작대기. '꺼짐'을 아이가 읽는 유일한 신호."""
    for y in range(SIZE):
        for x in range(SIZE):
            fx, fy = x / SIZE, y / SIZE
            if abs((fx - 0.18) - (fy - 0.82) * -1.0) < 0.045 and 0.16 <= fx <= 0.86:
                put(px, x, y, RED)


def main():
    os.makedirs(HERE, exist_ok=True)
    rows = []
    for name, build in (("T_SoundOn.png", (speaker, waves)), ("T_SoundOff.png", (speaker, slash))):
        px = blank()
        for fn in build:
            fn(px)
        opaque = sum(1 for p in px if p[3] > 0)
        assert opaque > 500, "%s is nearly empty (%d px)" % (name, opaque)
        path = os.path.join(HERE, name)
        size = png(path, px)
        digest = hashlib.sha256(open(path, "rb").read()).hexdigest()[:16]
        print("  %-16s bytes=%d opaque=%d sha256=%s" % (name, size, opaque, digest))
        rows.append("%s:%d" % (name, opaque))
    a = open(os.path.join(HERE, "T_SoundOn.png"), "rb").read()
    b = open(os.path.join(HERE, "T_SoundOff.png"), "rb").read()
    assert a != b, "on/off icons are identical"
    print("ICONS_OK count=2 size=%d [%s]" % (SIZE, " ".join(rows)))


if __name__ == "__main__":
    main()
