# Imports a fish species' FBX + base colour / normal / roughness textures into
# /Game/Fish/<name> and builds the PBR material M_<name> (Subsurface shading).
# Idempotent: re-running replaces the existing assets.
#
# Run headless (defaults to BlueTang + Clownfish + YellowTang + Butterflyfish + Damselfish):
#   UnrealEditor-Cmd Aquarium.uproject -run=pythonscript -script=Scripts/import_fish.py
#
# Select species explicitly, either via a script argument:
#   ... -script="Scripts/import_fish.py species=Clownfish"
# or, if -run=pythonscript does not forward script args into sys.argv, via an
# environment variable:
#   AQUARIUM_SPECIES=Clownfish UnrealEditor-Cmd ...
import os
import unreal

ROOT = os.path.abspath(os.path.join(unreal.Paths.project_dir(), "..", ".."))

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def import_asset(dest, filename, options=None, destination_name=None):
    task = unreal.AssetImportTask()
    task.filename = filename
    task.destination_path = dest
    if destination_name:
        task.destination_name = destination_name
    task.automated = True
    task.replace_existing = True
    task.save = True
    if options:
        task.options = options
    asset_tools.import_asset_tasks([task])
    return [str(p) for p in task.imported_object_paths]


def bone_names(mesh):
    """Walk the reference skeleton via SkeletalMesh.get_bone_parent/children."""
    root = "Root"
    # SkeletalMesh exposes no direct bone-existence query to Python. A missing
    # bone name returns an empty list from get_bone_children (indistinguishable
    # from a real childless leaf) but returns python None from get_bone_parent,
    # whereas an existing bone returns its parent's FName (as a string) -
    # "Root" always has a parent (the rig root) by contract, so this
    # distinguishes a missing/mistyped start bone and fails loudly instead of
    # silently walking a bogus one-bone hierarchy.
    assert mesh.get_bone_parent(root) is not None, \
        "start bone 'Root' not found on skeleton %s" % mesh.get_path_name()
    # Walk up in case "Root" is not the true root of the hierarchy.
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


def import_texture(dest, name, kind):
    """Import T_<name>_<kind>.png. Normal/Roughness are linear data, not colour."""
    png = os.path.join(ROOT, "assets", "blender", "export",
                       "T_%s_%s.png" % (name, kind))
    assert os.path.isfile(png), "missing PNG: %s" % png
    tex_name = "T_%s_%s" % (name, kind)
    paths = import_asset(dest, png, destination_name=tex_name)
    assert paths, "texture import failed: %s" % png
    tex = unreal.load_asset(paths[0])
    assert isinstance(tex, unreal.Texture2D), "not a Texture2D: %s" % paths
    if kind == "BaseColor":
        tex.set_editor_property("srgb", True)
    elif kind == "Normal":
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("compression_settings",
                                unreal.TextureCompressionSettings.TC_NORMALMAP)
    else:  # Roughness and any other linear mask
        tex.set_editor_property("srgb", False)
    eal.save_loaded_asset(tex)
    return tex


def import_species(name):
    fbx = os.path.join(ROOT, "assets", "blender", "export", "%s.fbx" % name)
    dest = "/Game/Fish/%s" % name

    assert os.path.isfile(fbx), "missing FBX: %s" % fbx

    # --- textures ------------------------------------------------------------
    maps = ["BaseColor", "Normal", "Roughness"]
    tex_by_kind = dict((k, import_texture(dest, name, k)) for k in maps)
    tex = tex_by_kind["BaseColor"]

    # --- skeletal mesh ---------------------------------------------------------
    opt = unreal.FbxImportUI()
    opt.import_mesh = True
    opt.import_as_skeletal = True
    opt.import_animations = False
    opt.import_materials = False
    opt.import_textures = False
    # UE 5.8 routes FbxImportUI through the Interchange override pipeline, which
    # ignores create_physics_asset entirely; the physics asset is built
    # explicitly below via SkeletalMeshEditorSubsystem instead.
    opt.create_physics_asset = False
    opt.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    skd = opt.skeletal_mesh_import_data
    skd.set_editor_property("import_morph_targets", False)
    skd.set_editor_property("convert_scene", True)
    skd.set_editor_property("import_uniform_scale", 1.0)
    skd.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)

    sk_name = "SK_%s" % name
    sk_paths = import_asset(dest, fbx, opt, destination_name=sk_name)
    sk = None
    for p in sk_paths:
        a = unreal.load_asset(p)
        if isinstance(a, unreal.SkeletalMesh):
            sk = a
    assert sk is not None, "no SkeletalMesh imported: %s" % sk_paths

    # --- material: texture -> base color ---------------------------------------
    mat_name = "M_%s" % name
    mat_path = dest + "/" + mat_name
    if eal.does_asset_exist(mat_path):
        mat = unreal.load_asset(mat_path)
        assert isinstance(mat, unreal.Material), "not a Material: %s" % mat_path
        unreal.MaterialEditingLibrary.delete_all_material_expressions(mat)
    else:
        mat = asset_tools.create_asset(mat_name, dest, unreal.Material, unreal.MaterialFactoryNew())
    mel = unreal.MaterialEditingLibrary

    # Subsurface shading gives the thin fins and body their translucent look;
    # the subsurface colour is the base colour dimmed, opacity is the
    # scattering weight (low = mostly opaque flesh with a soft bleed).
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SUBSURFACE)

    base = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -600, 0)
    base.set_editor_property("texture", tex)
    mel.connect_material_property(base, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)

    nrm = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -600, 300)
    nrm.set_editor_property("texture", tex_by_kind["Normal"])
    nrm.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    mel.connect_material_property(nrm, "RGB", unreal.MaterialProperty.MP_NORMAL)

    rgh = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -600, 600)
    rgh.set_editor_property("texture", tex_by_kind["Roughness"])
    rgh.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
    mel.connect_material_property(rgh, "R", unreal.MaterialProperty.MP_ROUGHNESS)

    ss_scale = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -600, 900)
    ss_scale.set_editor_property("r", 0.6)
    ss_mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 900)
    mel.connect_material_expressions(base, "RGB", ss_mul, "A")
    mel.connect_material_expressions(ss_scale, "", ss_mul, "B")
    mel.connect_material_property(ss_mul, "", unreal.MaterialProperty.MP_SUBSURFACE_COLOR)

    opac = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -600, 1100)
    opac.set_editor_property("r", 0.15)
    mel.connect_material_property(opac, "", unreal.MaterialProperty.MP_OPACITY)

    spec = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -600, 1300)
    spec.set_editor_property("r", 0.4)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)

    # Required so the material is actually used on the skeletal mesh in -game
    # (otherwise the engine falls back to the default material at runtime).
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
    phys_name = "SK_%s_PhysicsAsset" % name
    phys_path = dest + "/" + phys_name
    if sk.physics_asset is None:
        if eal.does_asset_exist(phys_path):
            phys = unreal.load_asset(phys_path)
        else:
            sms = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
            phys = sms.create_physics_asset(sk)
            assert phys is not None, "create_physics_asset failed"
            if phys.get_name() != phys_name:
                eal.rename_loaded_asset(phys, phys_path)
            eal.save_loaded_asset(phys)
        sk.set_editor_property("physics_asset", phys)
        eal.save_loaded_asset(sk)

    # --- diagnostics -------------------------------------------------------------
    bones = bone_names(sk)
    ext = sk.get_bounds().box_extent
    slots = [(str(m.get_editor_property("material_slot_name")),
              m.get_editor_property("material_interface").get_path_name()
              if m.get_editor_property("material_interface") else None)
             for m in sk.get_editor_property("materials")]
    print("IMPORT_OK species=%s maps=[%s] mesh=%s bones=%s extent=(%.2f,%.2f,%.2f) slots=%s skeleton=%s physics=%s" % (
        name, ",".join(maps), sk.get_path_name(), bones, ext.x, ext.y, ext.z, slots,
        sk.skeleton.get_path_name() if sk.skeleton else None,
        sk.physics_asset.get_path_name() if sk.physics_asset else None))


import sys
names = [a.split("=", 1)[1] for a in sys.argv if a.startswith("species=")]
if not names:
    env_species = os.environ.get("AQUARIUM_SPECIES")
    names = [env_species] if env_species else ["BlueTang", "Clownfish", "YellowTang", "Butterflyfish", "Damselfish"]
for n in names:
    import_species(n)
