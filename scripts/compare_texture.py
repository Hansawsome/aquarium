#!/usr/bin/env python3
"""Tolerance-based comparison of two baked textures.

The Cycles bake is not bit-deterministic: repeated runs of an unchanged script
flip a few dozen sub-pixels near UV seams by 1/255 (measured 2026-09-20). Use
this instead of `cmp` when checking that a refactor left a texture unchanged.

Usage: scripts/compare_texture.py A.png B.png [max_delta] [max_changed_fraction]
Prints TEXTURE_MATCH or TEXTURE_DIFFERS with the measured numbers.
"""
import sys
from PIL import Image

a_path, b_path = sys.argv[1], sys.argv[2]
max_delta = int(sys.argv[3]) if len(sys.argv) > 3 else 2
max_frac = float(sys.argv[4]) if len(sys.argv) > 4 else 0.0001

a = Image.open(a_path).convert("RGBA")
b = Image.open(b_path).convert("RGBA")
if a.size != b.size:
    print(f"TEXTURE_DIFFERS size {a.size} != {b.size}")
    sys.exit(1)
ba, bb = a.tobytes(), b.tobytes()
deltas = [abs(x - y) for x, y in zip(ba, bb) if x != y]
worst = max(deltas) if deltas else 0
frac = len(deltas) / len(ba)
ok = worst <= max_delta and frac <= max_frac
print(f"{'TEXTURE_MATCH' if ok else 'TEXTURE_DIFFERS'} changed={len(deltas)} ({frac:.6%}) maxDelta={worst}")
sys.exit(0 if ok else 1)
