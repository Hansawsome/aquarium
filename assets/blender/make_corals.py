# assets/blender/make_corals.py
# Three procedural static coral props (branch / plate / brain). Corals are scenery,
# so unlike the fish they have no armature and no animation: mesh + baked base color only.
#
# Run: /Applications/Blender.app/Contents/MacOS/Blender -b -P assets/blender/make_corals.py
#
# Conventions (must match the Unreal import side):
#   +Z = up, 1 unit = 1 cm, origin at the base of the coral so it can be dropped on the floor.
#   Every shape is driven by a fixed random seed, so a rebuild reproduces the same geometry.
#
# Outputs per coral <Name>: assets/blender/<Name>.blend,
# assets/blender/export/<Name>.fbx, assets/blender/export/T_<Name>_BaseColor.png (1024²).
# Plus one shared preview of all three: assets/blender/export/preview_corals.png.
import os, sys, math, random

import bpy, bmesh
from mathutils import Vector, Matrix

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fishlib as F

ROOT = os.path.dirname(os.path.abspath(__file__))
EXPORT = os.path.join(ROOT, "export")
TEXTURE_SIZE = 1024

BRANCH_HEIGHT = 70.0    # cm, enforced by a final uniform scale
PLATE_RADIUS = 50.0
PLATE_THICKNESS = 4.0
BRAIN_RADIUS = 30.0


# ---------------------------------------------------------------------------
# geometry
# ---------------------------------------------------------------------------

def build_branch_coral(name, seed=20260920, depth=3, root_len=26.0, root_radius=5.0):
    """Recursive tapered cylinders: one vertical trunk from the origin, then `depth`
    generations of 2-3 children per node. Children are shorter (x0.55..0.75), thinner
    (x0.65) and tilted off the parent direction but biased back toward +Z.
    All segments are joined into one mesh and uniformly scaled to BRANCH_HEIGHT.
    Precondition: object mode, scene reset. Returns the joined object."""
    rng = random.Random(seed)
    segments = []

    def add_segment(start, direction, length, radius):
        """One tapered cylinder from `start` along `direction`."""
        end_radius = radius * 0.65
        bpy.ops.mesh.primitive_cone_add(vertices=8, radius1=radius, radius2=end_radius,
                                        depth=length, location=(0, 0, 0))
        seg = bpy.context.object
        # the cone is built along +Z centered on the origin: aim it, then move to the midpoint
        seg.rotation_euler = direction.to_track_quat('Z', 'Y').to_euler()
        seg.location = start + direction * (length / 2)
        segments.append(seg)
        return start + direction * length, end_radius

    def grow(start, direction, length, radius, level):
        tip, tip_radius = add_segment(start, direction, length, radius)
        if level >= depth:
            return
        for _ in range(rng.randint(2, 3)):
            # random tilt off the parent, then blended back toward +Z so the coral grows upward
            tilt = rng.uniform(0.6, 1.15)
            axis = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-0.2, 0.2)))
            if axis.length < 1e-4:
                axis = Vector((1, 0, 0))
            child = (Matrix.Rotation(tilt, 3, axis.normalized()) @ direction)
            child = (child + Vector((0, 0, 0.22))).normalized()   # mild upward bias
            grow(tip, child, length * rng.uniform(0.55, 0.75), tip_radius, level + 1)

    grow(Vector((0, 0, 0)), Vector((0, 0, 1)), root_len, root_radius, 0)

    bpy.ops.object.select_all(action='DESELECT')
    for s in segments:
        s.select_set(True)
    ob = segments[0]
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.join()
    ob.name = name
    ob.data.name = name
    # normalise the silhouette height; the recursion only gets close to it by construction
    ob.scale = (BRANCH_HEIGHT / ob.dimensions.z,) * 3
    bpy.ops.object.transform_apply(scale=True)
    bpy.ops.object.shade_smooth()
    return ob


def build_plate_coral(name, radius=PLATE_RADIUS, thickness=PLATE_THICKNESS,
                      rings=6, sectors=48, seed=815):
    """Wide shallow disc whose rim undulates: the top surface is a polar grid whose Z is a
    function of angle and radius, and whose rim radius is jittered by deterministic per-sector
    noise. Solidify gives it `thickness`. Precondition: object mode. Returns the object."""
    rng = random.Random(seed)
    jitter = [rng.uniform(-0.09, 0.09) for _ in range(sectors)]   # per-sector rim radius noise

    def wave(angle, t):
        """Vertical undulation, 0 at the centre and strongest at the rim (t = r / radius)."""
        return (4.0 * math.sin(3.0 * angle) + 1.5 * math.sin(7.0 * angle + 1.3)) * t * t

    mesh = bpy.data.meshes.new(name)
    bm = bmesh.new()
    centre = bm.verts.new((0, 0, 0))
    grid = []   # grid[i][j] = vertex of ring i+1, sector j
    for i in range(1, rings + 1):
        t = i / rings
        ring = []
        for j in range(sectors):
            angle = 2 * math.pi * j / sectors
            r = radius * t * (1.0 + jitter[j] * t * t)
            ring.append(bm.verts.new((r * math.cos(angle), r * math.sin(angle), wave(angle, t))))
        grid.append(ring)
    for j in range(sectors):
        bm.faces.new((centre, grid[0][j], grid[0][(j + 1) % sectors]))
    for i in range(rings - 1):
        for j in range(sectors):
            k = (j + 1) % sectors
            bm.faces.new((grid[i][j], grid[i + 1][j], grid[i + 1][k], grid[i][k]))
    bm.normal_update()
    bm.to_mesh(mesh); bm.free()

    ob = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(ob)
    bpy.ops.object.select_all(action='DESELECT'); ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    sol = ob.modifiers.new("Solidify", 'SOLIDIFY'); sol.thickness = thickness; sol.offset = 0
    bpy.ops.object.modifier_apply(modifier="Solidify")
    bpy.ops.object.shade_smooth()
    return ob


def build_brain_coral(name, radius=BRAIN_RADIUS, segments=64, rings=32, groove=3.6):
    """Hemisphere (the z<0 half of a UV sphere is removed and the opening filled) whose
    vertices are pushed along their normals by a meandering function of x and y, so the
    surface reads as convoluted ridges. Precondition: object mode. Returns the object."""
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments, ring_count=rings, radius=radius)
    ob = bpy.context.object
    ob.name = name; ob.data.name = name

    bm = bmesh.new(); bm.from_mesh(ob.data)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.z < -1e-4], context='VERTS')
    bm.verts.ensure_lookup_table()
    boundary = [e for e in bm.edges if e.is_boundary]
    if boundary:
        bmesh.ops.holes_fill(bm, edges=boundary)
    bm.normal_update()
    for v in bm.verts:
        if v.co.z < 1e-4:
            continue                       # keep the flat underside flat
        u, w = v.co.x / radius, v.co.y / radius
        # two phase-shifted waves whose phase wanders with the other axis -> meandering grooves
        d = math.sin(9.0 * u + 2.2 * math.sin(5.3 * w)) * math.cos(8.0 * w + 1.9 * math.sin(4.7 * u))
        v.co += v.normal * (groove * d)
    bm.normal_update()
    bm.to_mesh(ob.data); bm.free()

    ob.location = (0, 0, 0)
    bpy.ops.object.select_all(action='DESELECT'); ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.shade_smooth()
    return ob


# ---------------------------------------------------------------------------
# procedural color: one flat base tint broken up by a noise texture
# ---------------------------------------------------------------------------

def coral_color_fn(base, shade, scale=7.0):
    """Return a build_color_nodes(nt, bsdf) callback that mixes `shade` over `base` by a
    contrast-stretched noise texture, so the baked map is mottled rather than flat."""
    def build(nt, bsdf):
        tex = nt.nodes.new("ShaderNodeTexNoise")
        tex.inputs["Scale"].default_value = scale
        tex.inputs["Detail"].default_value = 6.0
        tex.inputs["Roughness"].default_value = 0.55
        stretch = nt.nodes.new("ShaderNodeMapRange")       # widen the 0.35..0.65 noise core
        stretch.inputs["From Min"].default_value = 0.35
        stretch.inputs["From Max"].default_value = 0.65
        nt.links.new(tex.outputs["Fac"], stretch.inputs["Value"])
        col = F.mix_over(nt, base, stretch.outputs[0], shade)
        nt.links.new(col, bsdf.inputs["Base Color"])
    return build


# ---------------------------------------------------------------------------
# export
# ---------------------------------------------------------------------------

def check_export_ready(ob, verts=(200, 8000), dims=None):
    """Raise before writing anything if the generated mesh is not a sane prop:
    missing UVs, an out-of-range vertex count, or dimensions outside `dims`
    ((w_min, w_max), (d_min, d_max), (h_min, h_max)) in cm."""
    if not ob.data.uv_layers:
        raise RuntimeError(f"{ob.name}: no UV layer (unwrap must run before the bake)")
    n = len(ob.data.vertices)
    if not verts[0] <= n <= verts[1]:
        raise RuntimeError(f"{ob.name}: vertex count {n} outside {verts}")
    if dims:
        for axis, value, (lo, hi) in zip("WDH", ob.dimensions, dims):
            if not lo <= value <= hi:
                raise RuntimeError(f"{ob.name}: dimension {axis}={value:.2f} outside ({lo}, {hi})")


def save_and_export_static(ob, blend_path, fbx_path):
    """Save .blend, then export this object alone as FBX with the Unreal axis convention.
    Static prop: no armature, no animation. Precondition: object mode, guards passed."""
    bpy.ops.wm.save_as_mainfile(filepath=blend_path)
    bpy.ops.object.select_all(action='DESELECT'); ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.export_scene.fbx(filepath=fbx_path, use_selection=True,
                             apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE',
                             axis_forward='X', axis_up='Z', add_leaf_bones=False,
                             bake_anim=False, mesh_smooth_type='FACE', path_mode='AUTO',
                             embed_textures=False)


def build_coral(spec):
    """Full pipeline for one coral: fresh scene -> geometry -> unwrap -> bake -> guards ->
    .blend + .fbx. Returns dict(verts, dims)."""
    name = spec["name"]
    F.reset_scene()
    ob = spec["build"](name)
    F.unwrap(ob)
    F.bake_base_color(ob, "M_" + name, spec["color"], "T_%s_BaseColor" % name,
                      EXPORT, size=TEXTURE_SIZE)
    check_export_ready(ob, dims=spec["dims"])
    save_and_export_static(ob, os.path.join(ROOT, name + ".blend"),
                           os.path.join(EXPORT, name + ".fbx"))
    return dict(verts=len(ob.data.vertices),
                dims=tuple(round(v, 2) for v in ob.dimensions))


def render_group_preview(specs, path, xs=(-120.0, 0.0, 120.0)):
    """Rebuild the three corals side by side in one fresh scene and render a single EEVEE
    still. The procedural material is re-created directly (no bake) because this is only a
    visual check; nothing here is saved or exported."""
    F.reset_scene()
    for spec, x in zip(specs, xs):
        ob = spec["build"](spec["name"])
        ob.location = (x, 0.0, 0.0)
        mat = bpy.data.materials.new("Preview_" + spec["name"])
        if mat.node_tree is None:
            mat.use_nodes = True
        nt = mat.node_tree; nt.nodes.clear()
        out = nt.nodes.new("ShaderNodeOutputMaterial")
        bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
        bsdf.inputs["Roughness"].default_value = 0.35
        spec["color"](nt, bsdf)
        nt.links.new(bsdf.outputs[0], out.inputs[0])
        ob.data.materials.append(mat)
    F.render_preview(path, cam_loc=(0.0, -620.0, 130.0), cam_rot=(1.40, 0.0, 0.0))


SPECS = [
    dict(name="BranchCoral", build=build_branch_coral,
         color=coral_color_fn((0.9, 0.35, 0.45, 1), (0.55, 0.15, 0.3, 1), scale=9.0),
         dims=((20, 100), (20, 100), (68, 72))),
    dict(name="PlateCoral", build=build_plate_coral,
         color=coral_color_fn((0.85, 0.7, 0.5, 1), (0.55, 0.42, 0.28, 1), scale=6.0),
         dims=((80, 120), (80, 120), (10, 22))),
    dict(name="BrainCoral", build=build_brain_coral,
         color=coral_color_fn((0.75, 0.6, 0.8, 1), (0.42, 0.3, 0.5, 1), scale=11.0),
         dims=((50, 72), (50, 72), (24, 40))),
]

if __name__ == "__main__":
    os.makedirs(EXPORT, exist_ok=True)
    results = [build_coral(s) for s in SPECS]
    render_group_preview(SPECS, os.path.join(EXPORT, "preview_corals.png"))
    print("CORALS_OK branch=%d plate=%d brain=%d dims=%s"
          % (results[0]["verts"], results[1]["verts"], results[2]["verts"],
             [r["dims"] for r in results]))
