# assets/blender/make_yellowtang.py
# Yellow tang (Zebrasoma flavescens) species spec: flat oval body + sail dorsal/anal,
# 10-bone armature, baked procedural base color. fishlib.build_species runs the
# pipeline and writes YellowTang.blend / export/YellowTang.fbx for Unreal.
#
# Run: /Applications/Blender.app/Contents/MacOS/Blender -b -P assets/blender/make_yellowtang.py
#
# Conventions (must match the Unreal import side):
#   +X = head, -X = tail, +Z = up, 1 unit = 1 cm, body length ~20 cm.
#   Bones: Root, Spine0..Spine5, Tail, PecL, PecR (exactly these names).
import os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fishlib as F

ROOT = os.path.dirname(os.path.abspath(__file__))
EXPORT = os.path.join(ROOT, "export")

BODY_LEN = 20.0   # cm
BODY_H = 10.0
BODY_W = 2.6
TAPER_Z = 0.55    # tail taper of the hull height; shared by build_body and ctx["body_h"]
TAPER_Y = 0.40
SPINE = 6


# ---------- fins (flat plates joined to body; fin roots sit at 70% of the local
# ---------- body half-height so the plates overlap the hull) ----------
def tail_verts(c):
    """Rounded caudal fan reaching just past the Tail bone tip (-L - 0.5*L)."""
    L, H = c["L"], c["BODY_H"]
    return [(-L + L * 0.08, 0, H * 0.22), (-L - L * 0.50, 0, H * 0.44),
            (-L - L * 0.42, 0, 0),
            (-L - L * 0.50, 0, -H * 0.44), (-L + L * 0.08, 0, -H * 0.22)]


def dorsal_verts(c):
    """Tang-like sail dorsal: long root, high arched outer edge."""
    L, H, body_h = c["L"], c["BODY_H"], c["body_h"]
    return [(L * 0.55, 0, body_h(L * 0.55) * 0.7), (0, 0, body_h(0) * 0.7),
            (-L * 0.60, 0, body_h(-L * 0.60) * 0.7),
            (-L * 0.60, 0, H * 0.62), (0, 0, H * 0.88), (L * 0.50, 0, H * 0.60)]


def anal_verts(c):
    """Mirrored sail anal fin, slightly shorter than the dorsal."""
    L, H, body_h = c["L"], c["BODY_H"], c["body_h"]
    return [(L * 0.10, 0, -body_h(L * 0.10) * 0.7), (-L * 0.60, 0, -body_h(-L * 0.60) * 0.7),
            (-L * 0.60, 0, -H * 0.58), (L * 0.05, 0, -H * 0.76)]


def pecL_verts(c):
    L, H, Y = c["L"], c["BODY_H"], c["PEC_ROOT_Y"]
    return [(L * 0.35, Y, H * 0.05), (L * 0.10, Y + L * 0.36, -H * 0.13),
            (0.0, Y + L * 0.28, -H * 0.21), (L * 0.25, Y, -H * 0.13)]


def pecR_verts(c):
    L, H, Y = c["L"], c["BODY_H"], c["PEC_ROOT_Y"]
    return [(L * 0.35, -Y, H * 0.05), (L * 0.25, -Y, -H * 0.13),
            (0.0, -Y - L * 0.28, -H * 0.21), (L * 0.10, -Y - L * 0.36, -H * 0.13)]


# ---------- procedural material ----------
YELLOW = (0.95, 0.75, 0.05, 1)   # saturated tang yellow
WHITE = (0.95, 0.95, 0.90, 1)    # caudal-peduncle band


def yellowtang_color(nt, bsdf, c):
    """Uniform yellow body with a white vertical band on the caudal peduncle."""
    L = c["L"]
    sep = F.position_axis(nt)
    band = F.axis_band_mask(nt, sep.outputs["X"], center=-L * 0.75,
                            half_width=L * 0.06, ramp=L * 0.04)
    col = F.mix_over(nt, YELLOW, band, WHITE)
    nt.links.new(col, bsdf.inputs["Base Color"])


SPEC = dict(
    name="YellowTang",
    body=(BODY_LEN, BODY_H, BODY_W, TAPER_Z, TAPER_Y),
    fin_thickness=0.20,
    fins=[
        ("TailFin", tail_verts, [(0, 1, 2), (0, 2, 4), (2, 3, 4)]),
        ("DorsalFin", dorsal_verts, [(0, 1, 4, 5), (1, 2, 3, 4)]),
        ("AnalFin", anal_verts, [(0, 1, 2, 3)]),
        ("PecFinL", pecL_verts, [(0, 1, 2, 3)]),
        ("PecFinR", pecR_verts, [(0, 1, 2, 3)]),
    ],
    color_fn=yellowtang_color,
    rig=dict(pec_z=0.4, tail_tip_x=-BODY_LEN / 2 - 5.0, pec_span_y=3.2, pec_drop_z=2.0),
    spine=SPINE,
    cam_loc=(27.2, -60.8, 12.8),   # pulled back 1.6x so fins stay in frame
    cam_rot=(1.35, 0, 0.42),
    root_dir=ROOT,
    export_dir=EXPORT,
)

r = F.build_species(SPEC)

print("YELLOWTANG_OK verts=%d bones=%d unweighted=%d rootMaxW=%.3f pecStray=%d"
      % (r["verts"], r["bones"], r["unweighted"], r["root_max_w"], r["pec_stray"]))
