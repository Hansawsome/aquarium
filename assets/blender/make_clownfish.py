# assets/blender/make_clownfish.py
# Procedurally builds the clownfish (Amphiprion ocellaris) fish asset:
# small rounded body + fin plates, 10-bone armature, baked procedural base color
# (orange with three white bars, dark-edged), then saves Clownfish.blend and
# exports Clownfish.fbx for Unreal.
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
os.makedirs(EXPORT, exist_ok=True)

F.reset_scene()

BODY_LEN = 11.0   # cm
BODY_H = 5.0
BODY_W = 2.2
L = BODY_LEN / 2
PEC_ROOT_Y = BODY_W * 0.45   # lateral offset where pectoral fin plates and PecL/PecR bones attach
SPINE = 6
TAPER_Z = 0.5    # rounder hull than the tang; shared by build_body and body_h below
TAPER_Y = 0.45
FIN_T = 0.15     # fin plate thickness (cm)

# ---------- body ----------
body = F.build_body("Clownfish", BODY_LEN, BODY_H, BODY_W, taper_z=TAPER_Z, taper_y=TAPER_Y)

# ---------- fins (flat plates joined to body) ----------
def body_h(x):
    """Half-height of the (tapered) body at x, used to sink fin roots into the hull."""
    return F.body_half_height(x, L, BODY_H, TAPER_Z)

# rounded tail fin: extra rim vertices approximate the ocellaris' rounded caudal fin
tail = F.add_fin("TailFin",
    [(-L + 0.5, 0, 1.2), (-L - 2.0, 0, 2.2), (-L - 2.8, 0, 1.4), (-L - 3.0, 0, 0),
     (-L - 2.8, 0, -1.4), (-L - 2.0, 0, -2.2), (-L + 0.5, 0, -1.2)],
    [(0, 1, 2), (0, 2, 3), (0, 3, 4), (0, 4, 5), (0, 5, 6)], thickness=FIN_T)
# fin roots sit at 70% of the local body half-height so the plates overlap the hull
dorsal = F.add_fin("DorsalFin",
    [(L * 0.5, 0, body_h(L * 0.5) * 0.7), (0, 0, body_h(0) * 0.7), (-L * 0.55, 0, body_h(-L * 0.55) * 0.7),
     (-L * 0.55, 0, body_h(-L * 0.55) * 0.7 + 1.2), (0, 0, body_h(0) * 0.7 + 1.8),
     (L * 0.45, 0, body_h(L * 0.45) * 0.7 + 1.0)],
    [(0, 1, 4, 5), (1, 2, 3, 4)], thickness=FIN_T)
anal = F.add_fin("AnalFin",
    [(L * 0.05, 0, -body_h(L * 0.05) * 0.7), (-L * 0.55, 0, -body_h(-L * 0.55) * 0.7),
     (-L * 0.55, 0, -body_h(-L * 0.55) * 0.7 - 1.0), (0, 0, -body_h(0) * 0.7 - 1.3)],
    [(0, 1, 2, 3)], thickness=FIN_T)
pecL = F.add_fin("PecFinL",
    [(L * 0.35, PEC_ROOT_Y, 0.3), (L * 0.1, PEC_ROOT_Y + 2.0, -0.6),
     (0, PEC_ROOT_Y + 1.6, -1.1), (L * 0.25, PEC_ROOT_Y, -0.6)],
    [(0, 1, 2, 3)], thickness=FIN_T)
pecR = F.add_fin("PecFinR",
    [(L * 0.35, -PEC_ROOT_Y, 0.3), (L * 0.25, -PEC_ROOT_Y, -0.6),
     (0, -PEC_ROOT_Y - 1.6, -1.1), (L * 0.1, -PEC_ROOT_Y - 2.0, -0.6)],
    [(0, 1, 2, 3)], thickness=FIN_T)
F.join_fins(body, [tail, dorsal, anal, pecL, pecR])

# ---------- UV ----------
F.unwrap(body)

# ---------- procedural material -> bake ----------
ORANGE = (0.95, 0.35, 0.03, 1)
WHITE = (0.95, 0.95, 0.92, 1)
DARK = (0.03, 0.02, 0.02, 1)
EDGE = 0.35   # dark edge extends this much (cm) beyond each white bar
# (center x, half width) of the three vertical bars: head, middle, tail
BARS = [(L * 0.55, 0.7), (0.0, 0.9), (-L * 0.6, 0.6)]

def clownfish_color(nt, bsdf):
    """Orange body with three white vertical bars, each with a thin dark edge.
    Bars are masked by |x - center| in world space (body sits at the origin)."""
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
    for cx, w in BARS:                       # dark edge first, then white on top
        col = mix_over(col, bar_mask(cx, w + EDGE), DARK)
        col = mix_over(col, bar_mask(cx, w), WHITE)
    nt.links.new(col, bsdf.inputs["Base Color"])

F.bake_base_color(body, "M_Clownfish", clownfish_color, "T_Clownfish_BaseColor", EXPORT)

# ---------- armature + skinning ----------
arm = F.build_rig("ClownfishRig", L, PEC_ROOT_Y, pec_z=0.3, spine=SPINE, tail_tip_x=-L - 2.6,
                  pec_span_y=2.0, pec_drop_z=1.2)
F.skin(body, arm)

# ---------- rig contract guards ----------
unweighted, root_max_w = F.assert_rig_contract(body, arm, SPINE)

# diagnostic: body vertices captured by the pectoral bones outside the pectoral region
pec_gi = {body.vertex_groups[n].index for n in ("PecL", "PecR")}
pec_stray = sum(1 for v in body.data.vertices
                if not (0.0 <= v.co.x <= L * 0.4)
                and any(g.group in pec_gi and g.weight > 0.5 for g in v.groups))

# ---------- save + export ----------
F.save_and_export(body, arm, os.path.join(ROOT, "Clownfish.blend"), os.path.join(EXPORT, "Clownfish.fbx"))

# ---------- preview render (after export so camera/light never enter the FBX) ----------
F.render_preview(os.path.join(EXPORT, "preview_clownfish.png"), (12, -28, 6), (1.35, 0, 0.42))

print("CLOWNFISH_OK verts=%d bones=%d unweighted=%d rootMaxW=%.3f pecStray=%d"
      % (len(body.data.vertices), len(arm.data.bones), unweighted, root_max_w, pec_stray))
