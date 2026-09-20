# Imports the blue tang FBX + base color texture into /Game/Fish/BlueTang and
# builds M_BlueTang. Idempotent: re-running replaces the existing assets.
#
# Run headless:
#   UnrealEditor-Cmd Aquarium.uproject -run=pythonscript -script=Scripts/import_bluetang.py
import os
import unreal

ROOT = os.path.abspath(os.path.join(unreal.Paths.project_dir(), "..", ".."))
FBX = os.path.join(ROOT, "assets", "blender", "export", "BlueTang.fbx")
PNG = os.path.join(ROOT, "assets", "blender", "export", "T_BlueTang_BaseColor.png")
DEST = "/Game/Fish/BlueTang"

assert os.path.isfile(FBX), "missing FBX: %s" % FBX
assert os.path.isfile(PNG), "missing PNG: %s" % PNG

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def import_asset(filename, options=None, destination_name=None):
    task = unreal.AssetImportTask()
    task.filename = filename
    task.destination_path = DEST
    if destination_name:
        task.destination_name = destination_name
    task.automated = True
    task.replace_existing = True
    task.save = True
    if options:
        task.options = options
    asset_tools.import_asset_tasks([task])
    return [str(p) for p in task.imported_object_paths]


# --- texture -----------------------------------------------------------------
tex_paths = import_asset(PNG, destination_name="T_BlueTang_BaseColor")
assert tex_paths, "texture import failed"
tex = unreal.load_asset(tex_paths[0])
assert isinstance(tex, unreal.Texture2D), "not a Texture2D: %s" % tex_paths
tex.set_editor_property("srgb", True)
eal.save_loaded_asset(tex)

# --- skeletal mesh -----------------------------------------------------------
opt = unreal.FbxImportUI()
opt.import_mesh = True
opt.import_as_skeletal = True
opt.import_animations = False
opt.import_materials = False
opt.import_textures = False
opt.create_physics_asset = True
opt.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
skd = opt.skeletal_mesh_import_data
skd.set_editor_property("import_morph_targets", False)
skd.set_editor_property("convert_scene", True)
skd.set_editor_property("import_uniform_scale", 1.0)
skd.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)

sk_paths = import_asset(FBX, opt, destination_name="SK_BlueTang")
sk = None
for p in sk_paths:
    a = unreal.load_asset(p)
    if isinstance(a, unreal.SkeletalMesh):
        sk = a
assert sk is not None, "no SkeletalMesh imported: %s" % sk_paths

# --- material: texture -> base color -----------------------------------------
mat_path = DEST + "/M_BlueTang"
if eal.does_asset_exist(mat_path):
    mat = unreal.load_asset(mat_path)
    unreal.MaterialEditingLibrary.delete_all_material_expressions(mat)
else:
    mat = asset_tools.create_asset("M_BlueTang", DEST, unreal.Material, unreal.MaterialFactoryNew())
mel = unreal.MaterialEditingLibrary
node = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -400, 0)
node.set_editor_property("texture", tex)
mel.connect_material_property(node, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 300)
rough.set_editor_property("r", 0.35)
mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
# Required so the material is actually used on SK_BlueTang in -game (otherwise
# the engine falls back to the default material at runtime).
mat.set_editor_property("used_with_skeletal_mesh", True)
mel.recompile_material(mat)
eal.save_loaded_asset(mat)

mats = list(sk.get_editor_property("materials"))
assert mats, "skeletal mesh has no material slots"
for m in mats:
    m.set_editor_property("material_interface", mat)
sk.set_editor_property("materials", mats)
eal.save_loaded_asset(sk)

# --- physics asset -----------------------------------------------------------
# UE 5.8 routes FbxImportUI through the Interchange override pipeline, which
# ignores create_physics_asset, so build it explicitly (once).
phys_path = DEST + "/SK_BlueTang_PhysicsAsset"
if sk.physics_asset is None:
    if eal.does_asset_exist(phys_path):
        phys = unreal.load_asset(phys_path)
    else:
        sms = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
        phys = sms.create_physics_asset(sk)
        assert phys is not None, "create_physics_asset failed"
        if phys.get_name() != "SK_BlueTang_PhysicsAsset":
            eal.rename_loaded_asset(phys, phys_path)
        eal.save_loaded_asset(phys)
    sk.set_editor_property("physics_asset", phys)
    eal.save_loaded_asset(sk)

# --- diagnostics -------------------------------------------------------------
def bone_names(mesh):
    """Walk the reference skeleton via SkeletalMesh.get_bone_parent/children."""
    root = "Root"
    while True:
        parent = str(mesh.get_bone_parent(root))
        if parent in ("None", ""):
            break
        root = parent
    out, stack = [], [root]
    while stack:
        b = stack.pop(0)
        out.append(b)
        stack = [str(c) for c in mesh.get_bone_children(b)] + stack
    return out

bones = bone_names(sk)
ext = sk.get_bounds().box_extent
slots = [(str(m.get_editor_property("material_slot_name")),
          m.get_editor_property("material_interface").get_path_name()
          if m.get_editor_property("material_interface") else None)
         for m in sk.get_editor_property("materials")]
print("IMPORT_OK mesh=%s bones=%s extent=(%.2f,%.2f,%.2f) slots=%s skeleton=%s physics=%s" % (
    sk.get_path_name(), bones, ext.x, ext.y, ext.z, slots,
    sk.skeleton.get_path_name() if sk.skeleton else None,
    sk.physics_asset.get_path_name() if sk.physics_asset else None))
