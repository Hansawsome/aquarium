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

# Variance floor for every baked coral map. The library default 1e-6 only catches a perfectly
# constant image: a Bump node left at Blender's 0.001 default Distance baked a visually flat
# normal map with variance 2.4e-6 and still passed. 1e-4 sits ~40x above that observed noise
# floor and ~5.7x below the weakest real coral map (PlateCoral normal, 5.75e-4); the other
# normals run 1.9e-3..3.0e-3 and every roughness map runs 8.5e-2..1.4e-1, so a good bake clears
# it by a wide margin while a flat one cannot.
MIN_MAP_VARIANCE = 1e-4

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


FAN_HEIGHT = 60.0
TUBE_HEIGHT = 45.0


def build_fan_coral(name, seed=4711, height=FAN_HEIGHT, thickness=1.6, depth=4,
                    root_len=18.0, root_radius=2.4):
    """Sea fan: the branch-coral recursion constrained to the XZ plane, so the whole colony is a
    flat lattice one or two centimetres thick. Children fan out to either side of the parent with
    a strong +Z bias; because every direction has y = 0 the silhouette is a fan rather than a
    bush. Joined, scaled to `height`, then thickened along Y by Solidify.
    Precondition: object mode, scene reset. Returns the joined object."""
    rng = random.Random(seed)
    segments = []

    def add_segment(start, direction, length, radius):
        bpy.ops.mesh.primitive_cone_add(vertices=6, radius1=radius, radius2=radius * 0.7,
                                        depth=length, location=(0, 0, 0))
        seg = bpy.context.object
        seg.rotation_euler = direction.to_track_quat('Z', 'Y').to_euler()
        seg.location = start + direction * (length / 2)
        segments.append(seg)
        return start + direction * length, radius * 0.7

    def grow(start, direction, length, radius, level):
        tip, tip_radius = add_segment(start, direction, length, radius)
        if level >= depth:
            return
        for sign in (-1.0, 1.0):
            tilt = rng.uniform(0.35, 0.70) * sign
            child = (Matrix.Rotation(tilt, 3, Vector((0, 1, 0))) @ direction)
            child = (child + Vector((0, 0, 0.35))).normalized()
            child.y = 0.0
            child.normalize()
            grow(tip, child, length * rng.uniform(0.62, 0.80), tip_radius, level + 1)

    grow(Vector((0, 0, 0)), Vector((0, 0, 1)), root_len, root_radius, 0)

    bpy.ops.object.select_all(action='DESELECT')
    for s in segments:
        s.select_set(True)
    ob = segments[0]
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.join()
    ob.name = name
    ob.data.name = name
    ob.scale = (height / ob.dimensions.z,) * 3
    bpy.ops.object.transform_apply(scale=True)
    sol = ob.modifiers.new("Solidify", 'SOLIDIFY')
    sol.thickness = thickness
    sol.offset = 0
    bpy.ops.object.modifier_apply(modifier="Solidify")
    bpy.ops.object.shade_smooth()
    return ob


def build_tube_coral(name, seed=9182, count=11, height=TUBE_HEIGHT, radius=4.2,
                     spread=16.0, sides=10):
    """Cluster of upright open-topped tubes rising from a common base. Each tube leans slightly
    outward from the cluster centre, has its own height (x0.55..1.0) and its own radius
    (x0.75..1.15), and is hollow at the top: the rim is a ring of two concentric circles, which
    is what makes it read as a tube rather than a peg. Joined into one mesh.
    Precondition: object mode, scene reset. Returns the joined object."""
    rng = random.Random(seed)
    mesh = bpy.data.meshes.new(name)
    bm = bmesh.new()

    for i in range(count):
        angle = 2 * math.pi * i / count + rng.uniform(-0.25, 0.25)
        dist = spread * math.sqrt(rng.uniform(0.0, 1.0))
        bx, by = dist * math.cos(angle), dist * math.sin(angle)
        h = height * rng.uniform(0.55, 1.0)
        r_out = radius * rng.uniform(0.75, 1.15)
        r_in = r_out * 0.62
        lean_x = (bx / max(1e-6, spread)) * h * 0.14
        lean_y = (by / max(1e-6, spread)) * h * 0.14

        def ring(z, r, ox, oy):
            return [bm.verts.new((bx + ox + r * math.cos(2 * math.pi * j / sides),
                                  by + oy + r * math.sin(2 * math.pi * j / sides), z))
                    for j in range(sides)]

        bottom = ring(0.0, r_out, 0.0, 0.0)
        top_out = ring(h, r_out * 0.88, lean_x, lean_y)
        top_in = ring(h, r_in * 0.88, lean_x, lean_y)
        inner_bottom = ring(h * 0.25, r_in, lean_x * 0.25, lean_y * 0.25)
        for j in range(sides):
            k = (j + 1) % sides
            bm.faces.new((bottom[j], top_out[j], top_out[k], bottom[k]))          # outer wall
            bm.faces.new((top_out[j], top_in[j], top_in[k], top_out[k]))          # rim
            bm.faces.new((top_in[k], inner_bottom[k], inner_bottom[j], top_in[j]))  # inner wall
        bm.faces.new(list(reversed(inner_bottom)))                                 # tube floor
        bm.faces.new(bottom)                                                       # base cap

    bm.normal_update()
    bm.to_mesh(mesh)
    bm.free()

    ob = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(ob)
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


def coral_maps_fn(base, shade, detail, scale=7.0, bump_strength=0.35,
                  bump_distance=0.3):
    """Return a build_nodes(nt, bsdf, ctx) callback for fishlib.bake_maps.

    `base`/`shade` are the two tints mixed by a contrast-stretched noise texture, exactly as
    coral_color_fn does, so the baked base colour is unchanged in character. `detail(nt, ctx)`
    returns (height_socket, roughness_socket): the height drives a Bump node into the Principled
    Normal input (that is what the NORMAL bake picks up) and the roughness goes straight into
    Roughness. The height also darkens the base colour in the troughs, because a groove that is
    only a normal-map dent still reads flat under flat ambient light.

    `bump_distance` is the relief amplitude in centimetres and MUST be set explicitly: Blender
    5.2 defaults the Bump node's Distance socket to 0.001, not 1.0, so leaving it alone bakes a
    normal map with ~2e-6 pixel variance -- a map that passes _assert_not_degenerate on noise
    alone and then does nothing in Unreal. 0.3 cm puts the five coral normal maps at 1e-3..4e-3
    variance, a few times the fish maps' 5e-4, which is right for a coarser surface."""
    def build(nt, bsdf, ctx):
        tex = nt.nodes.new("ShaderNodeTexNoise")
        tex.inputs["Scale"].default_value = scale
        tex.inputs["Detail"].default_value = 6.0
        tex.inputs["Roughness"].default_value = 0.55
        stretch = nt.nodes.new("ShaderNodeMapRange")
        stretch.inputs["From Min"].default_value = 0.35
        stretch.inputs["From Max"].default_value = 0.65
        nt.links.new(tex.outputs["Fac"], stretch.inputs["Value"])
        col = F.mix_over(nt, base, stretch.outputs[0], shade)

        height, rough = detail(nt, ctx)

        # troughs (height near 0) get the shade tint laid over the mottled base
        ao = nt.nodes.new("ShaderNodeMath"); ao.operation = 'SUBTRACT'
        ao.inputs[0].default_value = 1.0
        nt.links.new(height, ao.inputs[1])
        darkened = F.mix_over(nt, col, ao.outputs[0], shade)
        nt.links.new(darkened, bsdf.inputs["Base Color"])

        nt.links.new(rough, bsdf.inputs["Roughness"])

        bump = nt.nodes.new("ShaderNodeBump")
        bump.inputs["Strength"].default_value = bump_strength
        bump.inputs["Distance"].default_value = bump_distance
        nt.links.new(height, bump.inputs["Height"])
        nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    return build


def detail_branch(nt, ctx):
    """Branch coral: small polyp bumps all over the tapered cylinders."""
    return ctx["scale_pattern_world"](nt, cell_size=1.1, sharpness=0.9)


def detail_plate(nt, ctx):
    """Plate coral: concentric growth rings, 2.4 cm apart, plus a fine polyp break-up."""
    return F.radial_ridge_pattern(nt, period=2.4, sharpness=1.3)


def detail_brain(nt, ctx):
    """Brain coral: a second, much finer meander on top of the geometric grooves. The mesh
    already carries the big convolutions; what is missing at 10 m is the texture inside them."""
    return ctx["scale_pattern_world"](nt, cell_size=0.8, sharpness=1.6)


def detail_fan(nt, ctx):
    """Sea fan: fine cross-hatch along the lattice, read as the polyp rows on each branch."""
    return F.ridge_pattern(nt, axis="Z", period=0.9, sharpness=1.1)


def detail_tube(nt, ctx):
    """Tube coral: horizontal growth bands around each tube, 1.8 cm apart."""
    return F.ridge_pattern(nt, axis="Z", period=1.8, sharpness=1.4)

# ---------------------------------------------------------------------------
# export
# ---------------------------------------------------------------------------

def check_export_ready(ob, verts=(200, 14000), dims=None):
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
    """Full pipeline for one coral: fresh scene -> geometry -> unwrap -> bake three maps ->
    guards -> .blend + .fbx. Returns dict(verts, dims)."""
    name = spec["name"]
    F.reset_scene()
    ob = spec["build"](name)
    F.unwrap(ob)
    F.bake_maps(ob, "M_" + name, spec["maps"], name, EXPORT, size=TEXTURE_SIZE,
                min_variance=MIN_MAP_VARIANCE)
    check_export_ready(ob, dims=spec["dims"])
    save_and_export_static(ob, os.path.join(ROOT, name + ".blend"),
                           os.path.join(EXPORT, name + ".fbx"))
    return dict(verts=len(ob.data.vertices),
                dims=tuple(round(v, 2) for v in ob.dimensions))


def render_group_preview(specs, path, xs=(-260.0, -130.0, 0.0, 130.0, 260.0)):
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
    F.render_preview(path, cam_loc=(0.0, -900.0, 150.0), cam_rot=(1.40, 0.0, 0.0))


SPECS = [
    dict(name="BranchCoral", build=build_branch_coral,
         color=coral_color_fn((0.9, 0.35, 0.45, 1), (0.55, 0.15, 0.3, 1), scale=9.0),
         maps=coral_maps_fn((0.9, 0.35, 0.45, 1), (0.55, 0.15, 0.3, 1), detail_branch, scale=9.0),
         dims=((20, 100), (20, 100), (68, 72))),
    dict(name="PlateCoral", build=build_plate_coral,
         color=coral_color_fn((0.85, 0.7, 0.5, 1), (0.55, 0.42, 0.28, 1), scale=6.0),
         maps=coral_maps_fn((0.85, 0.7, 0.5, 1), (0.55, 0.42, 0.28, 1), detail_plate, scale=6.0),
         dims=((80, 120), (80, 120), (10, 22))),
    dict(name="BrainCoral", build=build_brain_coral,
         color=coral_color_fn((0.75, 0.6, 0.8, 1), (0.42, 0.3, 0.5, 1), scale=11.0),
         maps=coral_maps_fn((0.75, 0.6, 0.8, 1), (0.42, 0.3, 0.5, 1), detail_brain, scale=11.0),
         dims=((50, 72), (50, 72), (24, 40))),
    dict(name="FanCoral", build=build_fan_coral,
         color=coral_color_fn((0.92, 0.45, 0.30, 1), (0.55, 0.20, 0.14, 1), scale=13.0),
         maps=coral_maps_fn((0.92, 0.45, 0.30, 1), (0.55, 0.20, 0.14, 1), detail_fan, scale=13.0),
         dims=((20, 90), (1, 12), (58, 62))),
    dict(name="TubeCoral", build=build_tube_coral,
         color=coral_color_fn((0.55, 0.80, 0.72, 1), (0.25, 0.45, 0.42, 1), scale=8.0),
         maps=coral_maps_fn((0.55, 0.80, 0.72, 1), (0.25, 0.45, 0.42, 1), detail_tube, scale=8.0),
         dims=((25, 60), (25, 60), (24, 48))),
]

if __name__ == "__main__":
    os.makedirs(EXPORT, exist_ok=True)
    results = [build_coral(s) for s in SPECS]
    render_group_preview(SPECS, os.path.join(EXPORT, "preview_corals.png"))
    print("CORALS_OK " + " ".join(
        "%s=%d" % (s["name"], r["verts"]) for s, r in zip(SPECS, results))
        + " dims=%s" % [r["dims"] for r in results])
