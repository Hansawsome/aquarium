# assets/blender/make_bluetang.py
# Blue tang (Paracanthurus hepatus) species spec: flat oval body + fin plates,
# 10-bone armature, baked procedural base color. fishlib.build_species runs the
# pipeline and writes BlueTang.blend / export/BlueTang.fbx for Unreal.
#
# Run: /Applications/Blender.app/Contents/MacOS/Blender -b -P assets/blender/make_bluetang.py
#
# Conventions (must match the Unreal import side):
#   +X = head, -X = tail, +Z = up, 1 unit = 1 cm, body length ~25 cm.
#   Bones: Root, Spine0..Spine5, Tail, PecL, PecR (exactly these names).
import os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fishlib as F

ROOT = os.path.dirname(os.path.abspath(__file__))
EXPORT = os.path.join(ROOT, "export")

BODY_LEN = 25.0   # cm
BODY_H = 12.0
BODY_W = 3.0
TAPER_Z = 0.55    # tail taper of the hull height; shared by build_body and ctx["body_h"]
TAPER_Y = 0.4
SPINE = 6


# ---------- fins (flat plates joined to body; fin roots sit at 70% of the local
# ---------- body half-height so the plates overlap the hull) ----------
def tail_verts(c):
    L = c["L"]
    return [(-L + 1, 0, 2.5), (-L - 7, 0, 5.5), (-L - 6, 0, 0), (-L - 7, 0, -5.5), (-L + 1, 0, -2.5)]


def dorsal_verts(c):
    L, H, body_h = c["L"], c["BODY_H"], c["body_h"]
    return [(L * 0.55, 0, body_h(L * 0.55) * 0.7), (0, 0, body_h(0) * 0.7),
            (-L * 0.6, 0, body_h(-L * 0.6) * 0.7),
            (-L * 0.6, 0, H * 0.62), (0, 0, H * 0.85), (L * 0.5, 0, H * 0.6)]


def anal_verts(c):
    L, H, body_h = c["L"], c["BODY_H"], c["body_h"]
    return [(L * 0.1, 0, -body_h(L * 0.1) * 0.7), (-L * 0.6, 0, -body_h(-L * 0.6) * 0.7),
            (-L * 0.6, 0, -H * 0.6), (L * 0.05, 0, -H * 0.75)]


def pecL_verts(c):
    L, Y = c["L"], c["PEC_ROOT_Y"]
    return [(L * 0.35, Y, 0.5), (L * 0.1, Y + 4.5, -1.5), (L * 0.0, Y + 3.5, -2.5), (L * 0.25, Y, -1.5)]


def pecR_verts(c):
    L, Y = c["L"], c["PEC_ROOT_Y"]
    return [(L * 0.35, -Y, 0.5), (L * 0.25, -Y, -1.5), (L * 0.0, -Y - 3.5, -2.5), (L * 0.1, -Y - 4.5, -1.5)]


# ---------- procedural material ----------
BLUE = (0.02, 0.18, 0.75, 1)    # royal blue
BLACK = (0.01, 0.01, 0.02, 1)   # near black
YELLOW = (0.95, 0.75, 0.05, 1)


def bluetang_color(nt, bsdf, c):
    """Royal blue body, dark flank band shaped by |z|, yellow tail. Links into Base Color."""
    L, H = c["L"], c["BODY_H"]
    sep = F.position_axis(nt)
    # dark flank band: 1 where |z| <= H*0.05, fading out by H*0.28 (ascending ramp, inverted)
    band = F.axis_band_mask(nt, sep.outputs["Z"], from_min=H * 0.05, from_max=H * 0.28, invert=True)
    col = F.mix_over(nt, BLUE, band, BLACK)
    # tail mask: signed ramp, 0 at x = -L*0.72 rising to 1 at x = -L*0.80 (From Min > From Max)
    tail = F.axis_band_mask(nt, sep.outputs["X"], from_min=-L * 0.72, from_max=-L * 0.80, use_abs=False)
    col = F.mix_over(nt, col, tail, YELLOW)
    nt.links.new(col, bsdf.inputs["Base Color"])


SPEC = dict(
    name="BlueTang",
    body=(BODY_LEN, BODY_H, BODY_W, TAPER_Z, TAPER_Y),
    fin_thickness=0.25,
    fins=[
        ("TailFin", tail_verts, [(0, 1, 2), (0, 2, 4), (2, 3, 4)]),
        ("DorsalFin", dorsal_verts, [(0, 1, 4, 5), (1, 2, 3, 4)]),
        ("AnalFin", anal_verts, [(0, 1, 2, 3)]),
        ("PecFinL", pecL_verts, [(0, 1, 2, 3)]),
        ("PecFinR", pecR_verts, [(0, 1, 2, 3)]),
    ],
    color_fn=bluetang_color,
    rig=dict(pec_z=0.5, tail_tip_x=None, pec_span_y=4.0, pec_drop_z=2.5),
    spine=SPINE,
    cam_loc=(20, -45, 10),
    cam_rot=(1.35, 0, 0.42),
    root_dir=ROOT,
    export_dir=EXPORT,
)

r = F.build_species(SPEC)

print("BLUETANG_OK verts=%d bones=%d unweighted=%d rootMaxW=%.3f pecStray=%d"
      % (r["verts"], r["bones"], r["unweighted"], r["root_max_w"], r["pec_stray"]))
