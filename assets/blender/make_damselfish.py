# assets/blender/make_damselfish.py
# Damselfish (Chrysiptera sp.) species spec: small streamlined body, modest fins,
# 10-bone armature, baked procedural base color. fishlib.build_species runs the
# pipeline and writes Damselfish.blend / export/Damselfish.fbx for Unreal.
#
# Run: /Applications/Blender.app/Contents/MacOS/Blender -b -P assets/blender/make_damselfish.py
#
# Conventions (must match the Unreal import side):
#   +X = head, -X = tail, +Z = up, 1 unit = 1 cm, body length ~7 cm.
#   Bones: Root, Spine0..Spine5, Tail, PecL, PecR (exactly these names).
import os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fishlib as F

ROOT = os.path.dirname(os.path.abspath(__file__))
EXPORT = os.path.join(ROOT, "export")

BODY_LEN = 7.0    # cm
BODY_H = 3.4
BODY_W = 1.6
TAPER_Z = 0.50    # tail taper of the hull height; shared by build_body and ctx["body_h"]
TAPER_Y = 0.45
SPINE = 6


# ---------- fins (flat plates joined to body; fin roots sit at 70% of the local
# ---------- body half-height so the plates overlap the hull) ----------
def tail_verts(c):
    """Small rounded caudal fan reaching just past the Tail bone tip (-L - 0.514*L)."""
    L, H = c["L"], c["BODY_H"]
    return [(-L + L * 0.08, 0, H * 0.24), (-L - L * 0.52, 0, H * 0.48),
            (-L - L * 0.44, 0, 0),
            (-L - L * 0.52, 0, -H * 0.48), (-L + L * 0.08, 0, -H * 0.24)]


def dorsal_verts(c):
    """Low, simple dorsal ridge."""
    L, H, body_h = c["L"], c["BODY_H"], c["body_h"]
    return [(L * 0.50, 0, body_h(L * 0.50) * 0.7), (0, 0, body_h(0) * 0.7),
            (-L * 0.58, 0, body_h(-L * 0.58) * 0.7),
            (-L * 0.58, 0, H * 0.45), (0, 0, H * 0.62), (L * 0.45, 0, H * 0.45)]


def anal_verts(c):
    """Short anal fin near the tail."""
    L, H, body_h = c["L"], c["BODY_H"], c["body_h"]
    return [(L * 0.05, 0, -body_h(L * 0.05) * 0.7), (-L * 0.55, 0, -body_h(-L * 0.55) * 0.7),
            (-L * 0.55, 0, -H * 0.45), (0.0, 0, -H * 0.55)]


def pecL_verts(c):
    L, H, Y = c["L"], c["BODY_H"], c["PEC_ROOT_Y"]
    return [(L * 0.35, Y, H * 0.06), (L * 0.10, Y + L * 0.34, -H * 0.14),
            (0.0, Y + L * 0.26, -H * 0.22), (L * 0.25, Y, -H * 0.14)]


def pecR_verts(c):
    L, H, Y = c["L"], c["BODY_H"], c["PEC_ROOT_Y"]
    return [(L * 0.35, -Y, H * 0.06), (L * 0.25, -Y, -H * 0.14),
            (0.0, -Y - L * 0.26, -H * 0.22), (L * 0.10, -Y - L * 0.34, -H * 0.14)]


# ---------- procedural material ----------
DEEP_BLUE = (0.05, 0.15, 0.60, 1)   # dark blue body
CYAN = (0.25, 0.60, 0.95, 1)        # brighter rear half


def damselfish_color(nt, bsdf, c):
    """Deep blue body brightening to cyan toward the tail (soft signed ramp along x)."""
    L = c["L"]
    sep = F.position_axis(nt)
    # signed ramp: 0 at x = -L*0.30, rising to 1 at x = -L*0.60 (From Min > From Max)
    tail = F.axis_band_mask(nt, sep.outputs["X"], from_min=-L * 0.30, from_max=-L * 0.60,
                            use_abs=False)
    col = F.mix_over(nt, DEEP_BLUE, tail, CYAN)
    nt.links.new(col, bsdf.inputs["Base Color"])


SPEC = dict(
    name="Damselfish",
    body=(BODY_LEN, BODY_H, BODY_W, TAPER_Z, TAPER_Y),
    fin_thickness=0.12,
    fins=[
        ("TailFin", tail_verts, [(0, 1, 2), (0, 2, 4), (2, 3, 4)]),
        ("DorsalFin", dorsal_verts, [(0, 1, 4, 5), (1, 2, 3, 4)]),
        ("AnalFin", anal_verts, [(0, 1, 2, 3)]),
        ("PecFinL", pecL_verts, [(0, 1, 2, 3)]),
        ("PecFinR", pecR_verts, [(0, 1, 2, 3)]),
    ],
    color_fn=damselfish_color,
    rig=dict(pec_z=0.25, tail_tip_x=-BODY_LEN / 2 - 1.8, pec_span_y=1.4, pec_drop_z=0.8),
    spine=SPINE,
    cam_loc=(11.2, -25.6, 5.6),   # pulled back 1.6x so fins stay in frame
    cam_rot=(1.35, 0, 0.42),
    root_dir=ROOT,
    export_dir=EXPORT,
)

r = F.build_species(SPEC)

print("DAMSELFISH_OK verts=%d bones=%d unweighted=%d rootMaxW=%.3f pecStray=%d"
      % (r["verts"], r["bones"], r["unweighted"], r["root_max_w"], r["pec_stray"]))
