# assets/audio/*.wav 를 /Game/Audio 아래 USoundWave로 임포트한다.
# 멱등하다: 같은 경로에 replace_existing=True로 다시 임포트하므로 _1 사본이 생기지 않는다.
#
# SoundCue도 MetaSound도 만들지 않는다 -- 사운드 그래프는 에디터 파이썬으로
# 만들 수 없고, 그래서 이 게임의 소리는 SoundWave + C++ 재생만으로 이루어진다.
#
# 앰비언스(A_Underwater)는 CC0 녹음이라 아직 저장소에 없을 수 있다. 없으면
# 조용히 넘어가지 않고 MISSING으로 찍고 마지막 줄에 남긴다 -- 도입 여부의
# 판정은 scripts/check_ambience.sh가 한다(그쪽이 빨간불의 주인이다).
#
# 헤드리스 실행:
#   UnrealEditor-Cmd Aquarium.uproject -run=pythonscript -script=Scripts/import_audio.py
#     -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput
# 판정은 마지막 AUDIO_OK 줄로만 한다. 종료 코드는 무관한 GameFeatureData 오류
# 때문에 항상 1이다.
import os
import unreal

ROOT = os.path.abspath(os.path.join(unreal.Paths.project_dir(), "..", ".."))
AUDIO_SRC = os.path.join(ROOT, "assets", "audio")
DEST = "/Game/Audio"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary

# (원본 파일, 에셋 이름, 루프 여부, 필수 여부)
WAVS = [
    (os.path.join(AUDIO_SRC, "S_Swim.wav"), "S_Swim", True, True),
    (os.path.join(AUDIO_SRC, "S_Startle.wav"), "S_Startle", False, True),
    (os.path.join(AUDIO_SRC, "S_Nibble.wav"), "S_Nibble", False, True),
    (os.path.join(AUDIO_SRC, "S_Split.wav"), "S_Split", False, True),
    (os.path.join(AUDIO_SRC, "S_Bubble.wav"), "S_Bubble", False, True),
    (os.path.join(AUDIO_SRC, "ambience", "A_Underwater.wav"), "A_Underwater", True, False),
]


def import_sound(filename, name, looping):
    factory = unreal.SoundFactory()
    # 자동 큐 생성을 끈다: SoundCue는 이 프로젝트가 쓰지 않는 에셋이고,
    # 생기면 배포물에 출처 없는 에셋이 하나 늘어난다.
    factory.set_editor_property("auto_create_cue", False)  # 계획서의 b_auto_create_cue는 틀린 이름이다(2026-09-21 확인)
    task = unreal.AssetImportTask()
    task.filename = filename
    task.destination_path = DEST
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = factory
    asset_tools.import_asset_tasks([task])
    paths = [str(p) for p in task.imported_object_paths]
    assert paths, "import produced no asset: %s" % filename
    sound = unreal.load_asset(DEST + "/" + name)
    assert sound is not None, "could not load back: %s/%s" % (DEST, name)
    sound.set_editor_property("looping", looping)
    eal.save_loaded_asset(sound)
    # 되읽어서 확인한다. set_editor_property가 조용히 무시되는 경우를 잡는다.
    check = unreal.load_asset(DEST + "/" + name)
    got = check.get_editor_property("looping")
    assert bool(got) == bool(looping), "%s looping=%s, wanted %s" % (name, got, looping)
    dur = float(check.get_editor_property("duration"))
    assert dur > 0.01, "%s has zero duration" % name
    return name, dur, bool(got)


rows = []
missing = []
for filename, name, looping, required in WAVS:
    if not os.path.exists(filename):
        assert not required, "missing required wav: %s" % filename
        missing.append(name)
        print("  MISSING  %-14s %s (scripts/check_ambience.sh 참고)" % (name, filename))
        continue
    n, dur, loop = import_sound(filename, name, looping)
    print("  imported %-14s duration=%.3f looping=%s" % (n, dur, loop))
    rows.append("%s:%.3f:%d" % (n, dur, 1 if loop else 0))

# 임포트되지 않은 잔여물이 없는지 확인한다 -- _Cue가 딸려 왔다면 여기서 보인다.
existing = sorted(os.path.basename(p) for p in eal.list_assets(DEST, recursive=False, include_folder=False))
existing = [e.split(".")[0] for e in existing]
print("AUDIO_OK count=%d missing=[%s] assets=[%s] listed=[%s]"
      % (len(rows), " ".join(missing), " ".join(rows), " ".join(existing)))
assert len(existing) == len(rows), "unexpected extra assets in %s: %s" % (DEST, existing)
