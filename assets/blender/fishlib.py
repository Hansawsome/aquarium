"""Shared Blender build steps for scripted fish.

Runs inside Blender 5.2 headless:
    /Applications/Blender.app/Contents/MacOS/Blender -b -P assets/blender/make_<species>.py

Conventions (must match the Unreal import side):
    +X = head, -X = tail, +Z = up, 1 unit = 1 cm.
    Bones: Root, Spine0..Spine{spine-1}, Tail, PecL, PecR (exactly these names).

Required call order (each step assumes the previous ones ran, object mode throughout):
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
"""
import bpy, bmesh, math, os


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


def render_preview(path, cam_loc, cam_rot, resolution=(1280, 720)):
    """EEVEE still to `path`. Adds PreviewCam/PreviewSun to the scene collection and switches the
    render engine to EEVEE. The FBX is already protected by use_selection; calling this last keeps
    the camera, light and EEVEE settings out of the saved .blend."""
    scene = bpy.context.scene
    cam = bpy.data.objects.new("PreviewCam", bpy.data.cameras.new("PreviewCam"))
    scene.collection.objects.link(cam)
    cam.location = cam_loc; cam.rotation_euler = cam_rot
    scene.camera = cam
    sun = bpy.data.objects.new("PreviewSun", bpy.data.lights.new("PreviewSun", 'SUN'))
    scene.collection.objects.link(sun)
    sun.rotation_euler = (0.8, 0.3, 0.5)
    scene.render.engine = 'BLENDER_EEVEE'
    scene.render.resolution_x, scene.render.resolution_y = resolution
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
