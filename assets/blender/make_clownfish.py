# assets/blender/make_clownfish.py
# Clownfish (Amphiprion ocellaris) species spec: small rounded body + fin plates,
# 10-bone armature, baked procedural base color (orange with three white bars,
# dark-edged). fishlib.build_species runs the pipeline and writes
# Clownfish.blend / export/Clownfish.fbx for Unreal.
#
# Run: /Applications/Blender.app/Contents/MacOS/Blender -b -P assets/blender/make_clownfish.py
#
# Conventions (must match the Unreal import side):
#   +X = head, -X = tail, +Z = up, 1 unit = 1 cm, body length ~11 cm.
#   Bones: Root, Spine0..Spine5, Tail, PecL, PecR (exactly these names).
import os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fishlib as F

ROOT = os.path.dirname(os.path.abspath(__file__))
EXPORT = os.path.join(ROOT, "export")

BODY_LEN = 11.0   # cm
BODY_H = 5.0
BODY_W = 2.2
TAPER_Z = 0.5     # rounder hull than the tang; shared by build_body and ctx["body_h"]
TAPER_Y = 0.45
FIN_T = 0.15      # fin plate thickness (cm)
SPINE = 6


# ---------- fins (flat plates joined to body; fin roots sit at 70% of the local
# ---------- body half-height so the plates overlap the hull) ----------
def tail_verts(c):
    """Rounded tail fin: extra rim vertices approximate the ocellaris' rounded caudal fin."""
    L = c["L"]
    return [(-L + 0.5, 0, 1.2), (-L - 2.0, 0, 2.2), (-L - 2.8, 0, 1.4), (-L - 3.0, 0, 0),
            (-L - 2.8, 0, -1.4), (-L - 2.0, 0, -2.2), (-L + 0.5, 0, -1.2)]


def dorsal_verts(c):
    L, body_h = c["L"], c["body_h"]
    return [(L * 0.5, 0, body_h(L * 0.5) * 0.7), (0, 0, body_h(0) * 0.7),
            (-L * 0.55, 0, body_h(-L * 0.55) * 0.7),
            (-L * 0.55, 0, body_h(-L * 0.55) * 0.7 + 1.2), (0, 0, body_h(0) * 0.7 + 1.8),
            (L * 0.45, 0, body_h(L * 0.45) * 0.7 + 1.0)]


def anal_verts(c):
    L, body_h = c["L"], c["body_h"]
    return [(L * 0.05, 0, -body_h(L * 0.05) * 0.7), (-L * 0.55, 0, -body_h(-L * 0.55) * 0.7),
            (-L * 0.55, 0, -body_h(-L * 0.55) * 0.7 - 1.0), (0, 0, -body_h(0) * 0.7 - 1.3)]


def pecL_verts(c):
    L, Y = c["L"], c["PEC_ROOT_Y"]
    return [(L * 0.35, Y, 0.3), (L * 0.1, Y + 2.0, -0.6), (0, Y + 1.6, -1.1), (L * 0.25, Y, -0.6)]


def pecR_verts(c):
    L, Y = c["L"], c["PEC_ROOT_Y"]
    return [(L * 0.35, -Y, 0.3), (L * 0.25, -Y, -0.6), (0, -Y - 1.6, -1.1), (L * 0.1, -Y - 2.0, -0.6)]


# ---------- procedural material ----------
ORANGE = (0.95, 0.35, 0.03, 1)
WHITE = (0.95, 0.95, 0.92, 1)
DARK = (0.03, 0.02, 0.02, 1)
EDGE = 0.35   # dark edge extends this much (cm) beyond each white bar


def clownfish_color(nt, bsdf, c):
    """Orange body with three white vertical bars, each with a thin dark edge.
    Bars are masked by |x - center| in world space (body sits at the origin).

    Kept as a hand-written graph: expressing it through fishlib.axis_band_mask /
    mix_over reproduced the look but shifted three subpixels of the bake by 1/255,
    so the original node wiring stays verbatim to keep T_Clownfish_BaseColor.png
    byte-identical."""
    L = c["L"]
    # (center x, half width) of the three vertical bars: head, middle, tail
    bars = [(L * 0.55, 0.7), (0.0, 0.9), (-L * 0.6, 0.6)]
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(geo.outputs["Position"], sep.inputs[0])

    def bar_mask(center, width):
        """1 inside |x-center| < width*0.85, ramping to 0 at width (inverted MapRange).
        The short ramp keeps the dark rim between the white bar and the orange readable."""
        d = nt.nodes.new("ShaderNodeMath"); d.operation = 'SUBTRACT'; d.inputs[1].default_value = center
        nt.links.new(sep.outputs["X"], d.inputs[0])
        a = nt.nodes.new("ShaderNodeMath"); a.operation = 'ABSOLUTE'
        nt.links.new(d.outputs[0], a.inputs[0])
        # From Min > From Max is intentional: factor rises as |x-center| falls below width
        ramp = nt.nodes.new("ShaderNodeMapRange")
        ramp.inputs["From Min"].default_value = width; ramp.inputs["From Max"].default_value = width * 0.85
        nt.links.new(a.outputs[0], ramp.inputs["Value"])
        return ramp.outputs[0]

    def mix_over(base_socket, mask_socket, color):
        m = nt.nodes.new("ShaderNodeMix"); m.data_type = 'RGBA'
        m.inputs["B"].default_value = color
        nt.links.new(base_socket, m.inputs["A"])
        nt.links.new(mask_socket, m.inputs["Factor"])
        return m.outputs["Result"]

    base = nt.nodes.new("ShaderNodeRGB"); base.outputs[0].default_value = ORANGE
    col = base.outputs[0]
    for cx, w in bars:                       # dark edge first, then white on top
        col = mix_over(col, bar_mask(cx, w + EDGE), DARK)
        col = mix_over(col, bar_mask(cx, w), WHITE)
    nt.links.new(col, bsdf.inputs["Base Color"])


L_HALF = BODY_LEN / 2

SPEC = dict(
    name="Clownfish",
    body=(BODY_LEN, BODY_H, BODY_W, TAPER_Z, TAPER_Y),
    fin_thickness=FIN_T,
    fins=[
        ("TailFin", tail_verts, [(0, 1, 2), (0, 2, 3), (0, 3, 4), (0, 4, 5), (0, 5, 6)]),
        ("DorsalFin", dorsal_verts, [(0, 1, 4, 5), (1, 2, 3, 4)]),
        ("AnalFin", anal_verts, [(0, 1, 2, 3)]),
        ("PecFinL", pecL_verts, [(0, 1, 2, 3)]),
        ("PecFinR", pecR_verts, [(0, 1, 2, 3)]),
    ],
    color_fn=clownfish_color,
    rig=dict(pec_z=0.3, tail_tip_x=-L_HALF - 2.6, pec_span_y=2.0, pec_drop_z=1.2),
    spine=SPINE,
    pec_window=(0.0, 0.4),
    cam_loc=(12, -28, 6),
    cam_rot=(1.35, 0, 0.42),
    root_dir=ROOT,
    export_dir=EXPORT,
)

r = F.build_species(SPEC)

print("CLOWNFISH_OK verts=%d bones=%d unweighted=%d rootMaxW=%.3f pecStray=%d"
      % (r["verts"], r["bones"], r["unweighted"], r["root_max_w"], r["pec_stray"]))
