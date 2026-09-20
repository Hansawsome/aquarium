# assets/blender/make_butterflyfish.py
# Butterflyfish (Chaetodon sp.) species spec: deep disc-shaped body, tall dorsal/anal,
# 10-bone armature, baked procedural base color. fishlib.build_species runs the
# pipeline and writes Butterflyfish.blend / export/Butterflyfish.fbx for Unreal.
#
# Run: /Applications/Blender.app/Contents/MacOS/Blender -b -P assets/blender/make_butterflyfish.py
#
# Conventions (must match the Unreal import side):
#   +X = head, -X = tail, +Z = up, 1 unit = 1 cm, body length ~14 cm.
#   Bones: Root, Spine0..Spine5, Tail, PecL, PecR (exactly these names).
import os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fishlib as F

ROOT = os.path.dirname(os.path.abspath(__file__))
EXPORT = os.path.join(ROOT, "export")

BODY_LEN = 14.0   # cm
BODY_H = 10.5     # deep-bodied: height is 3/4 of the length
BODY_W = 2.4
TAPER_Z = 0.50    # tail taper of the hull height; shared by build_body and ctx["body_h"]
TAPER_Y = 0.42
SPINE = 6


# ---------- fins (flat plates joined to body; fin roots sit at 70% of the local
# ---------- body half-height so the plates overlap the hull) ----------
def tail_verts(c):
    """Rounded caudal fan reaching just past the Tail bone tip (-L - 0.457*L)."""
    L, H = c["L"], c["BODY_H"]
    return [(-L + L * 0.08, 0, H * 0.20), (-L - L * 0.46, 0, H * 0.36),
            (-L - L * 0.40, 0, 0),
            (-L - L * 0.46, 0, -H * 0.36), (-L + L * 0.08, 0, -H * 0.20)]


def dorsal_verts(c):
    """Tall dorsal sail matching the deep disc body profile."""
    L, H, body_h = c["L"], c["BODY_H"], c["body_h"]
    return [(L * 0.55, 0, body_h(L * 0.55) * 0.7), (0, 0, body_h(0) * 0.7),
            (-L * 0.62, 0, body_h(-L * 0.62) * 0.7),
            (-L * 0.62, 0, H * 0.68), (0, 0, H * 0.95), (L * 0.50, 0, H * 0.68)]


def anal_verts(c):
    """Tall anal fin, nearly mirroring the dorsal."""
    L, H, body_h = c["L"], c["BODY_H"], c["body_h"]
    return [(L * 0.12, 0, -body_h(L * 0.12) * 0.7), (-L * 0.62, 0, -body_h(-L * 0.62) * 0.7),
            (-L * 0.62, 0, -H * 0.66), (0, 0, -H * 0.88), (L * 0.08, 0, -H * 0.74)]


def pecL_verts(c):
    L, H, Y = c["L"], c["BODY_H"], c["PEC_ROOT_Y"]
    return [(L * 0.35, Y, H * 0.05), (L * 0.10, Y + L * 0.38, -H * 0.12),
            (0.0, Y + L * 0.30, -H * 0.20), (L * 0.25, Y, -H * 0.12)]


def pecR_verts(c):
    L, H, Y = c["L"], c["BODY_H"], c["PEC_ROOT_Y"]
    return [(L * 0.35, -Y, H * 0.05), (L * 0.25, -Y, -H * 0.12),
            (0.0, -Y - L * 0.30, -H * 0.20), (L * 0.10, -Y - L * 0.38, -H * 0.12)]


# ---------- procedural material ----------
YELLOW = (0.95, 0.80, 0.15, 1)   # warm body yellow
BLACK = (0.03, 0.02, 0.02, 1)    # eye band / rear band


def butterflyfish_color(nt, bsdf, c):
    """Yellow body with a black eye band near the snout and a second band before the tail."""
    L = c["L"]
    sep = F.position_axis(nt)
    eye = F.axis_band_mask(nt, sep.outputs["X"], center=L * 0.62,
                           half_width=L * 0.06, ramp=L * 0.03)
    col = F.mix_over(nt, YELLOW, eye, BLACK)
    rear = F.axis_band_mask(nt, sep.outputs["X"], center=-L * 0.55,
                            half_width=L * 0.05, ramp=L * 0.03)
    col = F.mix_over(nt, col, rear, BLACK)
    nt.links.new(col, bsdf.inputs["Base Color"])


SPEC = dict(
    name="Butterflyfish",
    body=(BODY_LEN, BODY_H, BODY_W, TAPER_Z, TAPER_Y),
    fin_thickness=0.15,
    fins=[
        ("TailFin", tail_verts, [(0, 1, 2), (0, 2, 4), (2, 3, 4)]),
        ("DorsalFin", dorsal_verts, [(0, 1, 4, 5), (1, 2, 3, 4)]),
        ("AnalFin", anal_verts, [(0, 1, 3, 4), (1, 2, 3)]),
        ("PecFinL", pecL_verts, [(0, 1, 2, 3)]),
        ("PecFinR", pecR_verts, [(0, 1, 2, 3)]),
    ],
    color_fn=butterflyfish_color,
    rig=dict(pec_z=0.35, tail_tip_x=-BODY_LEN / 2 - 3.2, pec_span_y=2.4, pec_drop_z=1.4),
    spine=SPINE,
    cam_loc=(24.0, -56.0, 12.0),   # pulled back 2.0x: the tall dorsal fin needs more room than the other species
    cam_rot=(1.35, 0, 0.42),
    root_dir=ROOT,
    export_dir=EXPORT,
)

r = F.build_species(SPEC)

print("BUTTERFLYFISH_OK verts=%d bones=%d unweighted=%d rootMaxW=%.3f pecStray=%d"
      % (r["verts"], r["bones"], r["unweighted"], r["root_max_w"], r["pec_stray"]))
