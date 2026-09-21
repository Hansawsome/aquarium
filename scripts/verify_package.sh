#!/bin/bash
# Asserts what must and must not be inside a packaged macOS build.
#
# Usage:  ./scripts/verify_package.sh <path-to-Aquarium.app> [Development|Shipping]
#         CONTROL=1 ./scripts/verify_package.sh ...   # also run the control cases
#
# ---------------------------------------------------------------------------
# WHY THIS SCRIPT DOES NOT USE `strings`
# ---------------------------------------------------------------------------
# macOS /usr/bin/strings (Xcode toolchain) has NO -e flag: it only ever sees
# 8-bit strings. Unreal 5.5+ TCHAR literals are UTF-16, so every TEXT("...")
# token in the game -- every dev flag, every module name -- is invisible to it.
# Measured on the M6 Development package: all 11 dev flags came back
# ascii=n utf16le=Y. A `strings | grep -c` absence check over a UE binary
# therefore returns 0 whether the token is absent or merely UTF-16, i.e. it is
# an absence check that CANNOT BITE. scripts/aq_scan.py scans both encodings.
#
# ---------------------------------------------------------------------------
# EVERY CHECK HERE ASSERTS AN ABSENCE, AND EVERY ABSENCE HAS A CONTROL
# ---------------------------------------------------------------------------
#   mcp-code-absent  control: the same scanner aimed at the engine's MCP plugin
#                    editor binaries must report hits (measured: 12 in its
#                    UnrealEditor.modules, 1823 in libUnrealEditor-ModelContextProtocol.dylib).
#   devflags         control: Development must have >0 (all 11), Shipping 0.
#                    A Shipping-only run proves nothing.
#
# ---------------------------------------------------------------------------
# THE COOKED-PAK MCP STRINGS ARE DATA, NOT A MODULE -- SCOPE IS DELIBERATE
# ---------------------------------------------------------------------------
# Scanning the whole bundle for "ModelContextProtocol" finds two files, and
# neither is a module. Both were opened byte-for-byte:
#   Aquarium-Mac.pak (2 hits)
#     1. the staged copy of Aquarium.uproject -- the "TargetAllowList":["Editor"]
#        entry, i.e. THE EXCLUSION RECORD ITSELF. Removing it would remove the fix.
#     2. an empty [/Script/ModelContextProtocol.MCPSettings] section header that
#        our own Config/DefaultEngine.ini used to carry. That section was dead
#        config for a now editor-only plugin and has been DELETED at source; the
#        mcp-config-absent check below keeps it deleted.
#   global.ucas (47 hits)
#     the engine's global reflection name table, which lists every /Script/ path
#     the engine knows. Engine-side data, present in any UE package, no module.
# So the gate is on CODE (executable, dylibs, .modules manifests, staged plugin
# dirs) and on OUR OWN CONFIG. Gating on "zero bytes anywhere in the bundle"
# would be a check nobody can ever make green, and those get weakened until they
# check nothing. The data hits are printed as a NOTE so they stay visible.
set -euo pipefail

APP="${1:?usage: verify_package.sh <Aquarium.app> [config]}"
CONFIG="${2:-Development}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SCAN="$ROOT/scripts/aq_scan.py"
# The executable is NOT always called "Aquarium": the Shipping bundle ships
# Contents/MacOS/Aquarium-Mac-Shipping. Hard-coding the Development name would
# make every Shipping check exit before it ran.
BIN="$(find "$APP/Contents/MacOS" -maxdepth 1 -type f -perm -u+x -print -quit 2>/dev/null || true)"
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
FAIL=0

say()  { printf '%-28s %s\n' "$1" "$2"; }
bad()  { say "$1" "FAIL  $2"; FAIL=1; }
good() { say "$1" "ok    $2"; }
note() { say "$1" "note  $2"; }

[[ -n "$BIN" && -x "$BIN" ]] || { echo "no executable under $APP/Contents/MacOS" >&2; exit 1; }
[[ -x "$SCAN" ]] || { echo "no scanner at $SCAN" >&2; exit 1; }
SCRATCH_TMP="$(mktemp)"; trap 'rm -f "$SCRATCH_TMP"' EXIT

# ---------- 1. MCP code must not be in the package ----------
# Code = the game executable, every dylib, every .modules manifest, and any
# staged plugin directory carrying the name.
CODE_LIST="$SCRATCH_TMP"
{ echo "$BIN"; find "$APP" -type f \( -name '*.dylib' -o -name '*.modules' \) ; } 2>/dev/null > "$CODE_LIST"
CODE_N="$(grep -c . "$CODE_LIST")"
MCP_CODE="$(xargs -0 -I{} true </dev/null; tr '\n' '\0' < "$CODE_LIST" | xargs -0 "$SCAN" ModelContextProtocol || true)"
MCP_DIR="$(find "$APP" -type d -name '*ModelContextProtocol*' 2>/dev/null || true)"
if [[ -z "$MCP_CODE" && -z "$MCP_DIR" ]]; then
  good "mcp-code-absent" "0 hits across $CODE_N code files, no staged plugin dir"
else
  bad "mcp-code-absent" "MCP code in the package:"; echo "$MCP_CODE"; echo "$MCP_DIR"
fi

# inert data hits, reported so they cannot quietly grow
PAKS="$(find "$APP/Contents/UE" -type d -name Paks -print -quit 2>/dev/null || true)"
MCP_DATA="$([[ -n "$PAKS" ]] && "$SCAN" ModelContextProtocol "$PAKS" 2>/dev/null || true)"
if [[ -n "$MCP_DATA" ]]; then
  note "mcp-data" "inert name-table/config strings in cooked data (see header):"
  echo "$MCP_DATA" | sed 's|^|                               |'
fi

# ---------- 2. our own config must not declare MCP settings ----------
if grep -rq 'ModelContextProtocol' "$ROOT/unreal/Aquarium/Config/"*.ini 2>/dev/null; then
  bad "mcp-config-absent" "Config/*.ini still declares an MCP section; it cooks into the pak"
else
  good "mcp-config-absent" "no MCP section in unreal/Aquarium/Config/*.ini"
fi

# ---------- 3. dev-only flags ----------
# The token list is DERIVED FROM SOURCE, never hand-copied. Regenerate with:
#   grep -rhoE 'Aquarium[A-Za-z]+' unreal/Aquarium/Source/Aquarium/*.cpp | sort -u
# and keep only the command-line switch names.
FLAGS_FILE="${FLAGS_FILE:-$ROOT/.m6-devflags.txt}"
[[ -f "$FLAGS_FILE" ]] || { echo "missing $FLAGS_FILE" >&2; exit 1; }
FOUND=0; MISSING=""; FOUND_LIST=""
while read -r f; do
  [[ -n "$f" ]] || continue
  if [[ -n "$("$SCAN" "$f" "$BIN")" ]]; then FOUND=$((FOUND+1)); FOUND_LIST="$FOUND_LIST $f"
  else MISSING="$MISSING $f"; fi
done < "$FLAGS_FILE"
TOTAL="$(grep -c . "$FLAGS_FILE")"
if [[ "$CONFIG" == "Shipping" ]]; then
  if [[ $FOUND -eq 0 ]]; then good "devflags-absent" "0 of $TOTAL dev flags in the Shipping binary"
  else bad "devflags-absent" "$FOUND of $TOTAL still present:$FOUND_LIST"; fi
else
  if [[ $FOUND -eq $TOTAL ]]; then
    good "devflags-present(control)" "$FOUND of $TOTAL in Development -- the scan can see flags"
  elif [[ $FOUND -gt 0 ]]; then
    bad "devflags-present(control)" "only $FOUND of $TOTAL in Development; missing:$MISSING"
  else
    bad "devflags-present(control)" "0 of $TOTAL in a DEVELOPMENT binary: the scan is blind, so a Shipping 0 would mean nothing"
  fi
fi

# ---------- 4. Korean font must be in the package (F-04) ----------
if [[ -n "$PAKS" && -n "$("$SCAN" NotoSansKR "$PAKS" 2>/dev/null)" ]]; then
  good "font-present" "NotoSansKR found in cooked data"
else
  bad "font-present" "Korean font not in the package -- F-04 would render tofu"
fi

# ---------- 5. OFL licence copy must travel with the font ----------
if find "$APP" -iname '*OFL*' 2>/dev/null | grep -q .; then
  good "font-licence" "OFL copy present in the bundle"
else
  bad "font-licence" "OFL 1.1 requires the licence to travel with the font (assets/fonts/OFL.txt is not staged)"
fi

# ---------- 6. no student data in the bundle ----------
if find "$APP" -type f -name '*.sav' 2>/dev/null | grep -q .; then
  bad "no-savegames" "the bundle ships .sav files -- inspect for student data"
else
  good "no-savegames" "no .sav files in the bundle"
fi

# ---------- controls ----------
if [[ "${CONTROL:-0}" == "1" ]]; then
  echo
  echo "--- controls (these must BITE, or the green above means nothing)"
  CD="$UE_ROOT/Engine/Plugins/Experimental/ModelContextProtocol/Binaries/Mac"
  if [[ -d "$CD" ]] && [[ -n "$("$SCAN" ModelContextProtocol "$CD" | head -1)" ]]; then
    say "control:mcp-scan" "BITES  $("$SCAN" ModelContextProtocol "$CD" | wc -l | tr -d ' ') editor files hit"
  else
    say "control:mcp-scan" "DID NOT BITE -- the mcp scan finds nothing even where MCP lives"; FAIL=1
  fi
  if grep -rq 'ModelContextProtocol' "$ROOT/unreal/Aquarium/Aquarium.uproject"; then
    say "control:config-scan" "BITES  the grep finds MCP in .uproject, so it can see the name"
  else
    say "control:config-scan" "DID NOT BITE"; FAIL=1
  fi
fi

echo
if [[ $FAIL -eq 0 ]]; then echo "VERIFY_OK config=$CONFIG"; else echo "VERIFY_FAILED config=$CONFIG"; exit 1; fi
