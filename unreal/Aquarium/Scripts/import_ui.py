# assets/ui/*.png 를 /Game/UI 아래 텍스처로 임포트한다. 멱등하다.
#
# sRGB는 켜 두고 압축은 UI 기본(TC_EditorIcon = 압축 없음)으로 둔다. M4a에서
# 러프니스 텍스처를 TC_Default로 임포트했다가 머티리얼이 회색 기본으로 조용히
# 떨어진 전례가 있어, 임포트 설정은 추측하지 않고 명시한다.
#
# 판정은 마지막 UI_OK 줄로만 한다(종료 코드는 항상 1이다).
import os
import unreal

ROOT = os.path.abspath(os.path.join(unreal.Paths.project_dir(), "..", ".."))
SRC = os.path.join(ROOT, "assets", "ui")
DEST = "/Game/UI"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary

ICONS = ["T_SoundOn", "T_SoundOff"]
rows = []
for name in ICONS:
    filename = os.path.join(SRC, name + ".png")
    assert os.path.exists(filename), "missing icon: %s (python3 assets/ui/make_icons.py)" % filename
    task = unreal.AssetImportTask()
    task.filename = filename
    task.destination_path = DEST
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    asset_tools.import_asset_tasks([task])
    tex = unreal.load_asset(DEST + "/" + name)
    assert tex is not None, "import failed: %s" % name
    tex.set_editor_property("srgb", True)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    eal.save_loaded_asset(tex)
    check = unreal.load_asset(DEST + "/" + name)
    # UTexture2D에는 'imported_size' 프로퍼티가 없다(계획의 오류). 실제로 있는 것은
    # blueprint_get_size_x/y 이고, dir()로 확인해 고쳤다.
    w = int(check.blueprint_get_size_x())
    h = int(check.blueprint_get_size_y())
    assert w == 128 and h == 128, "%s is %dx%d, expected 128x128" % (name, w, h)
    rows.append("%s:%dx%d" % (name, w, h))
    print("  imported %s %dx%d" % (name, w, h))

print("UI_OK count=%d [%s]" % (len(rows), " ".join(rows)))
