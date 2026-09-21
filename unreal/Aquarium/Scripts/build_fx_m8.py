# M8이 런타임에 쓰는 머티리얼을 만든다. 레벨은 건드리지 않는다 --
# 물 밀림은 카메라 컴포넌트의 블렌더블이라 PostProcessVolume을 레벨에 넣지 않는다.
#
# M_Wake(난류 자국)는 **제거됐다.** 실제 화면에서 이동할 때 물고기에 회색 줄로
# 보였고, 사용자가 제거를 요구했다(2026-09-22). 헤드리스로는 확인할 수 없었던
# 바로 그 실패다 -- 계획 단계에서 "머티리얼이 기본 회색으로 대체될 수 있다"고
# 적어 둔 위험이 현실이 됐다.
# 따라서 ReefM1.umap은 M8에서도 한 바이트도 바뀌지 않는다(verify_scene.py 기대값 불변).
#
# Niagara는 쓰지 않는다(M4b에서 부딪힌 벽). 해석적 머티리얼만 쓴다.
# 판정은 마지막 FX8_OK 줄로만 한다.
#
# 이 스크립트의 이름들은 **추측이 아니라 실제 에디터에서 확인한 것**이다:
#   unreal.MaterialDomain.MD_POST_PROCESS
#   unreal.MaterialExpressionSceneTexture
#   unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0
# (M7에서 b_auto_create_cue / imported_size 같은 지어낸 이름에 당한 전례가 있다.)
import unreal

DEST = "/Game/Fx"
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

# C++이 찾는 파라미터 이름. 틀리면 **조용히 아무 일도 일어나지 않는다.**
STRENGTH_PARAM = "Strength"


def material(name):
    path = DEST + "/" + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)          # 멱등: 매번 같은 그래프를 다시 만든다
    return tools.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())


def expr(mat, cls, x, y):
    return mel.create_material_expression(mat, cls, x, y)


def build_water_push(mat):
    """물 밀림. 화면을 중심에서 바깥으로 Strength만큼 민다.

    포스트 프로세스 머티리얼이므로 도메인이 MD_POST_PROCESS다. UV는
    ScreenPosition에서 받아 중심(0.5, 0.5) 기준으로 스케일한다 -- 중심에서 먼
    픽셀일수록 더 밀리므로 '물이 밀려났다'로 읽힌다. Strength가 0이면 UV가
    그대로라 화면이 한 픽셀도 바뀌지 않는다."""
    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)

    strength = expr(mat, unreal.MaterialExpressionScalarParameter, -900, 400)
    strength.set_editor_property("parameter_name", STRENGTH_PARAM)
    strength.set_editor_property("default_value", 0.0)

    screen = expr(mat, unreal.MaterialExpressionScreenPosition, -900, 0)
    half = expr(mat, unreal.MaterialExpressionConstant2Vector, -900, 160)
    half.set_editor_property("r", 0.5)
    half.set_editor_property("g", 0.5)

    # centred = uv - 0.5
    centred = expr(mat, unreal.MaterialExpressionSubtract, -700, 60)
    mel.connect_material_expressions(screen, "", centred, "A")
    mel.connect_material_expressions(half, "", centred, "B")

    # push = centred * Strength * amount
    amount = expr(mat, unreal.MaterialExpressionConstant, -700, 460)
    amount.set_editor_property("r", 0.06)   # 최대 6% -- 눈에 띄되 멀미하지 않는다
    scaled = expr(mat, unreal.MaterialExpressionMultiply, -540, 420)
    mel.connect_material_expressions(strength, "", scaled, "A")
    mel.connect_material_expressions(amount, "", scaled, "B")
    push = expr(mat, unreal.MaterialExpressionMultiply, -380, 200)
    mel.connect_material_expressions(centred, "", push, "A")
    mel.connect_material_expressions(scaled, "", push, "B")

    # uv' = uv + push
    uv = expr(mat, unreal.MaterialExpressionAdd, -220, 60)
    mel.connect_material_expressions(screen, "", uv, "A")
    mel.connect_material_expressions(push, "", uv, "B")

    scene = expr(mat, unreal.MaterialExpressionSceneTexture, -60, 60)
    scene.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    mel.connect_material_expressions(uv, "", scene, "UV")
    mel.connect_material_property(scene, "Color", unreal.MaterialProperty.MP_EMISSIVE_COLOR)


built = []
for name, build in (("M_WaterPush", build_water_push),):
    m = material(name)
    assert m is not None, "could not create %s" % name
    build(m)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    built.append(name)
    print("  built %s" % name)

# C++이 이 이름으로 파라미터를 찾는다. 여기서 확인하지 않으면 오타가 런타임에
# **조용히** 아무 일도 안 하는 것으로만 드러난다.
push_mat = unreal.load_asset(DEST + "/M_WaterPush")
names = [str(n) for n in mel.get_scalar_parameter_names(push_mat)]
assert STRENGTH_PARAM in names, "M_WaterPush has no %s parameter: %s" % (STRENGTH_PARAM, names)
assert push_mat.get_editor_property("material_domain") == unreal.MaterialDomain.MD_POST_PROCESS, \
    "M_WaterPush is not a post process material"

sphere = unreal.load_asset("/Engine/BasicShapes/Sphere.Sphere")
assert sphere is not None, "missing /Engine/BasicShapes/Sphere"
print("FX8_OK count=%d materials=[%s] scalar_params=[%s]" % (len(built), " ".join(built), " ".join(names)))
