"""Shared Blender build steps for scripted fish.

Runs inside Blender 5.2 headless:
    /Applications/Blender.app/Contents/MacOS/Blender -b -P assets/blender/make_<species>.py

Conventions (must match the Unreal import side):
    +X = head, -X = tail, +Z = up, 1 unit = 1 cm.
    Bones: Root, Spine0..Spine{spine-1}, Tail, PecL, PecR (exactly these names).

Two body paths exist. The classic path (still used by make_corals.py) builds a tapered
sphere and bakes base colour only. The profile path (M4a, used by the five fish) lofts the
body from silhouette functions, adds eyeballs and tapered membrane fins, and bakes a full
BaseColor/Normal/Roughness set.

Classic call order (each step assumes the previous ones ran, object mode throughout):
    1. reset_scene()                       fresh empty scene, cm units
    2. build_body(...)                     body mesh at the origin (world-space masks rely on this)
    3. add_fin(...) x N                    fin plates with an unapplied Solidify modifier
    4. join_fins(body, fins)               applies Solidify, joins into body, shade smooth
    5. unwrap(body)                        smart UV project (bake needs UVs)
    6. bake_base_color(...)                material + bake; sets the scene render engine to CYCLES
    7. build_rig(...)                      armature, returns in object mode
    8. skin(body, arm)                     auto weights (needs the body to have no prior parent)
    9. assert_rig_contract(body, arm, spine)  raises on any rig contract violation
   10. save_and_export(...)                .blend then FBX (body + armature selection only)
   11. render_preview(...)                 adds camera/sun and switches to EEVEE; run last

Profile call order (steps 7..11 are identical to the classic path):
    1. reset_scene()
    2. build_body_profile(...)             lofted silhouette body at the origin
    3. add_eye(...) x2, add_membrane_fin(...) x N   separate objects in body space
    4. join_fins(body, parts)              joins eyes and fins into the body, shade smooth
    5. unwrap(body)                        smart UV project
    6. bake_maps(...)                      BaseColor + Normal + Roughness bake (CYCLES)
    7..11 build_rig / skin / assert_rig_contract / save_and_export / render_preview

build_species(spec) runs whichever order the spec asks for -- the profile path when the spec
carries profile/belly/width callables, the classic path otherwise; the make_<species>.py
scripts are specs plus one build_species call.
"""
import bpy, bmesh, math, mathutils, os


def reset_scene():
    """Fresh empty scene, metric units with 1 unit = 1 cm (matches Unreal). Returns the scene."""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.scale_length = 0.01   # 1 unit = 1 cm, matches Unreal
    return scene


def body_half_height(x, half_len, body_h, taper_z):
    """Half-height of the tapered hull built by build_body at longitudinal position x.
    Used to sink fin roots into the hull. Clamps outside the ellipse to 0."""
    t = max(0.0, -x / half_len)
    return (body_h / 2) * math.sqrt(max(0.0, 1.0 - (x / half_len) ** 2)) * (1.0 - taper_z * t * t)


def build_body(name, body_len, body_h, body_w, taper_z, taper_y, segments=48, rings=24):
    """Scaled UV sphere centered at the origin with the tail half (x<0) tapered toward the axis.
    Precondition: object mode, scene reset. Returns the body object (selected, active)."""
    L = body_len / 2
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments, ring_count=rings, radius=1.0)
    body = bpy.context.object
    body.name = name
    body.data.name = name
    body.scale = (L, body_w / 2, body_h / 2)
    bpy.ops.object.select_all(action='DESELECT'); body.select_set(True)
    bpy.context.view_layer.objects.active = body
    bpy.ops.object.transform_apply(scale=True)
    # taper the tail: pull vertices with x<0 toward the axis
    bm = bmesh.new(); bm.from_mesh(body.data)
    for v in bm.verts:
        t = max(0.0, -v.co.x / L)     # 0 at center, 1 at tail tip
        v.co.z *= 1.0 - taper_z * t * t
        v.co.y *= 1.0 - taper_y * t
    bm.to_mesh(body.data); bm.free()
    return body


def add_fin(name, verts, faces, thickness=0.25):
    """Flat fin plate linked to the scene collection, with an unapplied Solidify modifier
    (applied later by join_fins). Vertices are in body space (body at origin)."""
    mesh = bpy.data.meshes.new(name); mesh.from_pydata(verts, [], faces); mesh.update()
    ob = bpy.data.objects.new(name, mesh); bpy.context.scene.collection.objects.link(ob)
    sol = ob.modifiers.new("Solidify", 'SOLIDIFY'); sol.thickness = thickness; sol.offset = 0
    return ob


def join_fins(body, fins):
    """Apply fin modifiers, join everything into body, shade smooth.
    Precondition: object mode; body and fins are separate objects. Returns body."""
    bpy.ops.object.select_all(action='DESELECT')
    for f in fins: f.select_set(True)
    body.select_set(True); bpy.context.view_layer.objects.active = body
    bpy.ops.object.convert(target='MESH')      # apply solidify on fins
    bpy.ops.object.join()                      # everything into body
    bpy.ops.object.shade_smooth()
    return body


def unwrap(body):
    """Smart UV project of the whole mesh. Enters and leaves edit mode; ends in object mode."""
    bpy.ops.object.select_all(action='DESELECT'); body.select_set(True)
    bpy.context.view_layer.objects.active = body
    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.02)
    bpy.ops.object.mode_set(mode='OBJECT')


def bake_base_color(body, mat_name, build_color_nodes, image_name, export_dir, size=2048, samples=16):
    """Create a Principled material, let build_color_nodes(nt, bsdf) wire Base Color,
    bake diffuse color to <export_dir>/<image_name>.png and rewire the baked texture
    as the exported base color. Returns (material, image).
    Precondition: body has UVs (unwrap). Side effect: scene render engine set to CYCLES (CPU)."""
    scene = bpy.context.scene
    mat = bpy.data.materials.new(mat_name)
    if mat.node_tree is None:   # Blender 5.x may return a material without a node tree
        mat.use_nodes = True
    nt = mat.node_tree; nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled"); bsdf.inputs["Roughness"].default_value = 0.35
    build_color_nodes(nt, bsdf)
    nt.links.new(bsdf.outputs[0], out.inputs[0])
    body.data.materials.append(mat)

    img = bpy.data.images.new(image_name, size, size)
    tex_node = nt.nodes.new("ShaderNodeTexImage"); tex_node.image = img
    for n in nt.nodes: n.select = False
    tex_node.select = True
    nt.nodes.active = tex_node
    scene.render.engine = 'CYCLES'; scene.cycles.device = 'CPU'; scene.cycles.samples = samples
    scene.render.bake.use_pass_direct = False; scene.render.bake.use_pass_indirect = False
    scene.render.bake.use_pass_color = True
    bpy.ops.object.select_all(action='DESELECT'); body.select_set(True)
    bpy.context.view_layer.objects.active = body
    bpy.ops.object.bake(type='DIFFUSE', margin=8)
    img.filepath_raw = os.path.join(export_dir, image_name + ".png"); img.file_format = 'PNG'; img.save()
    # rewire baked texture as the exported base color
    nt.links.new(tex_node.outputs["Color"], bsdf.inputs["Base Color"])
    return mat, img


def build_rig(rig_name, half_len, pec_root_y, pec_z=0.5, spine=6, tail_tip_x=None,
              pec_span_y=4.0, pec_drop_z=2.5):
    """Armature: Root (non-deform) -> Spine0..Spine{spine-1} -> Tail; PecL/PecR under Spine1.
    Spine chain runs from x=0.95*half_len to x=-0.9*half_len. Pectoral bones start at
    (0.35*half_len, +-pec_root_y, pec_z) and end at (0.05*half_len, +-(pec_root_y+pec_span_y),
    pec_z-pec_drop_z). tail_tip_x defaults to -half_len-6, tuned for the blue tang's tail fin;
    pass it explicitly for other species. Precondition: object mode. Returns arm in object mode."""
    L = half_len
    if tail_tip_x is None:
        tail_tip_x = -L - 6
    bpy.ops.object.armature_add(enter_editmode=True, location=(0, 0, 0))
    arm = bpy.context.object; arm.name = rig_name; arm.data.name = rig_name
    ebones = arm.data.edit_bones
    for b in list(ebones): ebones.remove(b)
    root = ebones.new("Root"); root.head = (0, 0, 0); root.tail = (0, 0, 1)
    root.use_deform = False   # Root is a transform anchor only; heat weighting must ignore it
    x_head = L * 0.95; x_tail = -L * 0.9
    step = (x_head - x_tail) / spine
    prev = root
    for i in range(spine):
        b = ebones.new(f"Spine{i}")
        b.head = (x_head - step * i, 0, 0); b.tail = (x_head - step * (i + 1), 0, 0)
        b.parent = prev; b.use_connect = (i > 0); prev = b
    tailb = ebones.new("Tail"); tailb.head = prev.tail; tailb.tail = (tail_tip_x, 0, 0)
    tailb.parent = prev; tailb.use_connect = True
    for name, sign in (("PecL", 1), ("PecR", -1)):
        pb = ebones.new(name); pb.head = (L * 0.35, sign * pec_root_y, pec_z)
        pb.tail = (L * 0.05, sign * (pec_root_y + pec_span_y), pec_z - pec_drop_z); pb.parent = ebones["Spine1"]
    bpy.ops.object.mode_set(mode='OBJECT')
    return arm


def skin(body, arm):
    """Parent body to armature with automatic (heat) weights. Precondition: object mode,
    body has no parent yet, armature built."""
    bpy.ops.object.select_all(action='DESELECT'); body.select_set(True); arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.parent_set(type='ARMATURE_AUTO')


def assert_rig_contract(body, arm, spine):
    """Verify the rig contract after skin(): exact bone set, Root non-deform, every vertex
    weighted, and no weight on Root. Raises RuntimeError on any violation.
    Returns (unweighted, root_max_w); on normal return unweighted == 0 and root_max_w == 0.0."""
    expected = {"Root", "Tail", "PecL", "PecR"} | {f"Spine{i}" for i in range(spine)}
    actual = {b.name for b in arm.data.bones}
    if actual != expected:
        raise RuntimeError(f"bone set mismatch: {sorted(actual)}")
    if arm.data.bones["Root"].use_deform:
        raise RuntimeError("Root bone must have use_deform=False")
    root_gi = body.vertex_groups["Root"].index if "Root" in body.vertex_groups else -1
    unweighted = 0; root_max_w = 0.0
    for v in body.data.vertices:
        total = sum(g.weight for g in v.groups)
        if total < 1e-4: unweighted += 1
        for g in v.groups:
            if g.group == root_gi: root_max_w = max(root_max_w, g.weight)
    if unweighted:
        raise RuntimeError(f"{unweighted} body vertices have no skin weight")
    if root_max_w != 0.0:
        raise RuntimeError(f"Root bone carries skin weight (max {root_max_w:.3f})")
    return unweighted, root_max_w


def save_and_export(body, arm, blend_path, fbx_path):
    """Save .blend, then export body+armature only (use_selection) as FBX with the Unreal axis
    convention. Precondition: object mode, rig contract verified."""
    bpy.ops.wm.save_as_mainfile(filepath=blend_path)
    bpy.ops.object.select_all(action='DESELECT'); body.select_set(True); arm.select_set(True)
    bpy.ops.export_scene.fbx(filepath=fbx_path, use_selection=True,
                             apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE',
                             axis_forward='X', axis_up='Z', add_leaf_bones=False,
                             bake_anim=False, mesh_smooth_type='FACE', path_mode='AUTO', embed_textures=False)


def render_preview(path, cam_loc, cam_rot, resolution=(1280, 720), fill_energy=0.0):
    """EEVEE still to `path`. Adds PreviewCam/PreviewSun to the scene collection and switches the
    render engine to EEVEE. The FBX is already protected by use_selection; calling this last keeps
    the camera, light and EEVEE settings out of the saved .blend.
    fill_energy > 0 adds a second sun aimed from the camera side, which lifts surfaces that face
    the viewer (pectoral fins) out of the key light's shadow; 0.0 is the original single-sun setup."""
    scene = bpy.context.scene
    cam = bpy.data.objects.new("PreviewCam", bpy.data.cameras.new("PreviewCam"))
    scene.collection.objects.link(cam)
    cam.location = cam_loc; cam.rotation_euler = cam_rot
    scene.camera = cam
    sun = bpy.data.objects.new("PreviewSun", bpy.data.lights.new("PreviewSun", 'SUN'))
    scene.collection.objects.link(sun)
    sun.rotation_euler = (0.8, 0.3, 0.5)
    if fill_energy > 0.0:
        fill_data = bpy.data.lights.new("PreviewFill", 'SUN')
        fill_data.energy = fill_energy
        fill = bpy.data.objects.new("PreviewFill", fill_data)
        scene.collection.objects.link(fill)
        fill.rotation_euler = (1.1, -0.2, -2.4)     # from the camera side, low
    scene.render.engine = 'BLENDER_EEVEE'
    scene.render.resolution_x, scene.render.resolution_y = resolution
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)


# ---------------------------------------------------------------------------
# Procedural color-graph helpers (used by spec color_fn callbacks)
# ---------------------------------------------------------------------------

def position_axis(nt):
    """Geometry > Position -> SeparateXYZ. World space, so the body must sit at the origin.
    Returns the SeparateXYZ node (use .outputs["X"|"Y"|"Z"])."""
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(geo.outputs["Position"], sep.inputs[0])
    return sep


def axis_band_mask(nt, axis_socket, center=None, half_width=None, ramp=None,
                   from_min=None, from_max=None, use_abs=True, invert=False):
    """0..1 band mask along one axis, returned as a socket.

    Shape by half_width/ramp: 1 inside |axis-center| <= half_width, falling to 0 at
    half_width + ramp (an inverted MapRange, From Min > From Max).
    Alternatively pass from_min/from_max to drive the MapRange directly, which lets a
    caller reproduce an existing graph exactly (including ascending ramps).
    use_abs=False skips the ABSOLUTE node (signed ramp along the axis).
    invert=True appends a 1 - value SUBTRACT node."""
    val = axis_socket
    if center is not None:   # explicit 0.0 still inserts the (no-op) offset node
        d = nt.nodes.new("ShaderNodeMath"); d.operation = 'SUBTRACT'
        d.inputs[1].default_value = center
        nt.links.new(val, d.inputs[0]); val = d.outputs[0]
    if use_abs:
        a = nt.nodes.new("ShaderNodeMath"); a.operation = 'ABSOLUTE'
        nt.links.new(val, a.inputs[0]); val = a.outputs[0]
    if from_min is None or from_max is None:
        from_min = half_width + ramp
        from_max = half_width
    rampn = nt.nodes.new("ShaderNodeMapRange")
    rampn.inputs["From Min"].default_value = from_min
    rampn.inputs["From Max"].default_value = from_max
    nt.links.new(val, rampn.inputs["Value"]); val = rampn.outputs[0]
    if invert:
        inv = nt.nodes.new("ShaderNodeMath"); inv.operation = 'SUBTRACT'
        inv.inputs[0].default_value = 1.0
        nt.links.new(val, inv.inputs[1]); val = inv.outputs[0]
    return val


def mix_over(nt, base, mask_socket, color):
    """Mix RGBA: `color` laid over `base` by mask_socket. `base` is either a socket or an
    RGBA tuple. Returns the Result socket."""
    m = nt.nodes.new("ShaderNodeMix"); m.data_type = 'RGBA'
    m.inputs["B"].default_value = color
    if isinstance(base, (tuple, list)):
        rgb = nt.nodes.new("ShaderNodeRGB"); rgb.outputs[0].default_value = base
        nt.links.new(rgb.outputs[0], m.inputs["A"])
    else:
        nt.links.new(base, m.inputs["A"])
    nt.links.new(mask_socket, m.inputs["Factor"])
    return m.outputs["Result"]


def pec_stray_count(body, half_len, x_min=-0.05, x_max=0.45):
    """Diagnostic: body vertices outside the pectoral x window (fractions of half_len)
    that are dominated (weight > 0.5) by PecL or PecR. Run after skin()."""
    pec_gi = {body.vertex_groups[n].index for n in ("PecL", "PecR")}
    lo, hi = half_len * x_min, half_len * x_max
    return sum(1 for v in body.data.vertices
               if not (lo <= v.co.x <= hi)
               and any(g.group in pec_gi and g.weight > 0.5 for g in v.groups))


def build_species(spec):
    """Run the full documented pipeline for one species described as data.

    Spec keys: name, body=(len, h, w, taper_z, taper_y), fin_thickness,
    fins=[(fin_name, verts_fn(ctx), faces)], color_fn(nt, bsdf, ctx),
    rig=dict(pec_z, tail_tip_x, pec_span_y, pec_drop_z), spine (6),
    cam_loc, cam_rot ((1.35, 0, 0.42)), root_dir, export_dir, texture_size (2048),
    preview_name (preview_<name.lower()>.png), pec_window ((-0.05, 0.45)).
    ctx = {"L", "BODY_H", "BODY_W", "PEC_ROOT_Y", "body_h"}.
    Returns dict(verts, bones, unweighted, root_max_w, pec_stray).

    A spec that carries profile/belly/width callables takes the profile path instead
    (_build_species_profile); see that function for its extra keys."""
    if "profile" in spec:
        return _build_species_profile(spec)
    name = spec["name"]
    body_len, body_h_, body_w, taper_z, taper_y = spec["body"]
    L = body_len / 2
    pec_root_y = body_w * 0.45
    spine = spec.get("spine", 6)
    export_dir = spec["export_dir"]
    os.makedirs(export_dir, exist_ok=True)
    ctx = {"L": L, "BODY_H": body_h_, "BODY_W": body_w, "PEC_ROOT_Y": pec_root_y,
           "body_h": lambda x: body_half_height(x, L, body_h_, taper_z)}

    reset_scene()
    body = build_body(name, body_len, body_h_, body_w, taper_z=taper_z, taper_y=taper_y)
    thickness = spec.get("fin_thickness", 0.25)
    fins = [add_fin(fname, verts_fn(ctx), faces, thickness=thickness)
            for fname, verts_fn, faces in spec["fins"]]
    join_fins(body, fins)
    unwrap(body)
    color_fn = spec["color_fn"]
    bake_base_color(body, "M_" + name, lambda nt, bsdf: color_fn(nt, bsdf, ctx),
                    "T_%s_BaseColor" % name, export_dir, size=spec.get("texture_size", 2048))
    rig = spec["rig"]
    arm = build_rig(name + "Rig", L, pec_root_y, pec_z=rig["pec_z"], spine=spine,
                    tail_tip_x=rig.get("tail_tip_x"), pec_span_y=rig["pec_span_y"],
                    pec_drop_z=rig["pec_drop_z"])
    skin(body, arm)
    unweighted, root_max_w = assert_rig_contract(body, arm, spine)
    x_min, x_max = spec.get("pec_window", (-0.05, 0.45))
    pec_stray = pec_stray_count(body, L, x_min, x_max)
    save_and_export(body, arm, os.path.join(spec["root_dir"], name + ".blend"),
                    os.path.join(export_dir, name + ".fbx"))
    preview = spec.get("preview_name", "preview_%s.png" % name.lower())
    render_preview(os.path.join(export_dir, preview), spec["cam_loc"],
                   spec.get("cam_rot", (1.35, 0, 0.42)))
    return dict(verts=len(body.data.vertices), bones=len(arm.data.bones),
                unweighted=unweighted, root_max_w=root_max_w, pec_stray=pec_stray)


# ---------------------------------------------------------------------------
# M4a building blocks (additive; the classic build_body/add_fin path is unchanged)
# ---------------------------------------------------------------------------

def build_body_profile(name, length, profile, belly, width, sections=28, ring_segments=20):
    """Lofted fish body from silhouette functions instead of a scaled sphere.

    t runs 0 (nose, +X) to 1 (tail tip, -X). profile(t) is the height above the spine and belly(t)
    the depth below it, width(t) the half width -- all in cm. Sections are elliptical but vertically
    asymmetric (back and belly differ), which is what makes the silhouette read as a fish rather
    than a capsule. The nose and tail rings collapse to a point so the mesh is closed and manifold.
    Precondition: object mode, scene reset. Returns the body object (selected, active)."""
    half = length / 2.0
    bm = bmesh.new()

    def ring(t):
        """One closed cross-section ring at parameter t, as a list of bmesh verts."""
        x = half - t * length
        up = max(1e-4, profile(t))
        down = max(1e-4, belly(t))
        w = max(1e-4, width(t))
        verts = []
        for i in range(ring_segments):
            a = 2.0 * math.pi * i / ring_segments
            # a = 0 is the top of the section; sin drives y, cos drives z
            cz = math.cos(a)
            z = cz * (up if cz >= 0.0 else down)
            y = math.sin(a) * w
            verts.append(bm.verts.new((x, y, z)))
        return verts

    # interior rings only; the two ends are single points capped with fans
    rings = [ring((i + 1) / (sections + 1.0)) for i in range(sections)]
    nose = bm.verts.new((half, 0.0, 0.0))
    tail = bm.verts.new((-half, 0.0, 0.0))

    for a, b in zip(rings, rings[1:]):
        for i in range(ring_segments):
            j = (i + 1) % ring_segments
            bm.faces.new((a[i], a[j], b[j], b[i]))
    for i in range(ring_segments):          # nose fan
        j = (i + 1) % ring_segments
        bm.faces.new((nose, rings[0][j], rings[0][i]))
    for i in range(ring_segments):          # tail fan
        j = (i + 1) % ring_segments
        bm.faces.new((tail, rings[-1][i], rings[-1][j]))

    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh); bm.free(); mesh.update()
    body = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(body)
    bpy.ops.object.select_all(action='DESELECT')
    body.select_set(True); bpy.context.view_layer.objects.active = body
    bpy.ops.object.shade_smooth()
    return body


def add_eye(name, centre, radius, sink=0.0):
    """Eyeball sphere at `centre` (cm, body space), pushed `sink` cm toward the body axis so it
    sits in a socket. Joined into the body later and sharing the body's UV space, so the iris and
    pupil come from the baked colour map. Returns the object."""
    x, y, z = centre
    if sink and (y or z):
        # push along the -y/-z direction, i.e. toward the body's long axis at this x
        n = math.hypot(y, z)
        y -= sink * y / n
        z -= sink * z / n
    bpy.ops.mesh.primitive_uv_sphere_add(segments=24, ring_count=16, radius=radius,
                                         location=(x, y, z))
    eye = bpy.context.object
    eye.name = name; eye.data.name = name
    bpy.ops.object.shade_smooth()
    return eye


def add_membrane_fin(name, outline, thickness_root, thickness_edge, rays=0, ray_depth=0.0,
                     steps=6):
    """Fin membrane in the XZ plane (y = 0) whose thickness tapers from `thickness_root` at the
    first outline vertex to `thickness_edge` at the outer rim, optionally ridged by `rays` fin rays
    of `ray_depth` cm. Flat plates read as paper; the taper and the rays are what make a fin.
    `outline` is a list of (x, z) in body space, first vertex = root. Returns the object."""
    if len(outline) < 3:
        raise ValueError("membrane fin outline needs at least 3 points")
    root = mathutils.Vector(outline[0])
    rim = [mathutils.Vector(p) for p in outline[1:]]
    if not rim:
        raise ValueError("membrane fin outline needs rim points after the root")
    far = max((p - root).length for p in rim) or 1.0

    # Fin rays run from the root out to every (len(rim)/rays)-th rim point.
    ray_dirs = []
    if rays > 0 and ray_depth > 0.0:
        step = max(1, len(rim) / float(rays))
        for i in range(rays):
            d = rim[min(len(rim) - 1, int(round(i * step)))] - root
            if d.length > 1e-6:
                ray_dirs.append(d.normalized())

    def half_thickness(p):
        """Half thickness at p: root value at the root, edge value at the rim, plus any ray ridge."""
        d = mathutils.Vector(p) - root
        r = d.length
        u = min(1.0, r / far)
        t = 0.5 * (thickness_root + (thickness_edge - thickness_root) * u)
        if ray_dirs and r > 1e-6:
            # sharp falloff off-ray; ridges swell just outside the root and fade into the rim
            lobe = max(0.0, max(d.normalized().dot(a) for a in ray_dirs)) ** 10
            t += ray_depth * 0.5 * lobe * (1.0 - u) ** 0.6 * min(1.0, u * 5.0)
        return t

    # The membrane is tessellated root -> rim so the ridges have interior vertices to rise on,
    # and both shells are built explicitly (not via Solidify) so thickness varies per vertex.
    grid = [[root.lerp(p, s / float(steps)) for p in rim] for s in range(1, steps + 1)]
    verts, faces = [], []
    for sign in (1.0, -1.0):
        base = len(verts)
        verts.append((root.x, 0.0, root.y))            # shared apex per shell
        for row in grid:
            for p in row:
                t = half_thickness(p)
                verts.append((p.x, sign * t, p.y))
        w = len(rim)
        def idx(s, i):                                  # s in 0..steps-1 (grid row), i along rim
            return base + 1 + s * w + i
        for i in range(w - 1):                          # apex fan onto the first row
            tri = (base, idx(0, i), idx(0, i + 1))
            faces.append(tri if sign > 0 else tri[::-1])
        for s in range(steps - 1):                      # quad rows outward
            for i in range(w - 1):
                q = (idx(s, i), idx(s, i + 1), idx(s + 1, i + 1), idx(s + 1, i))
                faces.append(q if sign > 0 else q[::-1])

    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces); mesh.update()
    ob = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(ob)
    bpy.ops.object.select_all(action='DESELECT')
    ob.select_set(True); bpy.context.view_layer.objects.active = ob

    # Weld the two shells along the rim and the two straight root-to-rim edges, so the fin is
    # a single closed solid rather than two loose sheets.
    w = len(rim)
    shell = 1 + steps * w
    bm = bmesh.new(); bm.from_mesh(mesh); bm.verts.ensure_lookup_table()
    def top(s, i): return bm.verts[1 + s * w + i]
    def bot(s, i): return bm.verts[shell + 1 + s * w + i]
    for i in range(w - 1):                              # outer rim
        bm.faces.new((top(steps - 1, i), bot(steps - 1, i),
                      bot(steps - 1, i + 1), top(steps - 1, i + 1)))
    for s in range(steps - 1):                          # the two side edges back to the root
        for a, b in ((0, 0), (w - 1, w - 1)):
            bm.faces.new((top(s, a), bot(s, b), bot(s + 1, b), top(s + 1, a)))
    for i in (0, w - 1):                                # apex triangles closing the side edges
        bm.faces.new((bm.verts[0], bm.verts[shell], bot(0, i), top(0, i)))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=1e-5)
    bm.to_mesh(mesh); bm.free(); mesh.update()
    bpy.ops.object.shade_smooth()
    return ob


def scale_pattern(nt, cell_size, sharpness=1.0):
    """Voronoi-based scale pattern. Returns (height_socket, roughness_socket) in 0..1: feed the
    height into a Bump node so it lands in the normal bake, the roughness into Principled.

    `sharpness` is the border contrast: 1.0 is a linear dome-to-groove ramp (soft ridges), higher
    values push the plateau outward and leave a narrow, hard-edged crack. It was 3.0 through the
    first M4a bake, which together with a bump strength of 0.8 read as crazed ceramic glaze rather
    than scales."""
    tex = nt.nodes.new("ShaderNodeTexVoronoi")
    tex.voronoi_dimensions = '3D'
    tex.feature = 'DISTANCE_TO_EDGE'
    tex.inputs["Scale"].default_value = 1.0 / max(1e-4, cell_size)
    # Cells only need enough jitter to avoid a visible grid; full randomness makes them read as
    # irregular cracked plates instead of rows of scales.
    if "Randomness" in tex.inputs:
        tex.inputs["Randomness"].default_value = 0.55
    # Distance to edge is 0 on the scale border and grows toward the middle: ramp it so each
    # cell domes up and the borders sink in as soft grooves.
    dome = nt.nodes.new("ShaderNodeMapRange")
    dome.inputs["From Min"].default_value = 0.0
    dome.inputs["From Max"].default_value = 0.30
    dome.clamp = True
    nt.links.new(tex.outputs["Distance"], dome.inputs["Value"])
    shape = nt.nodes.new("ShaderNodeMath"); shape.operation = 'POWER'
    shape.inputs[1].default_value = max(0.01, 1.0 / sharpness)
    nt.links.new(dome.outputs[0], shape.inputs[0])
    height = shape.outputs[0]
    # Grooves stay wetter/glossier than the scale faces, so roughness rides the inverse.
    rough = nt.nodes.new("ShaderNodeMapRange")
    rough.inputs["To Min"].default_value = 1.0
    rough.inputs["To Max"].default_value = 0.0
    rough.clamp = True
    nt.links.new(height, rough.inputs["Value"])
    return height, rough.outputs[0]


def _bake_image(nt, img, body, bake_type, extra=None):
    """Make `img` the active bake target in nt and run one bake of `bake_type` onto body."""
    node = nt.nodes.new("ShaderNodeTexImage"); node.image = img
    for n in nt.nodes:
        n.select = False
    node.select = True
    nt.nodes.active = node
    bpy.ops.object.select_all(action='DESELECT'); body.select_set(True)
    bpy.context.view_layer.objects.active = body
    bpy.ops.object.bake(type=bake_type, margin=8, **(extra or {}))
    return node


def bake_maps(body, mat_name, build_nodes, base_name, export_dir, size=2048, samples=16):
    """Bakes base colour, normal and roughness to <export_dir>/T_<base_name>_{BaseColor,Normal,
    Roughness}.png and rewires the baked textures into the material. build_nodes(nt, bsdf, ctx)
    wires Base Color, Roughness and a Bump/Normal input; ctx carries {'scale_pattern': ...} and the
    colour helpers. Normal and roughness images are saved Non-Color. Raises RuntimeError if a baked
    map is degenerate (e.g. a normal map with near-zero pixel variance means nothing was baked).
    Returns dict(material=..., images={'BaseColor':..., 'Normal':..., 'Roughness':...})."""
    scene = bpy.context.scene
    mat = bpy.data.materials.new(mat_name)
    if mat.node_tree is None:   # Blender 5.x may return a material without a node tree
        mat.use_nodes = True
    nt = mat.node_tree; nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled"); bsdf.inputs["Roughness"].default_value = 0.35
    ctx = {"scale_pattern": scale_pattern, "position_axis": position_axis,
           "axis_band_mask": axis_band_mask, "mix_over": mix_over}
    build_nodes(nt, bsdf, ctx)
    nt.links.new(bsdf.outputs[0], out.inputs[0])
    body.data.materials.append(mat)

    scene.render.engine = 'CYCLES'; scene.cycles.device = 'CPU'; scene.cycles.samples = samples
    scene.render.bake.use_pass_direct = False; scene.render.bake.use_pass_indirect = False
    scene.render.bake.use_pass_color = True

    os.makedirs(export_dir, exist_ok=True)
    images, nodes = {}, {}
    for suffix, bake_type, non_color in (("BaseColor", 'DIFFUSE', False),
                                         ("Normal", 'NORMAL', True),
                                         ("Roughness", 'ROUGHNESS', True)):
        name = "T_%s_%s" % (base_name, suffix)
        img = bpy.data.images.new(name, size, size, is_data=non_color)
        if non_color:
            img.colorspace_settings.name = 'Non-Color'
        nodes[suffix] = _bake_image(nt, img, body, bake_type)
        _assert_not_degenerate(img, suffix)
        img.filepath_raw = os.path.join(export_dir, name + ".png")
        img.file_format = 'PNG'
        img.save()
        images[suffix] = img

    # rewire the baked maps back into the material so the .blend previews what was exported
    nt.links.new(nodes["BaseColor"].outputs["Color"], bsdf.inputs["Base Color"])
    nt.links.new(nodes["Roughness"].outputs["Color"], bsdf.inputs["Roughness"])
    nmap = nt.nodes.new("ShaderNodeNormalMap")
    nt.links.new(nodes["Normal"].outputs["Color"], nmap.inputs["Color"])
    nt.links.new(nmap.outputs["Normal"], bsdf.inputs["Normal"])
    return dict(material=mat, images=images)


def _assert_not_degenerate(img, suffix, min_variance=1e-6):
    """Raise if a freshly baked map carries no signal (a flat normal map, a constant roughness).
    Variance is measured per channel and the best channel decides: pooling R, G and B would let a
    flat tangent normal map (a constant 0.5, 0.5, 1.0) pass on the channel spread alone.
    Samples the buffer rather than reading every pixel of a 2k image."""
    px = img.pixels[:]
    n_px = len(px) // 4
    if n_px == 0:
        raise RuntimeError("baked %s map has no pixels" % suffix)
    step = max(1, n_px // 20000)                   # ~20k sampled pixels
    idx = range(0, n_px, step)
    stats = []
    for c in (0, 1, 2):
        vals = [px[i * 4 + c] for i in idx]
        mean = sum(vals) / len(vals)
        stats.append((sum((v - mean) ** 2 for v in vals) / len(vals), mean))
    best_var, best_mean = max(stats)
    if best_var < min_variance:
        raise RuntimeError("baked %s map is degenerate (max per-channel pixel variance %.3g, "
                           "mean %.3f); nothing was baked" % (suffix, best_var, best_mean))
    return best_var


def radial_mask(nt, centre, radius, softness=0.35):
    """0..1 disc mask around a world-space (x, z) point, measured in the XZ plane so it wraps
    both flanks of the fish at once. Used to paint the iris and pupil on the joined eyeball
    without trusting its smart-projected UV island. Returns a socket."""
    cx, cz = centre
    sep = position_axis(nt)
    dx = nt.nodes.new("ShaderNodeMath"); dx.operation = 'SUBTRACT'
    dx.inputs[1].default_value = cx
    nt.links.new(sep.outputs["X"], dx.inputs[0])
    dz = nt.nodes.new("ShaderNodeMath"); dz.operation = 'SUBTRACT'
    dz.inputs[1].default_value = cz
    nt.links.new(sep.outputs["Z"], dz.inputs[0])
    sx = nt.nodes.new("ShaderNodeMath"); sx.operation = 'MULTIPLY'
    nt.links.new(dx.outputs[0], sx.inputs[0]); nt.links.new(dx.outputs[0], sx.inputs[1])
    sz = nt.nodes.new("ShaderNodeMath"); sz.operation = 'MULTIPLY'
    nt.links.new(dz.outputs[0], sz.inputs[0]); nt.links.new(dz.outputs[0], sz.inputs[1])
    add = nt.nodes.new("ShaderNodeMath"); add.operation = 'ADD'
    nt.links.new(sx.outputs[0], add.inputs[0]); nt.links.new(sz.outputs[0], add.inputs[1])
    dist = nt.nodes.new("ShaderNodeMath"); dist.operation = 'SQRT'
    nt.links.new(add.outputs[0], dist.inputs[0])
    ramp = nt.nodes.new("ShaderNodeMapRange")          # 1 inside the disc, 0 outside the soft rim
    ramp.inputs["From Min"].default_value = radius * (1.0 + softness)
    ramp.inputs["From Max"].default_value = radius
    ramp.clamp = True
    nt.links.new(dist.outputs[0], ramp.inputs["Value"])
    return ramp.outputs[0]


def _smoothstep(u):
    u = min(1.0, max(0.0, u))
    return u * u * (3.0 - 2.0 * u)


def hump(peak_t, peak, tail_frac=0.18, rise=1.0, fall=1.0):
    """Silhouette function factory for build_body_profile: 0 at t=0, `peak` at t=peak_t,
    peak*tail_frac at t=1. Starting at 0 satisfies build_body_profile's caller contract
    (ease the nose yourself or the head comes out a chopped slab). rise < 1 blunts the head,
    rise > 1 draws it out into a snout; fall does the same for the tail side."""
    def f(t):
        if t <= peak_t:
            return peak * _smoothstep(t / max(1e-6, peak_t)) ** rise
        u = _smoothstep((1.0 - t) / max(1e-6, 1.0 - peak_t)) ** fall
        return peak * (tail_frac + (1.0 - tail_frac) * u)
    return f


def polyline(points, n):
    """Resample a list of (x, z) key points into n evenly spaced points along the polyline.
    Membrane fins want 30-40 outline points; this turns a handful of hand-placed keys into
    an outline dense enough for smooth rays."""
    segs = [(mathutils.Vector(a), mathutils.Vector(b)) for a, b in zip(points, points[1:])]
    lens = [(b - a).length for a, b in segs]
    total = sum(lens)
    if total <= 0.0:
        raise ValueError("polyline has zero length")
    out = []
    for i in range(n):
        d = total * i / (n - 1.0)
        for (a, b), l in zip(segs, lens):
            if d <= l or (a, b) == segs[-1]:
                out.append(tuple(a.lerp(b, 0.0 if l == 0 else min(1.0, d / l))))
                break
            d -= l
    return out


def _place(ob, rotate=None, translate=None):
    """Move a freshly built part into place and apply the transform, so that the world-space
    position masks used by the colour graph see the final coordinates."""
    if rotate:
        ob.rotation_euler = rotate
    if translate:
        ob.location = translate
    bpy.ops.object.select_all(action='DESELECT')
    ob.select_set(True); bpy.context.view_layer.objects.active = ob
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return ob


def _build_species_profile(spec):
    """build_species' profile path: lofted body + eyes + membrane fins + three baked maps.

    Extra spec keys (on top of the shared name/rig/spine/cam/root_dir/export_dir ones):
        length                 body length in cm, nose to caudal peduncle (t=0 .. t=1)
        profile, belly, width  silhouette callables of t (see build_body_profile / hump)
        sections, ring_segments  body resolution (defaults 28 / 20)
        eye                    dict(centre=(x, z), radius, sink) -- mirrored to both flanks
        fins_membrane          list of dicts(name, outline=fn(ctx) -> [(x, z)], thickness_root,
                               thickness_edge, rays, ray_depth, steps, rotate, translate)
        build_nodes(nt, bsdf, ctx)  wires Base Color, Roughness and Bump/Normal for bake_maps
    ctx adds profile/belly/width, x_of_t/t_of_x and top/bottom (body surface z at t).
    Returns the classic dict plus maps={suffix: path}."""
    name = spec["name"]
    length = spec["length"]
    L = length / 2.0
    profile, belly, width = spec["profile"], spec["belly"], spec["width"]
    ts = [i / 40.0 for i in range(41)]
    body_h = max(profile(t) + belly(t) for t in ts)
    body_w = 2.0 * max(width(t) for t in ts)
    pec_root_y = body_w * 0.45
    spine = spec.get("spine", 6)
    export_dir = spec["export_dir"]
    os.makedirs(export_dir, exist_ok=True)
    ctx = {"L": L, "LENGTH": length, "BODY_H": body_h, "BODY_W": body_w,
           "PEC_ROOT_Y": pec_root_y, "profile": profile, "belly": belly, "width": width,
           "x_of_t": lambda t: L - t * length, "t_of_x": lambda x: (L - x) / length,
           "top": lambda t: profile(t), "bottom": lambda t: -belly(t)}

    reset_scene()
    body = build_body_profile(name, length, profile, belly, width,
                              sections=spec.get("sections", 28),
                              ring_segments=spec.get("ring_segments", 20))
    parts = []
    eye = spec.get("eye")
    if eye:
        ex, ez = eye["centre"]
        ey = width(ctx["t_of_x"](ex)) * eye.get("out", 0.85)
        for side, sign in (("L", 1.0), ("R", -1.0)):
            parts.append(add_eye("Eye" + side, (ex, sign * ey, ez), eye["radius"],
                                 sink=eye.get("sink", 0.0)))
    for f in spec.get("fins_membrane", []):
        ob = add_membrane_fin(f["name"], f["outline"](ctx), f["thickness_root"],
                              f["thickness_edge"], rays=f.get("rays", 0),
                              ray_depth=f.get("ray_depth", 0.0), steps=f.get("steps", 6))
        _place(ob, f.get("rotate"), f.get("translate"))
        parts.append(ob)
    join_fins(body, parts)
    unwrap(body)
    build_nodes = spec["build_nodes"]
    baked = bake_maps(body, "M_" + name,
                      lambda nt, bsdf, bctx: build_nodes(nt, bsdf, dict(bctx, **ctx)),
                      name, export_dir, size=spec.get("texture_size", 2048))
    rig = spec["rig"]
    arm = build_rig(name + "Rig", L, pec_root_y, pec_z=rig["pec_z"], spine=spine,
                    tail_tip_x=rig.get("tail_tip_x"), pec_span_y=rig["pec_span_y"],
                    pec_drop_z=rig["pec_drop_z"])
    skin(body, arm)
    unweighted, root_max_w = assert_rig_contract(body, arm, spine)
    x_min, x_max = spec.get("pec_window", (-0.05, 0.45))
    pec_stray = pec_stray_count(body, L, x_min, x_max)
    save_and_export(body, arm, os.path.join(spec["root_dir"], name + ".blend"),
                    os.path.join(export_dir, name + ".fbx"))
    preview = spec.get("preview_name", "preview_%s.png" % name.lower())
    render_preview(os.path.join(export_dir, preview), spec["cam_loc"],
                   spec.get("cam_rot", (1.35, 0, 0.42)),
                   fill_energy=spec.get("preview_fill", 0.0))
    return dict(verts=len(body.data.vertices), bones=len(arm.data.bones),
                unweighted=unweighted, root_max_w=root_max_w, pec_stray=pec_stray,
                maps={k: bpy.path.abspath(v.filepath_raw) for k, v in baked["images"].items()})


def sail_outline(c, t0, t1, height, sign=1.0, root_frac=0.5, power=0.6, peak=0.5, n=36):
    """Dorsal (sign=+1) or anal (sign=-1) membrane outline in body XZ space, for
    add_membrane_fin. The rim follows the body surface between t0 and t1 pushed `height` cm
    away from it, swelling like sin(pi*u)**power with its peak moved to `peak`. The root is a
    single point on the mid-base sunk to root_frac of the local body surface, so the two base
    segments of the fan lie inside the hull."""
    surf = c["top"] if sign > 0 else c["bottom"]
    x_of_t = c["x_of_t"]
    rim = []
    for i in range(n):
        u = i / (n - 1.0)
        t = t0 + (t1 - t0) * u
        # skew the swell toward `peak` so a dorsal can crest forward or aft
        v = u * 0.5 / peak if u <= peak else 0.5 + (u - peak) * 0.5 / (1.0 - peak)
        rim.append((x_of_t(t), surf(t) + sign * height * math.sin(math.pi * v) ** power))
    tm = (t0 + t1) / 2.0
    return [(x_of_t(tm), surf(tm) * root_frac)] + rim


def caudal_outline(c, reach, spread, notch, root_t=0.93, n=36):
    """Caudal fin outline: a crescent reaching `reach` cm behind the tail tip with lobes
    `spread` cm from the axis and a fork cutting `notch` cm back in. Root sits inside the
    peduncle at root_t so the fin welds into the body."""
    L = c["L"]
    ped = max(0.4, c["top"](1.0))
    keys = [(-L, ped), (-L - reach * 0.75, spread * 0.85), (-L - reach, spread),
            (-L - reach + notch, 0.0),
            (-L - reach, -spread), (-L - reach * 0.75, -spread * 0.85), (-L, -ped)]
    return [(c["x_of_t"](root_t), 0.0)] + polyline(keys, n)


def fan_outline(c, t_root, length, height, n=30, drop=0.0):
    """Pectoral-style rounded fan built in the XZ plane (rotate it into place afterwards):
    root at the body surface at t_root, rim an ellipse trailing `length` cm aft and `height`
    cm across, optionally tilted down by `drop` cm at the far end."""
    x0 = c["x_of_t"](t_root)
    cx, a, b = x0 - length * 0.45, length * 0.55, height * 0.5
    rim = []
    for i in range(n):
        ang = math.radians(15.0 + 330.0 * i / (n - 1.0))
        u = (1.0 - math.cos(ang)) * 0.5          # 0 at the root end, 1 at the far end
        rim.append((cx + a * math.cos(ang), b * math.sin(ang) - drop * u))
    return [(x0, 0.0)] + rim


def paint_eye(nt, base, centre, radius, sclera=(0.9, 0.9, 0.88, 1), iris=(0.12, 0.08, 0.04, 1),
              pupil=(0.01, 0.01, 0.01, 1)):
    """Lay sclera/iris/pupil rings over `base` using world-position radial masks instead of UVs.
    The eyeball is joined into the body and its smart-projected UV island is unpredictable, so
    the eye is painted from position; the XZ mask covers both flanks at once. Returns a socket."""
    col = mix_over(nt, base, radial_mask(nt, centre, radius, softness=0.15), sclera)
    col = mix_over(nt, col, radial_mask(nt, centre, radius * 0.72, softness=0.12), iris)
    col = mix_over(nt, col, radial_mask(nt, centre, radius * 0.34, softness=0.15), pupil)
    return col


def scale_pattern_world(nt, cell_size, sharpness=1.0):
    """scale_pattern driven by world position, so `cell_size` really is centimetres.

    A Voronoi node with an unconnected Vector input samples Generated coordinates -- 0..1 across
    the object's bounding box -- where a scale of 1/cell_size resolves to two or three cells over
    the whole fish and the bake reads as a few big polygonal panels. Feeding Position instead makes
    the density body-size independent: a body h cm tall shows about h/cell_size rows of scales.
    Returns (height_socket, roughness_socket) exactly like scale_pattern."""
    before = {n for n in nt.nodes if n.type == 'TEX_VORONOI'}
    height, rough = scale_pattern(nt, cell_size, sharpness)
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    for n in nt.nodes:
        if n.type == 'TEX_VORONOI' and n not in before:
            nt.links.new(geo.outputs["Position"], n.inputs["Vector"])
    return height, rough
