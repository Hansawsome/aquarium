# assets/blender/make_yellowtang.py
# Yellow tang (Zebrasoma flavescens): the same deep compressed disc family as the blue tang
# but rounder and with a long drawn-out snout, all yellow with a white caudal-peduncle band.
# fishlib.build_species runs the profile pipeline (lofted body, eyes, membrane fins,
# BaseColor/Normal/Roughness bake) and writes YellowTang.blend / export/YellowTang.fbx.
#
# Run: /Applications/Blender.app/Contents/MacOS/Blender -b -P assets/blender/make_yellowtang.py
#
# Conventions: +X = head, -X = tail, +Z = up, 1 unit = 1 cm, ~20 cm nose to caudal tip.
#   Bones: Root, Spine0..Spine5, Tail, PecL, PecR (exactly these names).
import os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fishlib as F

ROOT = os.path.dirname(os.path.abspath(__file__))
EXPORT = os.path.join(ROOT, "export")

LENGTH = 14.0            # nose to caudal peduncle; the caudal fin adds ~6 cm
HALF = LENGTH / 2.0
SPINE = 6
SCALE_CELL = 0.58

# Rounder disc than the blue tang, and the snout is drawn out (a slow rise near t=0).
PROFILE = F.hump(0.38, 5.8, 0.15, rise=1.0)
BELLY = F.hump(0.46, 5.2, 0.13, rise=1.0)
WIDTH = F.hump(0.38, 1.3, 0.26, rise=0.7)

EYE_T = 0.16
EYE_X = HALF - EYE_T * LENGTH
EYE_Z = PROFILE(EYE_T) * 0.40
EYE_R = 0.8

YELLOW = (0.96, 0.68, 0.02, 1)
DEEP = (0.72, 0.40, 0.01, 1)     # shaded yellow in the fin/back creases
WHITE = (0.94, 0.94, 0.90, 1)


def dorsal(c):
    return F.sail_outline(c, 0.20, 0.92, 4.2, sign=1, root_frac=0.55, peak=0.55)


def anal(c):
    return F.sail_outline(c, 0.46, 0.92, 3.6, sign=-1, root_frac=0.55, peak=0.5)


def caudal(c):
    return F.caudal_outline(c, reach=6.0, spread=3.8, notch=2.0, n=48)


def pec(c):
    return F.fan_outline(c, 0.32, 4.2, 2.6)


def pec_place(sign):
    """Park the flat XZ fan against the flank: it already lies in the body's side plane, so it
    only needs a small downward pitch and an outward flare of the trailing tip."""
    y = WIDTH(0.32) * 1.0
    return dict(rotate=(0.0, -0.35, -sign * 0.30), translate=(0.0, sign * y, -0.5))


def build_nodes(nt, bsdf, c):
    """All yellow, deeper toward the back, a white band on the caudal peduncle, painted eyes
    and a voronoi scale pattern driving the bump and the roughness."""
    sep = F.position_axis(nt)
    back = F.axis_band_mask(nt, sep.outputs["Z"], from_min=c["BODY_H"] * 0.10,
                            from_max=c["BODY_H"] * 0.45, use_abs=False)
    col = F.mix_over(nt, YELLOW, back, DEEP)
    band = F.axis_band_mask(nt, sep.outputs["X"], center=-c["L"] * 0.86,
                            half_width=c["L"] * 0.06, ramp=c["L"] * 0.05)
    col = F.mix_over(nt, col, band, WHITE)
    col = F.paint_eye(nt, col, (EYE_X, EYE_Z), EYE_R)
    nt.links.new(col, bsdf.inputs["Base Color"])
    height, rough = F.scale_pattern_world(nt, SCALE_CELL)
    bump = nt.nodes.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.8
    bump.inputs["Distance"].default_value = 0.14
    nt.links.new(height, bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    rmap = nt.nodes.new("ShaderNodeMapRange")
    rmap.inputs["To Min"].default_value = 0.24
    rmap.inputs["To Max"].default_value = 0.62
    nt.links.new(rough, rmap.inputs["Value"])
    nt.links.new(rmap.outputs[0], bsdf.inputs["Roughness"])


SPEC = dict(
    name="YellowTang",
    length=LENGTH, profile=PROFILE, belly=BELLY, width=WIDTH,
    sections=36, ring_segments=24,
    eye=dict(centre=(EYE_X, EYE_Z), radius=EYE_R, sink=0.3, out=0.55),
    fins_membrane=[
        dict(name="DorsalFin", outline=dorsal, thickness_root=0.45, thickness_edge=0.05,
             rays=14, ray_depth=0.28, steps=5),
        dict(name="AnalFin", outline=anal, thickness_root=0.4, thickness_edge=0.05,
             rays=11, ray_depth=0.26, steps=5),
        dict(name="TailFin", outline=caudal, thickness_root=0.5, thickness_edge=0.06,
             rays=12, ray_depth=0.26, steps=5),
        dict(name="PecFinL", outline=pec, thickness_root=0.3, thickness_edge=0.04,
             rays=8, ray_depth=0.2, steps=4, **pec_place(1.0)),
        dict(name="PecFinR", outline=pec, thickness_root=0.3, thickness_edge=0.04,
             rays=8, ray_depth=0.2, steps=4, **pec_place(-1.0)),
    ],
    build_nodes=build_nodes,
    rig=dict(pec_z=0.0, tail_tip_x=-HALF - 3.5, pec_span_y=2.6, pec_drop_z=1.8),
    spine=SPINE,
    cam_loc=(24.0, -53.0, 11.5),
    cam_rot=(1.35, 0, 0.42),
    pec_window=(-0.30, 0.60),
    preview_fill=1.6,
    root_dir=ROOT,
    export_dir=EXPORT,
)

r = F.build_species(SPEC)

print("YELLOWTANG_OK verts=%d bones=%d unweighted=%d rootMaxW=%.3f pecStray=%d maps=%s"
      % (r["verts"], r["bones"], r["unweighted"], r["root_max_w"], r["pec_stray"],
         ",".join(os.path.basename(r["maps"][k]) for k in ("BaseColor", "Normal", "Roughness"))))
