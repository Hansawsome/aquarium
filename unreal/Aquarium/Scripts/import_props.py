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
CORALS = ["BranchCoral", "PlateCoral", "BrainCoral", "FanCoral", "TubeCoral"]
ROCKS = ["boulder_01", "rock_07", "rock_09"]
ROCK_SCALE = 1.0

# Per-instance colour variation: one MaterialInstanceConstant per tint, assigned
# to individual prop actors by build_reef_m1.py. Three tints per coral (the reef
# should not look stamped), two per rock (the rocks already vary by mesh and by
# AO).
CORAL_TINTS = [(1.00, 1.00, 1.00), (0.82, 0.90, 1.05), (1.10, 0.92, 0.86)]
ROCK_TINTS = [(1.00, 1.00, 1.00), (0.90, 0.95, 1.06)]

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


def add_tint(mat, source, source_output="", x=-150, y=-100):
    """Multiply `source`'s output by a 'Tint' vector parameter and return the multiply node, so
    the caller can connect it to Base Color. `source_output` is the output name on `source`
    ("RGB" for a TextureSample, "" for an arithmetic node) -- passed explicitly rather than
    sniffed from the node class, because a wrong guess here connects nothing and the tint simply
    never appears. The parameter is what the per-instance MaterialInstanceConstants below
    override; without it every copy of a mesh is the same colour and 22 props read as stamped."""
    tint = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, x, y + 200)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(r=1.0, g=1.0, b=1.0, a=1.0))
    mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, x + 150, y)
    mel.connect_material_expressions(source, source_output, mul, "A")
    mel.connect_material_expressions(tint, "RGB", mul, "B")
    return mul


def make_tint_instances(mat, name, tints):
    """Create (or reuse) MI_<name>_v0..vN as MaterialInstanceConstants of `mat`, each with its
    own Tint. Reused in place so re-running never leaves MI_*_1 duplicates behind. Returns the
    list of instances."""
    out = []
    for i, (r, g, b) in enumerate(tints):
        path = DEST + "/MI_%s_v%d" % (name, i)
        if eal.does_asset_exist(path):
            mi = unreal.load_asset(path)
            assert isinstance(mi, unreal.MaterialInstanceConstant), "not a MIC: %s" % path
        else:
            mi = asset_tools.create_asset("MI_%s_v%d" % (name, i), DEST,
                                          unreal.MaterialInstanceConstant,
                                          unreal.MaterialInstanceConstantFactoryNew())
        assert mi is not None, "material instance creation failed: %s" % path
        mel.set_material_instance_parent(mi, mat)
        mel.set_material_instance_vector_parameter_value(
            mi, "Tint", unreal.LinearColor(r=r, g=g, b=b, a=1.0))
        eal.save_loaded_asset(mi)
        out.append(mi)
    return out


def tri_count(sm):
    # LOD 0 only; that is the count that matters for scattered props.
    return sm.get_num_triangles(0)


def diag(sm):
    ext = sm.get_bounds().box_extent
    return (sm.get_name(), "extent=(%.1f,%.1f,%.1f)" % (ext.x, ext.y, ext.z),
            "tris=%d" % tri_count(sm), "lods=%d" % sm.get_num_lods())


def import_coral(name):
    src = os.path.join(ROOT, "assets", "blender", "export")
    base = import_texture(os.path.join(src, "T_%s_BaseColor.png" % name),
                          "T_%s_BaseColor" % name, srgb=True)
    nor = import_texture(os.path.join(src, "T_%s_Normal.png" % name),
                         "T_%s_Normal" % name, srgb=False,
                         compression=unreal.TextureCompressionSettings.TC_NORMALMAP)
    # TC_MASKS, not just srgb=False -- see the note in import_rock(); a roughness
    # map left on TC_Default fails the whole material compile and the coral
    # silently turns grey.
    rgh = import_texture(os.path.join(src, "T_%s_Roughness.png" % name),
                         "T_%s_Roughness" % name, srgb=False,
                         compression=unreal.TextureCompressionSettings.TC_MASKS)
    sm = import_static_mesh(os.path.join(src, "%s.fbx" % name), "SM_%s" % name, 1.0)

    mat = get_or_create_material("M_%s" % name)
    n_base = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -600, -200)
    n_base.set_editor_property("texture", base)
    mel.connect_material_property(add_tint(mat, n_base, "RGB"), "",
                                  unreal.MaterialProperty.MP_BASE_COLOR)

    n_nor = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -600, 200)
    n_nor.set_editor_property("texture", nor)
    n_nor.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    mel.connect_material_property(n_nor, "RGB", unreal.MaterialProperty.MP_NORMAL)

    n_rgh = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -600, 500)
    n_rgh.set_editor_property("texture", rgh)
    n_rgh.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    mel.connect_material_property(n_rgh, "R", unreal.MaterialProperty.MP_ROUGHNESS)

    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    instances = make_tint_instances(mat, name, CORAL_TINTS)

    assign_material(sm, instances[0])
    return [sm], [mat], [base, nor, rgh], instances


def import_rock(rid):
    src = os.path.join(ROOT, "assets", "models", "rocks", rid)
    tex_dir = os.path.join(src, "textures")

    diff = import_texture(os.path.join(tex_dir, "%s_diff_1k.jpg" % rid),
                          "T_%s_D" % rid, srgb=True)
    nor = import_texture(os.path.join(tex_dir, "%s_nor_gl_1k.jpg" % rid),
                         "T_%s_N" % rid, srgb=False,
                         compression=unreal.TextureCompressionSettings.TC_NORMALMAP)
    # TC_MASKS is required, not just srgb=False: a mask left on TC_Default is a
    # colour texture as far as the material compiler is concerned, and the
    # Linear Grayscale/Masks sampler below then fails the whole material
    # ("Sampler type is Linear Grayscale, should be Linear Color"), which
    # silently swaps the rock for the grey Default Material at runtime. Same
    # bug, same fix as the fish roughness maps in import_fish.py.
    rgh = import_texture(os.path.join(tex_dir, "%s_rough_1k.jpg" % rid),
                         "T_%s_R" % rid, srgb=False,
                         compression=unreal.TextureCompressionSettings.TC_MASKS)
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
    mel.connect_material_property(add_tint(mat, mul, ""), "", unreal.MaterialProperty.MP_BASE_COLOR)

    n_nor = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -600, 400)
    n_nor.set_editor_property("texture", nor)
    n_nor.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    mel.connect_material_property(n_nor, "RGB", unreal.MaterialProperty.MP_NORMAL)

    n_rgh = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -600, 700)
    n_rgh.set_editor_property("texture", rgh)
    n_rgh.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    mel.connect_material_property(n_rgh, "R", unreal.MaterialProperty.MP_ROUGHNESS)

    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    instances = make_tint_instances(mat, rid, ROCK_TINTS)

    assign_material(sm, instances[0])
    return [sm], [mat], [diff, nor, rgh, ao], instances


meshes, materials, textures, instances = [], [], [], []
for n in CORALS:
    m, mt, tx, mi = import_coral(n)
    meshes += m
    materials += mt
    textures += tx
    instances += mi
for r in ROCKS:
    m, mt, tx, mi = import_rock(r)
    meshes += m
    materials += mt
    textures += tx
    instances += mi

print("PROPS_OK count=%d meshes=%d materials=%d instances=%d textures=%d assets=[%s]" % (
    len(meshes) + len(materials) + len(instances) + len(textures), len(meshes),
    len(materials), len(instances), len(textures),
    ", ".join(str(diag(sm)) for sm in meshes)))
print("PROPS_OK materials=[%s]" % ", ".join(m.get_path_name() for m in materials))
print("PROPS_OK instances=[%s]" % ", ".join(m.get_path_name() for m in instances))
print("PROPS_OK textures=[%s]" % ", ".join(t.get_path_name() for t in textures))
