# assets/blender/make_damselfish.py
# Damselfish (Chrysiptera): a small tapered oval, dark blue over the head and back brightening
# to cyan at the tail. fishlib.build_species runs the profile pipeline (lofted body, eyes,
# membrane fins, BaseColor/Normal/Roughness bake) and writes Damselfish.blend /
# export/Damselfish.fbx.
#
# Run: /Applications/Blender.app/Contents/MacOS/Blender -b -P assets/blender/make_damselfish.py
#
# Conventions: +X = head, -X = tail, +Z = up, 1 unit = 1 cm, ~7 cm nose to caudal tip.
#   Bones: Root, Spine0..Spine5, Tail, PecL, PecR (exactly these names).
import math, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fishlib as F

ROOT = os.path.dirname(os.path.abspath(__file__))
EXPORT = os.path.join(ROOT, "export")

LENGTH = 5.4             # nose to caudal peduncle; the caudal fin adds ~1.8 cm
HALF = LENGTH / 2.0
SPINE = 6
SCALE_CELL = 0.11

# Small tapered oval: shallower than the tangs and pinched hard into the peduncle.
PROFILE = F.hump(0.34, 1.45, 0.16, rise=0.55)
BELLY = F.hump(0.44, 1.30, 0.14, rise=0.55)
WIDTH = F.hump(0.34, 0.68, 0.22, rise=0.45)

EYE_T = 0.13
EYE_X = HALF - EYE_T * LENGTH
EYE_Z = PROFILE(EYE_T) * 0.30
EYE_R = 0.26

NAVY = (0.02, 0.05, 0.32, 1)
CYAN = (0.05, 0.62, 0.78, 1)
DARK = (0.01, 0.02, 0.14, 1)


def dorsal(c):
    return F.sail_outline(c, 0.20, 0.88, 0.9, sign=1, root_frac=0.55, peak=0.5)


def anal(c):
    return F.sail_outline(c, 0.52, 0.88, 0.8, sign=-1, root_frac=0.55, peak=0.5)


def caudal(c):
    return F.caudal_outline(c, reach=1.9, spread=1.25, notch=0.55)


def pec(c):
    return F.fan_outline(c, 0.30, 1.6, 1.1)


def pec_place(sign):
    """Roll the flat XZ fan onto the flank and tilt its trailing tip down."""
    y = WIDTH(0.30) * 0.9
    return dict(rotate=(sign * math.pi / 2, -0.3, 0.0), translate=(0.0, sign * y, -0.18))


def build_nodes(nt, bsdf, c):
    """Dark blue head and back running to cyan over the tail, painted eyes and a fine voronoi
    scale pattern driving the bump and the roughness."""
    sep = F.position_axis(nt)
    aft = F.axis_band_mask(nt, sep.outputs["X"], from_min=c["L"] * 0.45,
                           from_max=-c["L"] * 0.85, use_abs=False)
    col = F.mix_over(nt, NAVY, aft, CYAN)
    back = F.axis_band_mask(nt, sep.outputs["Z"], from_min=c["BODY_H"] * 0.10,
                            from_max=c["BODY_H"] * 0.42, use_abs=False)
    col = F.mix_over(nt, col, back, DARK)
    col = F.paint_eye(nt, col, (EYE_X, EYE_Z), EYE_R)
    nt.links.new(col, bsdf.inputs["Base Color"])
    height, rough = F.scale_pattern(nt, SCALE_CELL)
    bump = nt.nodes.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 1.0
    bump.inputs["Distance"].default_value = 0.055
    nt.links.new(height, bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    rmap = nt.nodes.new("ShaderNodeMapRange")
    rmap.inputs["To Min"].default_value = 0.18
    rmap.inputs["To Max"].default_value = 0.55
    nt.links.new(rough, rmap.inputs["Value"])
    nt.links.new(rmap.outputs[0], bsdf.inputs["Roughness"])


SPEC = dict(
    name="Damselfish",
    length=LENGTH, profile=PROFILE, belly=BELLY, width=WIDTH,
    sections=26, ring_segments=18,
    eye=dict(centre=(EYE_X, EYE_Z), radius=EYE_R, sink=0.1, out=0.55),
    fins_membrane=[
        dict(name="DorsalFin", outline=dorsal, thickness_root=0.16, thickness_edge=0.02,
             rays=12, ray_depth=0.09, steps=5),
        dict(name="AnalFin", outline=anal, thickness_root=0.15, thickness_edge=0.02,
             rays=9, ray_depth=0.08, steps=5),
        dict(name="TailFin", outline=caudal, thickness_root=0.18, thickness_edge=0.025,
             rays=10, ray_depth=0.09, steps=5),
        dict(name="PecFinL", outline=pec, thickness_root=0.13, thickness_edge=0.02,
             rays=8, ray_depth=0.07, steps=4, **pec_place(1.0)),
        dict(name="PecFinR", outline=pec, thickness_root=0.13, thickness_edge=0.02,
             rays=8, ray_depth=0.07, steps=4, **pec_place(-1.0)),
    ],
    build_nodes=build_nodes,
    rig=dict(pec_z=0.0, tail_tip_x=-HALF - 1.1, pec_span_y=1.1, pec_drop_z=0.7),
    spine=SPINE,
    cam_loc=(9.0, -20.0, 4.5),
    cam_rot=(1.35, 0, 0.42),
    root_dir=ROOT,
    export_dir=EXPORT,
)

r = F.build_species(SPEC)

print("DAMSELFISH_OK verts=%d bones=%d unweighted=%d rootMaxW=%.3f pecStray=%d maps=%s"
      % (r["verts"], r["bones"], r["unweighted"], r["root_max_w"], r["pec_stray"],
         ",".join(os.path.basename(r["maps"][k]) for k in ("BaseColor", "Normal", "Roughness"))))
