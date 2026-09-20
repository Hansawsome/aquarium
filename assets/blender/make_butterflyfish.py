# assets/blender/make_butterflyfish.py
# Butterflyfish (Chaetodon): an almost circular disc with a tiny terminal snout, tall dorsal
# and anal, yellow with a black eye band and a second rear band. fishlib.build_species runs
# the profile pipeline (lofted body, eyes, membrane fins, BaseColor/Normal/Roughness bake)
# and writes Butterflyfish.blend / export/Butterflyfish.fbx.
#
# Run: /Applications/Blender.app/Contents/MacOS/Blender -b -P assets/blender/make_butterflyfish.py
#
# Conventions: +X = head, -X = tail, +Z = up, 1 unit = 1 cm, ~14 cm nose to caudal tip.
#   Bones: Root, Spine0..Spine5, Tail, PecL, PecR (exactly these names).
import os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fishlib as F

ROOT = os.path.dirname(os.path.abspath(__file__))
EXPORT = os.path.join(ROOT, "export")

LENGTH = 10.0            # nose to caudal peduncle; the caudal fin adds ~4 cm
HALF = LENGTH / 2.0
SPINE = 6
SCALE_CELL = 0.44

# Near-circular disc: the crest sits mid body and the snout is a short blunt point.
PROFILE = F.hump(0.44, 4.3, 0.20, rise=0.6)
BELLY = F.hump(0.50, 4.1, 0.18, rise=0.6)
WIDTH = F.hump(0.44, 1.0, 0.28, rise=0.5)

EYE_T = 0.15
EYE_X = HALF - EYE_T * LENGTH
EYE_Z = PROFILE(EYE_T) * 0.35
EYE_R = 0.62

YELLOW = (0.97, 0.72, 0.06, 1)
PALE = (0.98, 0.90, 0.55, 1)
BLACK = (0.02, 0.02, 0.03, 1)


def dorsal(c):
    return F.sail_outline(c, 0.22, 0.90, 3.0, sign=1, root_frac=0.55, peak=0.5)


def anal(c):
    return F.sail_outline(c, 0.50, 0.90, 2.6, sign=-1, root_frac=0.55, peak=0.5)


def caudal(c):
    return F.caudal_outline(c, reach=4.0, spread=2.4, notch=1.0, n=48)


def pec(c):
    return F.fan_outline(c, 0.38, 3.0, 1.9)


def pec_place(sign):
    """Park the flat XZ fan against the flank: it already lies in the body's side plane, so it
    only needs a small downward pitch and an outward flare of the trailing tip."""
    y = WIDTH(0.38) * 1.0
    return dict(rotate=(0.0, -0.35, -sign * 0.30), translate=(0.0, sign * y, -0.4))


def build_nodes(nt, bsdf, c):
    """Yellow disc paling toward the belly, a black bar through the eye and a second bar over
    the caudal peduncle, painted eyes and a voronoi scale pattern."""
    sep = F.position_axis(nt)
    low = F.axis_band_mask(nt, sep.outputs["Z"], from_min=-c["BODY_H"] * 0.22,
                           from_max=-c["BODY_H"] * 0.40, use_abs=False)
    col = F.mix_over(nt, YELLOW, low, PALE)
    eye_bar = F.axis_band_mask(nt, sep.outputs["X"], center=EYE_X,
                               half_width=c["L"] * 0.16, ramp=c["L"] * 0.08)
    col = F.mix_over(nt, col, eye_bar, BLACK)
    rear_bar = F.axis_band_mask(nt, sep.outputs["X"], center=-c["L"] * 0.72,
                                half_width=c["L"] * 0.10, ramp=c["L"] * 0.07)
    col = F.mix_over(nt, col, rear_bar, BLACK)
    col = F.paint_eye(nt, col, (EYE_X, EYE_Z), EYE_R)
    nt.links.new(col, bsdf.inputs["Base Color"])
    height, rough = F.scale_pattern_world(nt, SCALE_CELL)
    bump = nt.nodes.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.8
    bump.inputs["Distance"].default_value = 0.11
    nt.links.new(height, bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    rmap = nt.nodes.new("ShaderNodeMapRange")
    rmap.inputs["To Min"].default_value = 0.22
    rmap.inputs["To Max"].default_value = 0.60
    nt.links.new(rough, rmap.inputs["Value"])
    nt.links.new(rmap.outputs[0], bsdf.inputs["Roughness"])


SPEC = dict(
    name="Butterflyfish",
    length=LENGTH, profile=PROFILE, belly=BELLY, width=WIDTH,
    sections=36, ring_segments=24,
    eye=dict(centre=(EYE_X, EYE_Z), radius=EYE_R, sink=0.22, out=0.55),
    fins_membrane=[
        dict(name="DorsalFin", outline=dorsal, thickness_root=0.35, thickness_edge=0.04,
             rays=13, ray_depth=0.22, steps=5),
        dict(name="AnalFin", outline=anal, thickness_root=0.32, thickness_edge=0.04,
             rays=10, ray_depth=0.2, steps=5),
        dict(name="TailFin", outline=caudal, thickness_root=0.38, thickness_edge=0.05,
             rays=11, ray_depth=0.2, steps=5),
        dict(name="PecFinL", outline=pec, thickness_root=0.24, thickness_edge=0.035,
             rays=8, ray_depth=0.16, steps=4, **pec_place(1.0)),
        dict(name="PecFinR", outline=pec, thickness_root=0.24, thickness_edge=0.035,
             rays=8, ray_depth=0.16, steps=4, **pec_place(-1.0)),
    ],
    build_nodes=build_nodes,
    rig=dict(pec_z=0.0, tail_tip_x=-HALF - 2.4, pec_span_y=2.0, pec_drop_z=1.3),
    spine=SPINE,
    cam_loc=(19.0, -42.0, 9.0),
    cam_rot=(1.35, 0, 0.42),
    pec_window=(-0.30, 0.60),
    preview_fill=1.6,
    root_dir=ROOT,
    export_dir=EXPORT,
)

r = F.build_species(SPEC)

print("BUTTERFLYFISH_OK verts=%d bones=%d unweighted=%d rootMaxW=%.3f pecStray=%d maps=%s"
      % (r["verts"], r["bones"], r["unweighted"], r["root_max_w"], r["pec_stray"],
         ",".join(os.path.basename(r["maps"][k]) for k in ("BaseColor", "Normal", "Roughness"))))
