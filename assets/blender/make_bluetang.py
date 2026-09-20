# assets/blender/make_bluetang.py
# Blue tang (Paracanthurus hepatus): a very deep, laterally compressed disc with a small
# pointed snout, a long low dorsal and anal, a crescent caudal, eyeballs and a baked
# BaseColor/Normal/Roughness set. fishlib.build_species runs the profile pipeline and writes
# BlueTang.blend / export/BlueTang.fbx for Unreal.
#
# Run: /Applications/Blender.app/Contents/MacOS/Blender -b -P assets/blender/make_bluetang.py
#
# Conventions (must match the Unreal import side):
#   +X = head, -X = tail, +Z = up, 1 unit = 1 cm, ~25 cm nose to caudal tip.
#   Bones: Root, Spine0..Spine5, Tail, PecL, PecR (exactly these names).
import math, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fishlib as F

ROOT = os.path.dirname(os.path.abspath(__file__))
EXPORT = os.path.join(ROOT, "export")

LENGTH = 17.0            # nose to caudal peduncle; the caudal fin adds ~8 cm
HALF = LENGTH / 2.0
SPINE = 6
SCALE_CELL = 0.42        # scale cell size in cm, ~1/30 of the body height

# Silhouette: back crest well forward, deep belly behind it, very narrow across.
PROFILE = F.hump(0.30, 6.6, 0.16, rise=0.75)
BELLY = F.hump(0.44, 5.4, 0.13, rise=0.75)
WIDTH = F.hump(0.32, 1.45, 0.28, rise=0.55)

EYE_T = 0.12
EYE_X = HALF - EYE_T * LENGTH
EYE_Z = PROFILE(EYE_T) * 0.42
EYE_R = 0.95             # ~0.08 x body height

BLUE = (0.02, 0.16, 0.72, 1)
BLACK = (0.012, 0.012, 0.02, 1)
YELLOW = (0.95, 0.72, 0.04, 1)


def dorsal(c):
    return F.sail_outline(c, 0.16, 0.93, 3.4, sign=1, root_frac=0.55, peak=0.62)


def anal(c):
    return F.sail_outline(c, 0.42, 0.93, 3.0, sign=-1, root_frac=0.55, peak=0.55)


def caudal(c):
    return F.caudal_outline(c, reach=8.0, spread=5.6, notch=3.0)


def pec(c):
    return F.fan_outline(c, 0.27, 5.2, 3.2)


def pec_place(sign):
    """Pectorals are built flat in XZ, so roll them 90 degrees onto the flank and tilt the
    trailing tip down before they are joined."""
    y = WIDTH(0.27) * 0.9
    return dict(rotate=(sign * math.pi / 2, -0.35, 0.0), translate=(0.0, sign * y, -0.6))


def build_nodes(nt, bsdf, c):
    """Royal blue flank with the black palette marking, yellow caudal, painted eyes and a
    voronoi scale pattern driving both the bump (normal bake) and the roughness."""
    sep = F.position_axis(nt)
    # palette marking: a black field hugging the upper flank from mid body back
    band = F.axis_band_mask(nt, sep.outputs["Z"], from_min=c["BODY_H"] * 0.14,
                            from_max=c["BODY_H"] * 0.40, invert=True)
    aft = F.axis_band_mask(nt, sep.outputs["X"], from_min=c["L"] * 0.75,
                           from_max=c["L"] * 0.35, use_abs=False)
    mark = nt.nodes.new("ShaderNodeMath"); mark.operation = 'MULTIPLY'
    nt.links.new(band, mark.inputs[0]); nt.links.new(aft, mark.inputs[1])
    col = F.mix_over(nt, BLUE, mark.outputs[0], BLACK)
    tail = F.axis_band_mask(nt, sep.outputs["X"], from_min=-c["L"] * 0.80,
                            from_max=-c["L"] * 0.95, use_abs=False)
    col = F.mix_over(nt, col, tail, YELLOW)
    col = F.paint_eye(nt, col, (EYE_X, EYE_Z), EYE_R)
    nt.links.new(col, bsdf.inputs["Base Color"])
    height, rough = F.scale_pattern(nt, SCALE_CELL)
    bump = nt.nodes.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 1.0
    bump.inputs["Distance"].default_value = 0.22
    nt.links.new(height, bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    rmap = nt.nodes.new("ShaderNodeMapRange")       # keep roughness in a plausible wet range
    rmap.inputs["To Min"].default_value = 0.22
    rmap.inputs["To Max"].default_value = 0.62
    nt.links.new(rough, rmap.inputs["Value"])
    nt.links.new(rmap.outputs[0], bsdf.inputs["Roughness"])


SPEC = dict(
    name="BlueTang",
    length=LENGTH, profile=PROFILE, belly=BELLY, width=WIDTH,
    sections=26, ring_segments=18,
    eye=dict(centre=(EYE_X, EYE_Z), radius=EYE_R, sink=0.35, out=0.55),
    fins_membrane=[
        dict(name="DorsalFin", outline=dorsal, thickness_root=0.5, thickness_edge=0.06,
             rays=14, ray_depth=0.3, steps=5),
        dict(name="AnalFin", outline=anal, thickness_root=0.45, thickness_edge=0.06,
             rays=11, ray_depth=0.28, steps=5),
        dict(name="TailFin", outline=caudal, thickness_root=0.6, thickness_edge=0.07,
             rays=13, ray_depth=0.3, steps=5),
        dict(name="PecFinL", outline=pec, thickness_root=0.35, thickness_edge=0.05,
             rays=8, ray_depth=0.22, steps=4, **pec_place(1.0)),
        dict(name="PecFinR", outline=pec, thickness_root=0.35, thickness_edge=0.05,
             rays=8, ray_depth=0.22, steps=4, **pec_place(-1.0)),
    ],
    build_nodes=build_nodes,
    rig=dict(pec_z=0.0, tail_tip_x=-HALF - 5.0, pec_span_y=3.0, pec_drop_z=2.0),
    spine=SPINE,
    cam_loc=(26.0, -58.0, 13.0),
    cam_rot=(1.35, 0, 0.42),
    root_dir=ROOT,
    export_dir=EXPORT,
)

r = F.build_species(SPEC)

print("BLUETANG_OK verts=%d bones=%d unweighted=%d rootMaxW=%.3f pecStray=%d maps=%s"
      % (r["verts"], r["bones"], r["unweighted"], r["root_max_w"], r["pec_stray"],
         ",".join(os.path.basename(r["maps"][k]) for k in ("BaseColor", "Normal", "Roughness"))))
