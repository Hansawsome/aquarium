# assets/blender/make_bluetang.py
# Procedurally builds the blue tang (Paracanthurus hepatus) fish asset:
# flat oval body + fin plates, 10-bone armature, baked procedural base color,
# then saves BlueTang.blend and exports BlueTang.fbx for Unreal.
#
# Run: /Applications/Blender.app/Contents/MacOS/Blender -b -P assets/blender/make_bluetang.py
#
# Conventions (must match the Unreal import side):
#   +X = head, -X = tail, +Z = up, 1 unit = 1 cm, body length ~25 cm.
#   Bones: Root, Spine0..Spine5, Tail, PecL, PecR (exactly these names).
import math, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fishlib as F

ROOT = os.path.dirname(os.path.abspath(__file__))
EXPORT = os.path.join(ROOT, "export")
os.makedirs(EXPORT, exist_ok=True)

F.reset_scene()

BODY_LEN = 25.0   # cm
BODY_H = 12.0
BODY_W = 3.0
L = BODY_LEN / 2
PEC_ROOT_Y = BODY_W * 0.45   # lateral offset where pectoral fin plates and PecL/PecR bones attach
SPINE = 6

# ---------- body ----------
body = F.build_body("BlueTang", BODY_LEN, BODY_H, BODY_W, taper_z=0.55, taper_y=0.4)

# ---------- fins (flat plates joined to body) ----------
def body_h(x):
    """Half-height of the (tapered) body at x, used to sink fin roots into the hull."""
    t = max(0.0, -x / L)
    return (BODY_H / 2) * math.sqrt(max(0.0, 1.0 - (x / L) ** 2)) * (1.0 - 0.55 * t * t)

tail = F.add_fin("TailFin",
    [(-L + 1, 0, 2.5), (-L - 7, 0, 5.5), (-L - 6, 0, 0), (-L - 7, 0, -5.5), (-L + 1, 0, -2.5)],
    [(0, 1, 2), (0, 2, 4), (2, 3, 4)])
# fin roots sit at 70% of the local body half-height so the plates overlap the hull
dorsal = F.add_fin("DorsalFin",
    [(L * 0.55, 0, body_h(L * 0.55) * 0.7), (0, 0, body_h(0) * 0.7), (-L * 0.6, 0, body_h(-L * 0.6) * 0.7),
     (-L * 0.6, 0, BODY_H * 0.62), (0, 0, BODY_H * 0.85), (L * 0.5, 0, BODY_H * 0.6)],
    [(0, 1, 4, 5), (1, 2, 3, 4)])
anal = F.add_fin("AnalFin",
    [(L * 0.1, 0, -body_h(L * 0.1) * 0.7), (-L * 0.6, 0, -body_h(-L * 0.6) * 0.7),
     (-L * 0.6, 0, -BODY_H * 0.6), (L * 0.05, 0, -BODY_H * 0.75)],
    [(0, 1, 2, 3)])
pecL = F.add_fin("PecFinL",
    [(L * 0.35, PEC_ROOT_Y, 0.5), (L * 0.1, PEC_ROOT_Y + 4.5, -1.5),
     (L * 0.0, PEC_ROOT_Y + 3.5, -2.5), (L * 0.25, PEC_ROOT_Y, -1.5)],
    [(0, 1, 2, 3)])
pecR = F.add_fin("PecFinR",
    [(L * 0.35, -PEC_ROOT_Y, 0.5), (L * 0.25, -PEC_ROOT_Y, -1.5),
     (L * 0.0, -PEC_ROOT_Y - 3.5, -2.5), (L * 0.1, -PEC_ROOT_Y - 4.5, -1.5)],
    [(0, 1, 2, 3)])
F.join_fins(body, [tail, dorsal, anal, pecL, pecR])

# ---------- UV ----------
F.unwrap(body)

# ---------- procedural material -> bake ----------
def bluetang_color(nt, bsdf):
    """Royal blue body, dark flank band shaped by |z|, yellow tail. Links into Base Color."""
    # Geometry > Position is world-space; the masks below rely on the body sitting at the origin.
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(geo.outputs["Position"], sep.inputs[0])
    # tail mask: ramps from 0 at x = -L*0.72 to 1 at x = -L*0.80 (yellow toward the tail).
    # From Min > From Max is intentional: an inverted ramp so the factor rises as x decreases.
    tail_ramp = nt.nodes.new("ShaderNodeMapRange")
    tail_ramp.inputs["From Min"].default_value = -L * 0.72; tail_ramp.inputs["From Max"].default_value = -L * 0.80
    nt.links.new(sep.outputs["X"], tail_ramp.inputs["Value"])
    # palette pattern: dark band along the flank, shaped by |z|
    band = nt.nodes.new("ShaderNodeMath"); band.operation = 'ABSOLUTE'
    nt.links.new(sep.outputs["Z"], band.inputs[0])
    band_ramp = nt.nodes.new("ShaderNodeMapRange")
    band_ramp.inputs["From Min"].default_value = BODY_H * 0.05; band_ramp.inputs["From Max"].default_value = BODY_H * 0.28
    nt.links.new(band.outputs[0], band_ramp.inputs["Value"])
    invert = nt.nodes.new("ShaderNodeMath"); invert.operation = 'SUBTRACT'; invert.inputs[0].default_value = 1.0
    nt.links.new(band_ramp.outputs[0], invert.inputs[1])
    mixblack = nt.nodes.new("ShaderNodeMix"); mixblack.data_type = 'RGBA'
    mixblack.inputs["A"].default_value = (0.02, 0.18, 0.75, 1)   # royal blue
    mixblack.inputs["B"].default_value = (0.01, 0.01, 0.02, 1)   # near black
    nt.links.new(invert.outputs[0], mixblack.inputs["Factor"])
    mixtail = nt.nodes.new("ShaderNodeMix"); mixtail.data_type = 'RGBA'
    mixtail.inputs["B"].default_value = (0.95, 0.75, 0.05, 1)    # yellow
    nt.links.new(mixblack.outputs["Result"], mixtail.inputs["A"])
    nt.links.new(tail_ramp.outputs[0], mixtail.inputs["Factor"])
    nt.links.new(mixtail.outputs["Result"], bsdf.inputs["Base Color"])

F.bake_base_color(body, "M_BlueTang", bluetang_color, "T_BlueTang_BaseColor", EXPORT)

# ---------- armature + skinning ----------
arm = F.build_rig("BlueTangRig", L, PEC_ROOT_Y, pec_z=0.5, spine=SPINE)
F.skin(body, arm)

# ---------- rig contract guards ----------
unweighted, root_max_w = F.assert_rig_contract(body, arm, spine=SPINE)

# ---------- save + export ----------
F.save_and_export(body, arm, os.path.join(ROOT, "BlueTang.blend"), os.path.join(EXPORT, "BlueTang.fbx"))

# ---------- preview render (after export so camera/light never enter the FBX) ----------
F.render_preview(os.path.join(EXPORT, "preview.png"), (20, -45, 10), (1.35, 0, 0.42))

print("BLUETANG_OK verts=%d bones=%d unweighted=%d rootMaxW=%.3f"
      % (len(body.data.vertices), len(arm.data.bones), unweighted, root_max_w))
