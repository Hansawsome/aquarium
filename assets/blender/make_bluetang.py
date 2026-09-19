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
import bpy, bmesh, math, os

ROOT = os.path.dirname(os.path.abspath(__file__))
EXPORT = os.path.join(ROOT, "export")
os.makedirs(EXPORT, exist_ok=True)

# ---------- clean scene ----------
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 0.01   # 1 unit = 1 cm, matches Unreal

BODY_LEN = 25.0   # cm
BODY_H = 12.0
BODY_W = 3.0
L = BODY_LEN / 2

# ---------- body ----------
bpy.ops.mesh.primitive_uv_sphere_add(segments=48, ring_count=24, radius=1.0)
body = bpy.context.object
body.name = "BlueTang"
body.data.name = "BlueTang"
body.scale = (L, BODY_W / 2, BODY_H / 2)
bpy.ops.object.transform_apply(scale=True)
# taper the tail: pull vertices with x<0 toward the axis
bm = bmesh.new(); bm.from_mesh(body.data)
for v in bm.verts:
    t = max(0.0, -v.co.x / L)     # 0 at center, 1 at tail tip
    v.co.z *= 1.0 - 0.55 * t * t
    v.co.y *= 1.0 - 0.4 * t
bm.to_mesh(body.data); bm.free()

# ---------- fins (flat plates joined to body) ----------
def add_fin(name, verts, faces):
    mesh = bpy.data.meshes.new(name); mesh.from_pydata(verts, [], faces); mesh.update()
    ob = bpy.data.objects.new(name, mesh); bpy.context.collection.objects.link(ob)
    return ob

def body_h(x):
    """Half-height of the (tapered) body at x, used to sink fin roots into the hull."""
    t = max(0.0, -x / L)
    return (BODY_H / 2) * math.sqrt(max(0.0, 1.0 - (x / L) ** 2)) * (1.0 - 0.55 * t * t)

tail = add_fin("TailFin",
    [(-L + 1, 0, 2.5), (-L - 7, 0, 5.5), (-L - 6, 0, 0), (-L - 7, 0, -5.5), (-L + 1, 0, -2.5)],
    [(0, 1, 2), (0, 2, 4), (2, 3, 4)])
# fin roots sit at 70% of the local body half-height so the plates overlap the hull
dorsal = add_fin("DorsalFin",
    [(L * 0.55, 0, body_h(L * 0.55) * 0.7), (0, 0, body_h(0) * 0.7), (-L * 0.6, 0, body_h(-L * 0.6) * 0.7),
     (-L * 0.6, 0, BODY_H * 0.62), (0, 0, BODY_H * 0.85), (L * 0.5, 0, BODY_H * 0.6)],
    [(0, 1, 4, 5), (1, 2, 3, 4)])
anal = add_fin("AnalFin",
    [(L * 0.1, 0, -body_h(L * 0.1) * 0.7), (-L * 0.6, 0, -body_h(-L * 0.6) * 0.7),
     (-L * 0.6, 0, -BODY_H * 0.6), (L * 0.05, 0, -BODY_H * 0.75)],
    [(0, 1, 2, 3)])
pecL = add_fin("PecFinL",
    [(L * 0.35, BODY_W * 0.45, 0.5), (L * 0.1, BODY_W * 0.45 + 4.5, -1.5),
     (L * 0.0, BODY_W * 0.45 + 3.5, -2.5), (L * 0.25, BODY_W * 0.45, -1.5)],
    [(0, 1, 2, 3)])
pecR = add_fin("PecFinR",
    [(L * 0.35, -BODY_W * 0.45, 0.5), (L * 0.25, -BODY_W * 0.45, -1.5),
     (L * 0.0, -BODY_W * 0.45 - 3.5, -2.5), (L * 0.1, -BODY_W * 0.45 - 4.5, -1.5)],
    [(0, 1, 2, 3)])

fins = [tail, dorsal, anal, pecL, pecR]
for f in fins:
    sol = f.modifiers.new("Solidify", 'SOLIDIFY'); sol.thickness = 0.25; sol.offset = 0
bpy.ops.object.select_all(action='DESELECT')
for f in fins: f.select_set(True)
body.select_set(True); bpy.context.view_layer.objects.active = body
bpy.ops.object.convert(target='MESH')      # apply solidify on fins
bpy.ops.object.join()                      # everything into BlueTang
bpy.ops.object.shade_smooth()

# ---------- UV ----------
bpy.ops.object.select_all(action='DESELECT'); body.select_set(True)
bpy.context.view_layer.objects.active = body
bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.02)
bpy.ops.object.mode_set(mode='OBJECT')

# ---------- procedural material -> bake ----------
mat = bpy.data.materials.new("M_BlueTang")
if mat.node_tree is None:   # Blender < 6.0 still needs use_nodes for a node tree
    mat.use_nodes = True
nt = mat.node_tree; nt.nodes.clear()
out = nt.nodes.new("ShaderNodeOutputMaterial")
bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled"); bsdf.inputs["Roughness"].default_value = 0.35
geo = nt.nodes.new("ShaderNodeNewGeometry")
sep = nt.nodes.new("ShaderNodeSeparateXYZ")
nt.links.new(geo.outputs["Position"], sep.inputs[0])
# tail mask: x < -L*0.75  -> yellow
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
nt.links.new(bsdf.outputs[0], out.inputs[0])
body.data.materials.append(mat)

img = bpy.data.images.new("T_BlueTang_BaseColor", 2048, 2048)
tex_node = nt.nodes.new("ShaderNodeTexImage"); tex_node.image = img
for n in nt.nodes: n.select = False
tex_node.select = True
nt.nodes.active = tex_node
scene.render.engine = 'CYCLES'; scene.cycles.device = 'CPU'; scene.cycles.samples = 16
scene.render.bake.use_pass_direct = False; scene.render.bake.use_pass_indirect = False
scene.render.bake.use_pass_color = True
bpy.ops.object.select_all(action='DESELECT'); body.select_set(True)
bpy.context.view_layer.objects.active = body
bpy.ops.object.bake(type='DIFFUSE', margin=8)
img.filepath_raw = os.path.join(EXPORT, "T_BlueTang_BaseColor.png"); img.file_format = 'PNG'; img.save()
# rewire baked texture as the exported base color
nt.links.new(tex_node.outputs["Color"], bsdf.inputs["Base Color"])

# ---------- armature ----------
bpy.ops.object.armature_add(enter_editmode=True, location=(0, 0, 0))
arm = bpy.context.object; arm.name = "BlueTangRig"; arm.data.name = "BlueTangRig"
ebones = arm.data.edit_bones
for b in list(ebones): ebones.remove(b)
root = ebones.new("Root"); root.head = (0, 0, 0); root.tail = (0, 0, 1)
SPINE = 6
x_head = L * 0.95; x_tail = -L * 0.9
step = (x_head - x_tail) / SPINE
prev = root
for i in range(SPINE):
    b = ebones.new(f"Spine{i}")
    b.head = (x_head - step * i, 0, 0); b.tail = (x_head - step * (i + 1), 0, 0)
    b.parent = prev; b.use_connect = (i > 0); prev = b
tailb = ebones.new("Tail"); tailb.head = prev.tail; tailb.tail = (-L - 6, 0, 0)
tailb.parent = prev; tailb.use_connect = True
for name, sign in (("PecL", 1), ("PecR", -1)):
    pb = ebones.new(name); pb.head = (L * 0.35, sign * BODY_W * 0.45, 0.5)
    pb.tail = (L * 0.05, sign * (BODY_W * 0.45 + 4.0), -2.0); pb.parent = ebones["Spine1"]
bpy.ops.object.mode_set(mode='OBJECT')

# ---------- skinning ----------
bpy.ops.object.select_all(action='DESELECT'); body.select_set(True); arm.select_set(True)
bpy.context.view_layer.objects.active = arm
bpy.ops.object.parent_set(type='ARMATURE_AUTO')

# ---------- save + export ----------
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ROOT, "BlueTang.blend"))
bpy.ops.object.select_all(action='DESELECT'); body.select_set(True); arm.select_set(True)
bpy.ops.export_scene.fbx(filepath=os.path.join(EXPORT, "BlueTang.fbx"), use_selection=True,
                         apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE',
                         axis_forward='X', axis_up='Z', add_leaf_bones=False,
                         bake_anim=False, mesh_smooth_type='FACE', path_mode='AUTO', embed_textures=False)

# ---------- preview render (after export so camera/light never enter the FBX) ----------
cam = bpy.data.objects.new("PreviewCam", bpy.data.cameras.new("PreviewCam"))
scene.collection.objects.link(cam)
cam.location = (20, -45, 10); cam.rotation_euler = (1.35, 0, 0.42)
scene.camera = cam
sun = bpy.data.objects.new("PreviewSun", bpy.data.lights.new("PreviewSun", 'SUN'))
scene.collection.objects.link(sun)
sun.rotation_euler = (0.8, 0.3, 0.5)
scene.render.engine = 'BLENDER_EEVEE'
scene.render.resolution_x = 1280; scene.render.resolution_y = 720
scene.render.filepath = os.path.join(EXPORT, "preview.png")
bpy.ops.render.render(write_still=True)

print("BLUETANG_OK verts=%d bones=%d" % (len(body.data.vertices), len(arm.data.bones)))
