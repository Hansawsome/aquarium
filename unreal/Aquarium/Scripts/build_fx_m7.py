# M7이 런타임에 쓰는 머티리얼 두 개를 만든다. 레벨은 건드리지 않는다 --
# 기포도 먹이도 액터가 아니라 서브시스템이 런타임에 스폰하는 인스턴스라서,
# ReefM1.umap은 M7에서 한 바이트도 바뀌지 않는다(verify_scene.py 기대값 불변).
#
# Niagara는 쓰지 않는다: 에디터 파이썬으로 만들 수 없다(M4b에서 부딪힌 벽).
# 대신 M4b의 마린 스노와 같은 해석적 반투명 머티리얼을 쓴다.
#
# 판정은 마지막 FX_OK 줄로만 한다.
import unreal

DEST = "/Game/Fx"
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def material(name):
    path = DEST + "/" + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)          # 멱등: 매번 같은 그래프를 다시 만든다
    return tools.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())


def expr(mat, cls, x, y):
    return mel.create_material_expression(mat, cls, x, y)


def build_bubble(mat):
    """가장자리가 밝고 가운데가 비는 기포. Fresnel 하나면 구가 기포처럼 읽힌다."""
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", True)
    fres = expr(mat, unreal.MaterialExpressionFresnel, -600, 0)
    fres.set_editor_property("exponent", 2.6)
    fres.set_editor_property("base_reflect_fraction", 0.06)
    tint = expr(mat, unreal.MaterialExpressionConstant3Vector, -600, 220)
    tint.set_editor_property("constant", unreal.LinearColor(0.72, 0.88, 1.0, 1.0))
    emis = expr(mat, unreal.MaterialExpressionMultiply, -320, 120)
    mel.connect_material_expressions(tint, "", emis, "A")
    mel.connect_material_expressions(fres, "", emis, "B")
    gain = expr(mat, unreal.MaterialExpressionMultiply, -160, 120)
    gval = expr(mat, unreal.MaterialExpressionConstant, -320, 300)
    gval.set_editor_property("r", 1.8)
    mel.connect_material_expressions(emis, "", gain, "A")
    mel.connect_material_expressions(gval, "", gain, "B")
    mel.connect_material_property(gain, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    opac = expr(mat, unreal.MaterialExpressionMultiply, -160, 380)
    oval = expr(mat, unreal.MaterialExpressionConstant, -320, 460)
    oval.set_editor_property("r", 0.85)
    mel.connect_material_expressions(fres, "", opac, "A")
    mel.connect_material_expressions(oval, "", opac, "B")
    mel.connect_material_property(opac, "", unreal.MaterialProperty.MP_OPACITY)


def build_pellet(mat):
    """하얀 먹이 알갱이. 어두운 물속에서 또렷하게 보여야 하므로 언릿 발광이다."""
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    col = expr(mat, unreal.MaterialExpressionConstant3Vector, -320, 0)
    col.set_editor_property("constant", unreal.LinearColor(1.0, 0.97, 0.86, 1.0))
    gain = expr(mat, unreal.MaterialExpressionMultiply, -160, 0)
    gval = expr(mat, unreal.MaterialExpressionConstant, -320, 160)
    gval.set_editor_property("r", 2.4)
    mel.connect_material_expressions(col, "", gain, "A")
    mel.connect_material_expressions(gval, "", gain, "B")
    mel.connect_material_property(gain, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)


built = []
for name, build in (("M_Bubble", build_bubble), ("M_FoodPellet", build_pellet)):
    m = material(name)
    assert m is not None, "could not create %s" % name
    build(m)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    built.append(name)
    print("  built %s" % name)

# 런타임이 쓰는 엔진 기본 구 메시가 실재하는지 여기서 확인한다. 없으면 기포와
# 먹이가 '보이지 않는 인스턴스'가 되어 조용히 아무것도 안 그린다.
sphere = unreal.load_asset("/Engine/BasicShapes/Sphere.Sphere")
assert sphere is not None, "missing /Engine/BasicShapes/Sphere"
print("FX_OK count=%d materials=[%s] sphere=%s" % (len(built), " ".join(built), sphere.get_path_name()))
