# Imports the coral props (built by assets/blender/make_corals.py) and the
# Poly Haven CC0 rocks into /Game/Props as static meshes, with one material
# each. Corals re-import in place safely (no _1 duplicates). Rocks are instead
# re-created each run (the previous asset is deleted, then the new LOD0 is
# renamed onto the target path), which breaks any level (e.g. /Game/Maps/ReefM1)
# that already references the old rock assets. So when regenerating from
# scratch, run this script BEFORE build_reef_m1.py — matching the order in
# docs/SETUP.md.
#
# Run headless:
#   UnrealEditor-Cmd Aquarium.uproject -run=pythonscript -script=Scripts/import_props.py
#
# Judge success by the single PROPS_OK line printed at the end.
import os
import unreal

ROOT = os.path.abspath(os.path.join(unreal.Paths.project_dir(), "..", ".."))
DEST = "/Game/Props"

# StaticMeshEditorSubsystem lives in the StaticMeshEditor module, which a
# -run=pythonscript -nullrhi commandlet does not load on its own; without this
# get_editor_subsystem() returns None (and the deprecated
# EditorStaticMeshLibrary.set_lod_from_static_mesh silently returns -1).
unreal.load_module("StaticMeshEditor")

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

# Corals come out of Blender in centimetres already. The Poly Haven rocks are
# authored in metres, but UE 5.8's Interchange FBX pipeline honours the file's
# unit metadata and converts to centimetres on its own, so an extra uniform
# scale of 100 would make them 100x too large (verified against the imported
# bounds: a uniform scale of 1.0 gives boulder_01 half-extents 63.6/91.5/50.2 cm
# for a 1.3 x 1.8 x 1.0 m rock).
CORALS = ["BranchCoral", "PlateCoral", "BrainCoral"]
ROCKS = ["boulder_01", "rock_07", "rock_09"]
ROCK_SCALE = 1.0

# Scratch folder the rock LOD meshes are imported into before they are folded
# into a single static mesh; deleted at the end of each rock.
TMP = "/Game/Props/_LODImport"


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


def import_texture(png, name, srgb, compression=None):
    assert os.path.isfile(png), "missing texture: %s" % png
    paths = import_asset(DEST, png, destination_name=name)
    assert paths, "texture import failed: %s" % png
    tex = unreal.load_asset(paths[0])
    assert isinstance(tex, unreal.Texture2D), "not a Texture2D: %s" % paths
    if compression is not None:
        tex.set_editor_property("compression_settings", compression)
    tex.set_editor_property("srgb", srgb)
    eal.save_loaded_asset(tex)
    return tex


def static_mesh_options(uniform_scale, combine_meshes=True):
    opt = unreal.FbxImportUI()
    opt.import_mesh = True
    opt.import_as_skeletal = False
    opt.import_animations = False
    # The source materials/textures are wired up explicitly below, so the
    # importer must not create its own (they would land beside the mesh).
    opt.import_materials = False
    opt.import_textures = False
    opt.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    smd = opt.static_mesh_import_data
    smd.set_editor_property("combine_meshes", combine_meshes)
    # The Poly Haven FBXs name their LOD meshes "<id>_LOD0".."<id>_LOD3" rather
    # than nesting them under Unreal's "LOD_" group convention, so
    # import_mesh_lods does not build a LOD chain from them - with
    # combine_meshes the four meshes are merged into one 4x-dense mesh instead.
    # Rocks are therefore imported uncombined and the chain is assembled
    # explicitly in import_rock() via set_lod_from_static_mesh.
    smd.set_editor_property("import_mesh_lods", True)
    smd.set_editor_property("convert_scene", True)
    smd.set_editor_property("import_uniform_scale", uniform_scale)
    smd.set_editor_property("normal_import_method",
                            unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)
    smd.set_editor_property("generate_lightmap_u_vs", True)
    return opt


def import_static_mesh(fbx, name, uniform_scale):
    assert os.path.isfile(fbx), "missing FBX: %s" % fbx
    paths = import_asset(DEST, fbx, static_mesh_options(uniform_scale), destination_name=name)
    sm = None
    for p in paths:
        a = unreal.load_asset(p)
        if isinstance(a, unreal.StaticMesh):
            sm = a
    if sm is None:
        # Interchange may report no imported object path on a replace; fall back
        # to loading the asset at the expected path.
        expected = DEST + "/" + name
        if eal.does_asset_exist(expected):
            a = unreal.load_asset(expected)
            if isinstance(a, unreal.StaticMesh):
                sm = a
    assert sm is not None, "no StaticMesh imported from %s (paths=%s)" % (fbx, paths)
    return sm


def import_rock_mesh(fbx, rid):
    """Import the four LOD meshes, then fold them into one SM_<rid> LOD chain."""
    assert os.path.isfile(fbx), "missing FBX: %s" % fbx
    if eal.does_directory_exist(TMP):
        eal.delete_directory(TMP)
    paths = import_asset(TMP, fbx, static_mesh_options(ROCK_SCALE, combine_meshes=False))
    lods = {}
    for p in paths:
        a = unreal.load_asset(p)
        if not isinstance(a, unreal.StaticMesh):
            continue
        n = a.get_name()
        if "_LOD" in n:
            lods[int(n.rsplit("_LOD", 1)[1])] = a
    assert 0 in lods, "no LOD0 StaticMesh imported from %s (paths=%s)" % (fbx, paths)

    target = DEST + "/SM_%s" % rid
    # Rebuild in place: drop the previous asset so the rename below lands on the
    # exact same path instead of producing an SM_<rid>_1 duplicate.
    if eal.does_asset_exist(target):
        eal.delete_asset(target)
    ok = eal.rename_asset(lods[0].get_path_name().split(".")[0], target)
    assert ok, "failed to rename LOD0 to %s" % target
    sm = unreal.load_asset(target)
    assert isinstance(sm, unreal.StaticMesh), "not a StaticMesh: %s" % target

    smes = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    assert smes is not None, "StaticMeshEditorSubsystem unavailable"
    for i in sorted(k for k in lods if k > 0):
        idx = smes.set_lod_from_static_mesh(sm, i, lods[i], 0, True)
        assert idx >= 0, "set_lod_from_static_mesh failed for LOD%d of %s" % (i, rid)
    eal.save_loaded_asset(sm)
    eal.delete_directory(TMP)
    return sm


def get_or_create_material(name):
    path = DEST + "/" + name
    if eal.does_asset_exist(path):
        mat = unreal.load_asset(path)
        assert isinstance(mat, unreal.Material), "not a Material: %s" % path
        mel.delete_all_material_expressions(mat)
    else:
        mat = asset_tools.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    assert isinstance(mat, unreal.Material), "material creation failed: %s" % path
    return mat


def assign_material(sm, mat):
    slots = list(sm.get_editor_property("static_materials"))
    assert slots, "static mesh has no material slots: %s" % sm.get_path_name()
    for s in slots:
        s.set_editor_property("material_interface", mat)
    sm.set_editor_property("static_materials", slots)
    eal.save_loaded_asset(sm)


def tri_count(sm):
    # LOD 0 only; that is the count that matters for scattered props.
    return sm.get_num_triangles(0)


def diag(sm):
    ext = sm.get_bounds().box_extent
    return (sm.get_name(), "extent=(%.1f,%.1f,%.1f)" % (ext.x, ext.y, ext.z),
            "tris=%d" % tri_count(sm), "lods=%d" % sm.get_num_lods())


def import_coral(name):
    src = os.path.join(ROOT, "assets", "blender", "export")
    tex = import_texture(os.path.join(src, "T_%s_BaseColor.png" % name),
                         "T_%s_BaseColor" % name, srgb=True)
    sm = import_static_mesh(os.path.join(src, "%s.fbx" % name), "SM_%s" % name, 1.0)

    mat = get_or_create_material("M_%s" % name)
    base = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -400, 0)
    base.set_editor_property("texture", tex)
    mel.connect_material_property(base, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 300)
    rough.set_editor_property("r", 0.6)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)

    assign_material(sm, mat)
    return [sm], [mat], [tex]


def import_rock(rid):
    src = os.path.join(ROOT, "assets", "models", "rocks", rid)
    tex_dir = os.path.join(src, "textures")

    diff = import_texture(os.path.join(tex_dir, "%s_diff_1k.jpg" % rid),
                          "T_%s_D" % rid, srgb=True)
    nor = import_texture(os.path.join(tex_dir, "%s_nor_gl_1k.jpg" % rid),
                         "T_%s_N" % rid, srgb=False,
                         compression=unreal.TextureCompressionSettings.TC_NORMALMAP)
    rgh = import_texture(os.path.join(tex_dir, "%s_rough_1k.jpg" % rid),
                         "T_%s_R" % rid, srgb=False)
    ao = import_texture(os.path.join(tex_dir, "%s_ao_1k.jpg" % rid),
                        "T_%s_AO" % rid, srgb=False)

    sm = import_rock_mesh(os.path.join(src, "%s_1k.fbx" % rid), rid)

    mat = get_or_create_material("M_%s" % rid)
    n_diff = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -600, -200)
    n_diff.set_editor_property("texture", diff)
    n_ao = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -600, 100)
    n_ao.set_editor_property("texture", ao)
    mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, -100)
    mel.connect_material_expressions(n_diff, "RGB", mul, "A")
    mel.connect_material_expressions(n_ao, "RGB", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)

    n_nor = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -600, 400)
    n_nor.set_editor_property("texture", nor)
    n_nor.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    mel.connect_material_property(n_nor, "RGB", unreal.MaterialProperty.MP_NORMAL)

    n_rgh = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -600, 700)
    n_rgh.set_editor_property("texture", rgh)
    n_rgh.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
    mel.connect_material_property(n_rgh, "R", unreal.MaterialProperty.MP_ROUGHNESS)

    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)

    assign_material(sm, mat)
    return [sm], [mat], [diff, nor, rgh, ao]


meshes, materials, textures = [], [], []
for n in CORALS:
    m, mt, tx = import_coral(n)
    meshes += m
    materials += mt
    textures += tx
for r in ROCKS:
    m, mt, tx = import_rock(r)
    meshes += m
    materials += mt
    textures += tx

print("PROPS_OK count=%d meshes=%d materials=%d textures=%d assets=[%s]" % (
    len(meshes) + len(materials) + len(textures), len(meshes), len(materials), len(textures),
    ", ".join(str(diag(sm)) for sm in meshes)))
print("PROPS_OK materials=[%s]" % ", ".join(m.get_path_name() for m in materials))
print("PROPS_OK textures=[%s]" % ", ".join(t.get_path_name() for t in textures))
