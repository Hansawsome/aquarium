# macOS 개발 도구와 MCP 연결

상태 기준: 2026-09-21. 규칙 계층·Unreal 프로젝트·M1/M2/M2b/M3 장면과 에셋은 이 문서 끝의 재현 명령으로 만든다.

## 완료

- Homebrew로 Blender 5.2.2 LTS와 Epic Games Launcher 20.1.4 설치.
- CMake 4.4.3, Git LFS 3.8.0 설치 및 이 저장소의 LFS hook 활성화.
- Blender 실행, MCP 애드온 활성화와 환경설정 저장.
- PyPI 배포본 `mcp-for-blender==2.0.0`을 uv tool로 설치, Python 3.11 사용.
- Codex에 `blender` stdio MCP 등록. 기존 다른 MCP 항목은 보존.
- 2026-09-20 Claude Code에도 프로젝트 범위(`.mcp.json`)로 같은 서버 등록. 새 세션에서 승인 후 도구 사용 가능.
- Blender 애드온의 telemetry 동의와 MCP 프로세스의 telemetry를 모두 비활성화.

## Blender 연결 구성

- 실행 파일: `/Users/hans/.local/bin/mcp-for-blender`
- Python 환경: `/Users/hans/.local/share/uv/tools/mcp-for-blender/`
- 애드온: `~/Library/Application Support/Blender/5.2/scripts/addons/blender_mcp.py`
- 연결: `127.0.0.1:9876`, Blender 실행 시 애드온이 자동 시작.
- Codex 설정: `~/.codex/config.toml`의 `mcp_servers.blender`.
- Claude Code 설정: 저장소의 `.mcp.json` (`mcpServers.blender`, 동일 실행 파일·환경 변수).
- 환경 변수: `DISABLE_TELEMETRY=true`, `BLENDER_HOST=127.0.0.1`, `BLENDER_PORT=9876`.
- 외부 유료 3D 생성 서비스는 연결하지 않았다.

이 설정은 개발 도구용이며 배포 게임의 의존성이 아니다. Codex 설정 등록과 현재 대화의 도구 목록 갱신은 별개다. 현재 대화에 새 MCP 도구가 보이지 않으면 Codex 재시작 후 확인한다.

## 실제 수행한 연결 검증

Python MCP SDK로 등록된 실행 파일을 시작하고 stdio 클라이언트에서 아래 호출을 수행했다. Blender를 실행한 상태에서 확인했으며 장면은 수정하지 않았다.

| 확인 | 결과 |
|---|---|
| `lsof -nP -iTCP:9876 -sTCP:LISTEN` | Blender가 127.0.0.1에서 수신 |
| MCP `initialize` | 성공 |
| MCP `tools/list` | 31개 도구 |
| `get_addon_status` | 프로토콜 7, 최신 상태, Blender 5.2.2 LTS, telemetry false |
| `get_scene_info` | 기본 장면의 Cube, Light, Camera 반환 |
| `execute_blender_code` | 버전과 telemetry 설정을 읽는 Python 코드 실행 성공 |

2026-09-20 Claude Code 등록 직후 동일 stdio 클라이언트로 재검증: `initialize` 성공, `tools/list` 31개, `execute_blender_code`로 Blender 5.2.2 LTS·객체 3개 확인. `get_scene_info`는 `user_prompt` 인자가 필수다.

초기 GitHub 소스 체크아웃은 telemetry 구성 모듈이 배포용으로 제외되어 `get_addon_status`에서 오류가 났다. 소스에 임시 코드를 추가하지 않고 동일 버전의 정식 PyPI 배포본으로 교체한 뒤 위 검증을 통과했다. 검사한 원본은 `~/dev/tools/blender-mcp`, 커밋 `6f992ffbca3cb715d111fc640b737b808632273c`에 남아 있으며 현재 MCP 실행 경로는 이 소스가 아니다.

## 남은 설치

### Xcode

2026-09-20 사용자가 정식 Xcode를 직접 설치했다. 확인 결과:

| 확인 | 결과 |
|---|---|
| `xcodebuild -version` | Xcode 27.0, Build 27A266a |
| `xcode-select -p` | `/Applications/Xcode.app/Contents/Developer` |
| `xcodebuild -checkFirstLaunchStatus` | 종료 코드 0 (초기 구성 완료) |
| `xcodebuild -showsdks` | macOS 27.0 SDK |
| Metal Toolchain | 초기에 누락. `xcodebuild -downloadComponent MetalToolchain`으로 838.9MB 다운로드·설치 후 `xcrun metal --version` = 32023.921 정상 |

**버전 불일치 위험.** 당초 계획은 Epic의 UE 5.8 Mac 문서에 따른 Xcode 26.1.1이었다. 2026-09-20 재확인한 Epic 문서는 UE 5.8에 대해 최소 26.0, 권장 26.1.1을 명시하고 Xcode 26.4는 비호환이라고 적으며 27.x는 언급하지 않는다. Unreal Build Tool은 지원 범위 밖 Xcode를 거부할 수 있으므로, 엔진 설치 후 빈 프로젝트 C++ 컴파일이 실패하면 26.1.1을 `/Applications/Xcode-26.1.1.app`로 병행 설치하고 `xcode-select`로 전환한다. 27.0으로 빌드가 성공하면 그 사실을 여기에 기록한다.

### Unreal Engine — 설치 및 빌드 검증 (2026-09-20)

UE 5.8.2를 Launcher로 설치했다. C++ 프로젝트 골격을 `unreal/Aquarium/`에 만들고(모듈 `Aquarium`, `BuildSettingsVersion.Latest`) 다음을 실제 수행했다.

| 확인 | 명령 | 결과 |
|---|---|---|
| 에디터 타깃 컴파일 | `Engine/Build/BatchFiles/Mac/Build.sh AquariumEditor Mac Development -Project=…` | `Result: Succeeded` (18.5초, Xcode 27.0 / LLVM 21.1.6). `Binaries/Mac/libUnrealEditor-Aquarium.dylib` 생성 |
| 에디터 헤드리스 실행 | `UnrealEditor-Cmd Aquarium.uproject -run=pythonscript -script="print('EDITOR_SMOKE_OK')" -unattended -nullrhi` | 엔진 5.8.2 초기화, 프로젝트 모듈 로드, Python 3.11.8로 `EDITOR_SMOKE_OK` 출력, 종료 코드 0 |
| 게임 타깃 컴파일 | `Build.sh Aquarium Mac Development -Project=…` | 컴파일·링크 성공, `Binaries/Mac/Aquarium.app` 생성. **단, UBT의 마지막 "App finalization"(xcodebuild PostBuildSync) 단계가 `Touch UBT generated tiles` 스킴 pre-action에서 출력 없이 실패(exit 65)** |
| 앱 마무리 직접 실행 | UBT 로그에 찍힌 동일한 `xcodebuild … UE_XCODE_BUILD_MODE=PostBuildSync` 명령을 직접 실행 | `BUILD SUCCEEDED`, `codesign -dv`로 ad-hoc 서명 확인 |

첫 시도는 `BuildSettingsVersion.V5`가 5.8 설치형 엔진의 공유 빌드 환경과 충돌해 실패했고 `Latest`로 바꿔 해결했다.

**미해결:** UBT 내부에서만 앱 마무리가 실패한다. 샌드박스 비활성화, `Build/Mac/Resources` 디렉터리 생성, 존재하지 않는 `TMPDIR` 재현 시도로는 원인을 못 찾았다. Xcode 27 + UE 5.8.2 조합 이슈로 추정한다. 컴파일 검증 목적은 달성했으므로 이월하고, M5 패키징(`RunUAT BuildCookRun`) 단계에서 재확인한다. 에디터 GUI 실행은 아직 하지 않았다(헤드리스 commandlet만).

### Unreal 공식 MCP — 연결 검증 완료 (2026-09-20)

`Aquarium.uproject`에 `ModelContextProtocol`과 `AllToolsets` 플러그인을 활성화했다. 에디터 GUI를 다음 명령으로 실행하면 서버가 자동 시작된다:

```bash
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" \
  "$PWD/unreal/Aquarium/Aquarium.uproject" \
  -ExecCmds="ModelContextProtocol.EnableAnalytics 0, ModelContextProtocol.StartServer" -log
```

| 확인 | 결과 |
|---|---|
| 에디터 GUI 실행 | 창 `Aquarium - 언리얼 에디터` 확인(System Events). 실행 후 약 35~65초에 `127.0.0.1:8000` LISTEN |
| MCP `initialize` (Streamable HTTP, `POST /mcp`) | 200, `Mcp-Session-Id` 발급, protocolVersion 2025-06-18 |
| `tools/list` | 메타 도구 3개: `list_toolsets`, `describe_toolset`, `call_tool` |
| `list_toolsets` | **67개 툴셋** (EditorAppToolset, LogsToolset, Niagara, PCG, UMG, Sequencer 등) |
| 읽기 전용 조회 | `call_tool(toolset_name="EditorToolset.EditorAppToolset", tool_name="GetVisibleActors")` → 현재 레벨 액터 목록 반환 |
| 뷰포트 캡처 | `CaptureViewport(captureTransform=현재 카메라, annotations=[])` → 2027×1090 PNG. **macOS 화면 기록 권한 없이도 실제 렌더링 결과를 확인하는 채널** |

호출 규약: `call_tool` 인자는 `toolset_name`(툴셋 전체 이름) + `tool_name`(접두사 없는 이름) + `arguments`. 선택 인자도 스키마에 있으면 명시해야 한다(예: `CaptureViewport`의 `captureTransform`, `annotations`는 생략 시 "needs a default value" 오류). 에디터 호출은 순차 수행한다.

Claude Code 등록: `.mcp.json`의 `unreal` (HTTP, `http://127.0.0.1:8000/mcp`). 에디터가 꺼져 있으면 연결 실패가 정상이다. 배포 게임에는 이 플러그인의 서버가 포함되지 않도록 패키징 단계(M5)에서 확인한다.

참고: `DefaultEngine.ini`의 시작 맵 `/Engine/Maps/Templates/OpenWorld`는 에디터가 `Untitled_1`로 열었다. M1에서 프로젝트 자체 맵을 만들면 교체한다.

## M1~M3 재현 명령 (2026-09-21)

모든 에셋은 스크립트 산출물이다. 저장소 루트에서:

```bash
# 1. 규칙 계층 테스트 (66개)
cmake -S . -B build && cmake --build build -j && ctest --test-dir build --output-on-failure

# 2. 에셋 제작 (Blender 5.2, 각 약 1분; 공통 모듈 assets/blender/fishlib.py)
for f in bluetang clownfish yellowtang butterflyfish damselfish; do
  /Applications/Blender.app/Contents/MacOS/Blender -b -P assets/blender/make_$f.py
done
/Applications/Blender.app/Contents/MacOS/Blender -b -P assets/blender/make_corals.py   # 산호 3종

# 3. Unreal 에디터 빌드 (이 머신은 UnrealEditor.modules 갱신이 한 번 늦어 두 번 실행)
UE="/Users/Shared/Epic Games/UE_5.8"
"$UE/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex
"$UE/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex

# 4. FBX 임포트(다섯 종 + 소품) → 장면 생성 → 검증 (각각 에디터 부팅 약 1분; 종료 코드 대신 IMPORT_OK/REEF_OK/SCENE_OK 확인)
for s in import_fish.py import_props.py build_reef_m1.py verify_scene.py; do
  "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -run=pythonscript -script="$PWD/unreal/Aquarium/Scripts/$s" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -E "_OK|Traceback"
done

# 4b. 한국어 폰트 (FontFace 에셋; 폰트 임포트는 commandlet에서 크래시하므로 ExecCmds 방식)
"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput -ExecCmds="py $PWD/unreal/Aquarium/Scripts/import_fonts.py, quit" 2>&1 | grep FONT_OK

# 5. Unreal Automation 테스트 (37개)
"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep "Test Completed"

# 6. 영상: M1 34초 유영(-dumpmovie, UI 없음) / M2 13.5초 입장→세션→나가기 흐름(UI 포함 프레임 캡처, 개발 전용 옵션)
scripts/render_m1_video.sh
scripts/render_m2_video.sh    # M2 흐름 -AquariumAutoNickname=니모(테스트 데이터만) -AquariumAutoExitAfter=8 -AquariumAssignmentSeed=1 -AquariumCaptureUI=<dir>
scripts/render_m2b_video.sh   # M2b 산호초 22초 관람
scripts/render_m3_video.sh    # M3 방향키 조종 20초 (개발 전용 -AquariumAutoNickname=니모(테스트 데이터만) -AquariumAutoInput="R3,U2,L3,D2,0 2,R2,U2,0 2" -AquariumAssignmentSeed=1 -AquariumCaptureUI=<dir>; 빌드 2회 포함 약 3분, SKIP_BUILD=1로 생략)
scripts/measure_m2b_perf.sh   # 프레임 시간 CSV → docs/reviews/<날짜>-m2b-perf.md (-benchmark 미사용)
```

주의: GUI 에디터를 강제 종료한 뒤 다음 실행이 "패키지 복구" 프롬프트에 걸리면 `unreal/Aquarium/Saved/Autosaves`를 지운다. MCP `CaptureViewport`로 뷰포트 이미지를 얻는 절차는 위 "Unreal 공식 MCP" 절 참고.

## 출처

- [Blender MCP 원본과 설치법](https://github.com/ahujasid/mcp-for-blender)
- [PyPI 배포본](https://pypi.org/project/mcp-for-blender/2.0.0/)
- [Codex MCP 설정](https://developers.openai.com/codex/mcp)
- [Epic Mac 개발 요구사항](https://dev.epicgames.com/documentation/en-us/unreal-engine/macos-development-requirements-for-unreal-engine)
- [Unreal 공식 MCP](https://dev.epicgames.com/documentation/unreal-engine/unreal-mcp-in-unreal-editor)
- [Apple Xcode 26.1.1 다운로드 목록](https://developer.apple.com/download/all/?q=Xcode%2026.1.1)
