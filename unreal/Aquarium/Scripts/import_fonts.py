# Imports Noto Sans KR (SIL OFL 1.1) as a Slate/UMG FontFace asset at
# /Game/UI/FF_NotoSansKR. Idempotent: re-running reimports in place (no _1
# duplicate) because AssetImportTask.replace_existing=True.
#
# Why this only produces a FontFace, not a composed runtime Font asset:
# UE 5.8's Python reflection does not expose FCompositeFont::DefaultTypeface,
# FTypeface::Fonts, or FTypefaceEntry::Font/Name to script (they are plain
# UPROPERTY() with no Edit/BlueprintReadWrite specifiers -- verified by
# reading Engine/Source/Runtime/SlateCore/Public/Fonts/CompositeFont.h and by
# probing unreal.CompositeFont.get_editor_property("default_typeface") in the
# editor, which raises "Failed to find property 'default_typeface'"). There
# is no Python-visible API to author a UFont's composite font. Composing
# /Game/UI/F_NotoSansKR therefore has to happen either by hand once in the
# editor UI, or at runtime in C++ (construct an FStandaloneCompositeFont /
# use FSlateFontInfo pointing directly at the FontFace) -- the latter is the
# documented fallback for later tasks.
#
# Headless run note: this must run as a full headless Editor instance (Slate
# initialized, even under -nullrhi) via -ExecCmds="py <path>, quit" -- NOT
# via -run=pythonscript. The FontFileImportFactory used to import
# TTF/OTF files calls FSlateApplication::Get() during import (to flush the
# font cache) and segfaults immediately when run as a bare
# UPythonScriptCommandlet (-run=pythonscript), which never initializes Slate.
#
#   UnrealEditor-Cmd Aquarium.uproject -unattended -nopause -nosplash \
#     -nullrhi -stdout -FullStdOutLogOutput \
#     -ExecCmds="py Scripts/import_fonts.py, quit"
import os
import unreal

ROOT = os.path.abspath(os.path.join(unreal.Paths.project_dir(), "..", ".."))
OTF = os.path.join(ROOT, "assets", "fonts", "NotoSansKR-Regular.otf")
DEST = "/Game/UI"

assert os.path.isfile(OTF), "missing font: %s" % OTF

tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary

task = unreal.AssetImportTask()
task.filename = OTF
task.destination_path = DEST
task.destination_name = "FF_NotoSansKR"
task.automated = True
task.replace_existing = True
task.save = True
tools.import_asset_tasks([task])

face_path = DEST + "/FF_NotoSansKR"
face = unreal.load_asset(face_path)
assert isinstance(face, unreal.FontFace), "import did not produce a FontFace: %s" % face

# Embed the font data in the asset instead of streaming/lazy-loading from the
# absolute source path (which is a developer machine path, not something we
# can ship).
face.set_editor_property("loading_policy", unreal.FontLoadingPolicy.INLINE)
eal.save_loaded_asset(face)

sub_faces = face.get_editor_property("sub_faces")
print("FONT_OK face-only %s faces=%d subfaces=%s" % (face.get_path_name(), len(sub_faces), list(sub_faces)))
