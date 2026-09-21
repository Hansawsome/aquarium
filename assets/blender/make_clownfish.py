# assets/blender/make_clownfish.py
# Clownfish (Amphiprion ocellaris): a rounded oval with a blunt head, three white bars with
# dark edges on orange, a rounded caudal and prominent pectorals. fishlib.build_species runs
# the profile pipeline (lofted body, eyes, membrane fins, BaseColor/Normal/Roughness bake)
# and writes Clownfish.blend / export/Clownfish.fbx.
#
# Run: /Applications/Blender.app/Contents/MacOS/Blender -b -P assets/blender/make_clownfish.py
#
# Conventions: +X = head, -X = tail, +Z = up, 1 unit = 1 cm, ~11 cm nose to caudal tip.
#   Bones: Root, Spine0..Spine5, Tail, PecL, PecR (exactly these names).
import os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fishlib as F

ROOT = os.path.dirname(os.path.abspath(__file__))
EXPORT = os.path.join(ROOT, "export")

LENGTH = 8.2             # nose to caudal peduncle; the caudal fin adds ~2.8 cm
HALF = LENGTH / 2.0
SPINE = 6
SCALE_CELL = 0.12

# Rounded oval; the very blunt head comes from the low `rise` (the silhouette fills out fast).
PROFILE = F.hump(0.36, 2.55, 0.26, rise=0.42)
BELLY = F.hump(0.44, 2.35, 0.22, rise=0.42)
WIDTH = F.hump(0.36, 1.25, 0.30, rise=0.38)

EYE_T = 0.13
EYE_X = HALF - EYE_T * LENGTH
EYE_Z = PROFILE(EYE_T) * 0.30
EYE_R = 0.42

ORANGE = (0.95, 0.30, 0.02, 1)
DEEP = (0.66, 0.16, 0.01, 1)
WHITE = (0.95, 0.95, 0.93, 1)
EDGE = (0.03, 0.02, 0.02, 1)

BARS = (0.62, 0.02, -0.62)       # bar centres as a fraction of L (head, mid, peduncle)


def dorsal(c):
    return F.sail_outline(c, 0.18, 0.88, 1.5, sign=1, root_frac=0.55, peak=0.45)


def anal(c):
    return F.sail_outline(c, 0.52, 0.88, 1.3, sign=-1, root_frac=0.55, peak=0.5)


def caudal(c):
    return F.caudal_outline(c, reach=2.8, spread=2.0, notch=0.5, n=48)


def pec(c):
    return F.fan_outline(c, 0.30, 2.6, 1.8)


def pec_place(sign):
    """Park the flat XZ fan against the flank: it already lies in the body's side plane, so it
    only needs a small downward pitch and an outward flare of the trailing tip."""
    y = WIDTH(0.30) * 1.0
    return dict(rotate=(0.0, -0.35, -sign * 0.30), translate=(0.0, sign * y, -0.3))


def build_nodes(nt, bsdf, c):
    """Orange body darkening over the back, three dark-edged white bars, painted eyes and a
    fine voronoi scale pattern."""
    sep = F.position_axis(nt)
    back = F.axis_band_mask(nt, sep.outputs["Z"], from_min=c["BODY_H"] * 0.12,
                            from_max=c["BODY_H"] * 0.45, use_abs=False)
    col = F.mix_over(nt, ORANGE, back, DEEP)
    for frac in BARS:
        centre = c["L"] * frac
        # the dark edge is a wider band laid down first, the white bar sits inside it
        edge = F.axis_band_mask(nt, sep.outputs["X"], center=centre,
                                half_width=c["L"] * 0.13, ramp=c["L"] * 0.03)
        col = F.mix_over(nt, col, edge, EDGE)
        bar = F.axis_band_mask(nt, sep.outputs["X"], center=centre,
                               half_width=c["L"] * 0.08, ramp=c["L"] * 0.03)
        col = F.mix_over(nt, col, bar, WHITE)
    col = F.paint_eye(nt, col, (EYE_X, EYE_Z), EYE_R)
    nt.links.new(col, bsdf.inputs["Base Color"])
    height, rough = F.scale_pattern_world(nt, SCALE_CELL)
    bump = nt.nodes.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.22
    bump.inputs["Distance"].default_value = 0.02
    nt.links.new(height, bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    rmap = nt.nodes.new("ShaderNodeMapRange")
    rmap.inputs["To Min"].default_value = 0.20
    rmap.inputs["To Max"].default_value = 0.58
    nt.links.new(rough, rmap.inputs["Value"])
    nt.links.new(rmap.outputs[0], bsdf.inputs["Roughness"])


SPEC = dict(
    name="Clownfish",
    length=LENGTH, profile=PROFILE, belly=BELLY, width=WIDTH,
    sections=36, ring_segments=24,
    eye=dict(centre=(EYE_X, EYE_Z), radius=EYE_R, sink=0.16, out=0.55),
    fins_membrane=[
        dict(name="DorsalFin", outline=dorsal, thickness_root=0.26, thickness_edge=0.035,
             rays=12, ray_depth=0.14, steps=5),
        dict(name="AnalFin", outline=anal, thickness_root=0.24, thickness_edge=0.03,
             rays=9, ray_depth=0.12, steps=5),
        dict(name="TailFin", outline=caudal, thickness_root=0.3, thickness_edge=0.04,
             rays=10, ray_depth=0.14, steps=5),
        dict(name="PecFinL", outline=pec, thickness_root=0.22, thickness_edge=0.03,
             rays=8, ray_depth=0.12, steps=4, **pec_place(1.0)),
        dict(name="PecFinR", outline=pec, thickness_root=0.22, thickness_edge=0.03,
             rays=8, ray_depth=0.12, steps=4, **pec_place(-1.0)),
    ],
    build_nodes=build_nodes,
    rig=dict(pec_z=0.0, tail_tip_x=-HALF - 1.7, pec_span_y=1.6, pec_drop_z=1.0),
    spine=SPINE,
    cam_loc=(14.0, -31.0, 7.0),
    cam_rot=(1.35, 0, 0.42),
    pec_window=(-0.30, 0.60),
    preview_fill=1.6,
    root_dir=ROOT,
    export_dir=EXPORT,
)

r = F.build_species(SPEC)

print("CLOWNFISH_OK verts=%d bones=%d unweighted=%d rootMaxW=%.3f pecStray=%d maps=%s"
      % (r["verts"], r["bones"], r["unweighted"], r["root_max_w"], r["pec_stray"],
         ",".join(os.path.basename(r["maps"][k]) for k in ("BaseColor", "Normal", "Roughness"))))
