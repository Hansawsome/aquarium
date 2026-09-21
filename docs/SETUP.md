# macOS 개발 도구와 MCP 연결

상태 기준: 2026-09-21. 규칙 계층·Unreal 프로젝트·M1/M2/M2b/M3/M4a/M4b/M4c 장면과 에셋은 이 문서 끝의 재현 명령으로 만든다.

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

**미해결(M0 기록).** UBT 내부에서만 앱 마무리가 실패한다. 샌드박스 비활성화, `Build/Mac/Resources` 디렉터리 생성, 존재하지 않는 `TMPDIR` 재현 시도로는 원인을 못 찾았다.

**M6에서 재확인(2026-09-21) — 여전히 원인 미특정, 우회 적용.** `RunUAT BuildCookRun` 단계에서
이 문제를 정면으로 다뤘고 진단 기록은 [`docs/reviews/2026-09-21-m6-ubt.md`](reviews/2026-09-21-m6-ubt.md)에 있다.
**현재 환경에서 그대로 재현된다.** 명령·스킴·pre-action 스크립트 본문·환경 변수·cwd·stdin은 전부
기각됐다 — 완전히 동일한 `xcodebuild … UE_XCODE_BUILD_MODE=PostBuildSync` 명령이 셸에서는
`env -i`로 환경을 비워도 성공하고, **UBT 액션 실행기의 자식으로 돌 때만** 실패한다. Xcode가 남기는
진단은 base64 워크스페이스 토큰 하나뿐이고 result bundle에는 오류도 경고도 없다.
**가장 유력한 남은 설명은 Xcode 버전이다** — Epic은 UE 5.8에 Xcode 26.0 최소 / 26.1.1 권장을
문서화하고 27.x는 언급하지 않는데 이 기계는 Xcode 27.0 (27A266a) + MacOSX27.0 SDK다. 즉
**Xcode 26.1.1로 내리는 것은 우회가 아니라 정상화**일 가능성이 높다. **Xcode 설치는 사용자 결정이므로
M6에서 실행하지 않았다. 이 항목은 "해결"이 아니라 알려진 이탈(known deviation)로 닫는다.**

**패키징은 이 결함에 막히지 않는다.** `scripts/package_mac.sh`가 우회를 스크립트 안에 박아 둔다 —
`UE_BUILD_FROM_XCODE=1`로 빌드해 receipt를 쓰게 한 뒤 PostBuildSync를 스크립트가 직접 실행하고,
`-skipbuild`로 쿡·스테이징한다. 이 경로는 Development·Shipping 양쪽에서 동작하며, 실행할
`xcodebuild` 명령은 기억이 아니라 `Build.sh`의 stdout(`params:` 줄)에서 grep으로 되찾는다.
(계획서가 적은 `Engine/Programs/UnrealBuildTool/Log.txt`는 이 설치본에 존재하지 않는다.)

에디터 GUI 실행은 M0 시점에는 하지 않았다(헤드리스 commandlet만). 이후 MCP 검증에서 GUI를 띄웠다.

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

Claude Code 등록: `.mcp.json`의 `unreal` (HTTP, `http://127.0.0.1:8000/mcp`). 에디터가 꺼져 있으면
연결 실패가 정상이다. **배포 게임 포함 여부 — M6에서 확인하고 조치했다(2026-09-21).** 확인해 보니
`ModelContextProtocol`의 모듈 두 개가 `Type: Runtime`이라 **아무 조치 없이 패키징하면 배포 게임에
MCP 런타임이 들어간다.** `Aquarium.uproject`의 두 플러그인 항목에 `"TargetAllowList": ["Editor"]`를
추가해 에디터 타깃에서만 켜지게 했다(플러그인을 끄지 않은 이유는 `CaptureViewport` 검증 채널을
유지하기 위해서다). 배제는 주장이 아니라 단언으로 확인한다 — `scripts/verify_package.sh`가 패키지의
**코드**(실행 파일·dylib·`.modules` 매니페스트·스테이징된 플러그인 디렉터리)와 **우리 `.ini`**에서
`ModelContextProtocol` 0건을 확인하고, 같은 스캐너를 엔진의 MCP 플러그인 디렉터리에 겨눈 대조군이
**빨간불(7개 파일 적중)**임을 같은 실행에서 함께 단언한다.

**계획서의 원래 MCP 검사는 아무것도 검사하지 않았다** — "게임 빌드 로그에서 0건"은 대조군에서도
0건이었다(UBT는 플러그인 이름을 stdout에 찍지 않는다). 실제로 무는 검사는 UBT 중간 산출물 쪽이다:
게임 `Aquarium.rsp` 6건 → 0건, 에디터 메타데이터는 16건(플러그인 유지).

**쿡된 데이터의 MCP 문자열은 모듈이 아니다.** `Aquarium-Mac.pak` 1건은 스테이징된 `.uproject`의
`TargetAllowList` 항목, 곧 **배제 기록 그 자체**이고, `global.ucas` 47건은 엔진 전역 이름 표다.
그래서 게이트는 "번들 어디에도 0바이트"가 아니라 코드와 우리 설정에 걸려 있고, 데이터 적중은
`note`로 계속 보이게 둔다. 실행 중 `lsof -nP -iTCP:8000 -sTCP:LISTEN` 0행도 볼 수 있지만 그쪽은
약한 검사이므로 보조다.

참고: `DefaultEngine.ini`의 시작 맵 `/Engine/Maps/Templates/OpenWorld`는 에디터가 `Untitled_1`로 열었다. M1에서 프로젝트 자체 맵을 만들면 교체한다.

## M1~M6 재현 명령 (2026-09-21)

모든 에셋은 스크립트 산출물이다. 저장소 루트에서:

```bash
# 1. 규칙 계층 테스트 (90개) — 기대 출력은 `100% tests passed out of 90`
#    (이 ctest는 "0 tests failed out of N" 형식을 쓰지 않는다. 그 문자열을 grep하면 항상 0건이라 조용히 통과한다)
cmake -S . -B build && cmake --build build -j && ctest --test-dir build --output-on-failure

# 2. 에셋 제작 (Blender 5.2, 각 약 1분; 공통 모듈 assets/blender/fishlib.py)
for f in bluetang clownfish yellowtang butterflyfish damselfish; do
  /Applications/Blender.app/Contents/MacOS/Blender -b -P assets/blender/make_$f.py
done
/Applications/Blender.app/Contents/MacOS/Blender -b -P assets/blender/make_corals.py   # 산호 5종(M4b), 종당 1024² 3장 베이크

# 3. Unreal 에디터 빌드 (이 머신은 UnrealEditor.modules 갱신이 한 번 늦어 두 번 실행)
UE="/Users/Shared/Epic Games/UE_5.8"
"$UE/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex
"$UE/Engine/Build/BatchFiles/Mac/Build.sh" AquariumEditor Mac Development -Project="$PWD/unreal/Aquarium/Aquarium.uproject" -WaitMutex

# 4. FBX 임포트(다섯 종 + 소품) → 장면 생성 → 검증 (각각 에디터 부팅 약 1분; 종료 코드 대신 IMPORT_OK/REEF_OK/SCENE_OK 확인)
for s in import_fish.py import_props.py import_audio.py build_reef_m1.py verify_scene.py; do
  "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -run=pythonscript -script="$PWD/unreal/Aquarium/Scripts/$s" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -E "_OK|Traceback"
done

# 4b. 한국어 폰트 (FontFace 에셋; 폰트 임포트는 commandlet에서 크래시하므로 ExecCmds 방식)
"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput -ExecCmds="py $PWD/unreal/Aquarium/Scripts/import_fonts.py, quit" 2>&1 | grep FONT_OK

# 5. Unreal Automation 테스트 (47개)
"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/unreal/Aquarium/Aquarium.uproject" -ExecCmds="Automation RunTests Aquarium; Quit" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep "Test Completed"

# 6. 영상: M1 34초 유영(-dumpmovie, UI 없음) / M2 13.5초 입장→세션→나가기 흐름(UI 포함 프레임 캡처, 개발 전용 옵션)
scripts/render_m1_video.sh
scripts/render_m2_video.sh    # M2 흐름 -AquariumAutoNickname=니모(테스트 데이터만) -AquariumAutoExitAfter=8 -AquariumAssignmentSeed=1 -AquariumCaptureUI=<dir>
scripts/render_m2b_video.sh   # M2b 산호초 22초 관람
scripts/render_m3_video.sh    # M3 방향키 조종 20초 (개발 전용 -AquariumAutoNickname=니모(테스트 데이터만) -AquariumAutoInput="R3,U2,L3,D2,0 2,R2,U2,0 2" -AquariumAssignmentSeed=1 -AquariumCaptureUI=<dir>; 빌드 2회 포함 약 3분, SKIP_BUILD=1로 생략)
scripts/measure_m2b_perf.sh   # 프레임 시간 CSV → docs/reviews/<날짜>-m2b-perf.md (-benchmark 미사용)

# 7. M4a 검토 산출물: 근접 스틸 + 산호초 영상 + 장면 스틸 + 개선 전/후 나란히 비교
scripts/render_m4a_compare.sh   # CLOSEUP_OK / VIDEO_OK / SCENE_OK / COMPARE_OK 확인

# 8. M4b 검토 산출물: 산호 5종 근접 스틸 + 산호초 영상 + 장면 스틸 + M4a 대비 나란히 비교
scripts/render_m4b_compare.sh   # CLOSEUP_OK / VIDEO_OK / SCENE_OK / COMPARE_OK + MATERIAL_COMPILE_FAILURES=0 확인

# 9. M4c 검토 산출물: 산호초 영상(45초) + 수직 전환 전용 영상(60 fps 12초) + 장면 스틸 + M4b 대비 비교
scripts/render_m4c_compare.sh   # VIDEO_OK / VERTICAL_OK / SCENE_OK / COMPARE_OK +
                                # MATERIAL_COMPILE_FAILURES=0 AUTOINPUT_UNKNOWN=0 AUTOINPUT_BAD_DURATION=0 확인

# 10. M5 검토 산출물: 클릭 도망 클립(30초) + 클릭 로그 CSV + 장면 스틸 + M4c 대비 비교
scripts/render_m5_click.sh      # GUARDS_OK / VIDEO_OK / STILL_OK / COMPARE_OK +
                                # ARMED=7 CLICK_BAD=0 CLICK_RANGE=0 INPUT_UNKNOWN=0 INPUT_BAD=0
                                # MATERIAL_COMPILE_FAILURES=0 CLICK_ROWS=7, 적중/비적중 혼재 확인

# 11. M4c/M5 항목별 성능 분해 (기본 → 무리 끔 → 소품 회피 끔 → 둘 다 끔 → 도망 끔)
scripts/measure_m2b_perf.sh
EXTRA_ARGS="-AquariumNoSchooling" scripts/measure_m2b_perf.sh
EXTRA_ARGS="-AquariumNoPropAvoid" scripts/measure_m2b_perf.sh
EXTRA_ARGS="-AquariumNoSchooling -AquariumNoPropAvoid" scripts/measure_m2b_perf.sh
EXTRA_ARGS="-AquariumNoFlee" scripts/measure_m2b_perf.sh

# 12. M6 패키징과 검증 (출력 디렉터리 B는 저장소 밖이다. 패키지는 커밋하지 않는다)
CONFIG=Development ./scripts/package_mac.sh          # PACKAGE_OK config=Development
CONFIG=Shipping    ./scripts/package_mac.sh          # PACKAGE_OK config=Shipping
B=<패키지 출력 디렉터리>                               # package_mac.sh가 APP= 줄로 찍어 준다
CONTROL=1 ./scripts/verify_package.sh "$B/Development/Mac/Aquarium.app"              Development
CONTROL=1 ./scripts/verify_package.sh "$B/Shipping/Mac/Aquarium-Mac-Shipping.app"    Shipping
./scripts/verify_privacy.sh auto "$B/Development/Mac/Aquarium.app"   # AUTO_TOTAL>0 이고 전부 명령줄 에코
APP="$B/Development/Mac/Aquarium.app" ./scripts/measure_package_perf.sh  # SRS 공식 측정 210초
APP="$B/Development/Mac/Aquarium.app" ./scripts/render_m6_package.sh    # VIDEO_OK/STILL_OK/COMPARE_OK
```

**Shipping 번들 이름이 다르다.** Development는 `Aquarium.app`이지만 Shipping은
`Aquarium-Mac-Shipping.app`이고 실행 파일도 `Contents/MacOS/Aquarium-Mac-Shipping`이다.
`Aquarium.app`을 가정한 스크립트는 성공한 Shipping 패키지를 "없음"으로 보고하고 검증을 아예
돌리지 못한다 — M6에서 실제로 그랬고, 두 스크립트 모두 이제 실행 파일을 `find`로 찾는다.

**성능 측정 길이 주의.** `measure_m2b_perf.sh`의 기본값은 `RUN_SEC=95`이고, 예열 30초를 빼면
실측 구간이 **65초**뿐이다. SRS 38행은 3분을 요구하므로 **공식 판정은 반드시
`measure_package_perf.sh`(기본 `RUN_SEC=210`)로 한다.** 이 스크립트는 `RUN_SEC`이
예열+180초 미만이면 그 자리에서 거부한다.

**패키지는 샌드박스에서 돈다.** 로그는 `~/Library/Logs/Aquarium`이 아니라
`~/Library/Containers/com.YourCompany.Aquarium/Data/Library/Logs/Aquarium/`에 쌓이고,
`-AquariumFrameStats`·`-AquariumCaptureUI`가 컨테이너 밖 경로(저장소 포함)를 가리키면
**오류 없이 조용히 아무것도 쓰지 않아 크래시처럼 보인다.** 컨테이너 로그 디렉터리를
`rm -rf` 후 다시 만들지 말 것 — 소유권이 바뀌어 샌드박스가 모든 쓰기를 조용히 거부한다.

**Shipping 빌드는 헤드리스로 검증할 수 없다.** `#if !UE_BUILD_SHIPPING` 안의 개발 플래그가
없으므로 자동 입장도 자동 캡처도 불가능하다. 그것이 의도한 성질이며, Shipping 빌드의 게임 플레이는
사람만 확인할 수 있다.

**2단계 비고 (M4a 이후 물고기 임포트는 종당 세 장).** 물고기 스크립트는 종마다 2048² 세 장
(`T_<종>_{BaseColor,Normal,Roughness}.png`)을 베이크하므로 한 종에 약 1분이 더 걸린다.
각 스크립트는 `verts=… bones=10 unweighted=0 rootMaxW=0.000` 줄을 출력하고, 본 목록
(`Root, Spine0..5, Tail, PecL, PecR`)이 깨지면 그 자리에서 실패한다. 4단계의 `import_fish.py`는
이 세 장을 모두 임포트해 `M_<종>`을 Subsurface 셰이딩 모델로 다시 구성하며
`IMPORT_OK species=… maps=[BaseColor,Normal,Roughness]` 다섯 줄을 출력한다.
러프니스 계열 텍스처는 반드시 `TC_MASKS`로 임포트하고 머티리얼에서 `SAMPLERTYPE_MASKS`로 샘플링한다.
sRGB만 끄고 `TC_Default`로 두면 "Sampler type is Linear Grayscale, should be Linear Color"로
머티리얼 컴파일이 조용히 실패하고 해당 오브젝트가 회색 기본 머티리얼로 그려진다
(M4a에서 물고기 5종과 바위 3종에서 각각 한 번씩 발생했다. `import_props.py`도 같은 규약).

**7단계 비고.** `scripts/render_m4a_compare.sh`는 임시 레벨 `/Game/Maps/_CloseupTmp`에 물고기
한 마리와 `DiverCamera` 태그가 붙은 카메라를 1 m 앞에 놓고 근접 스틸을 찍은 뒤 레벨을 지운다
(`CLOSEUP_SPECIES`·`CLOSEUP_FOV`로 종과 화각을 바꾼다. 기본 BlueTang / 30도 — 산호초의 75도로는
25 cm 물고기가 화면의 1/6이라 눈·비늘 판단에 쓸 수 없다). 산호초 영상과 장면 스틸은
`render_m3_video.sh`와 완전히 같은 맵·시드·자동 입력(`-AquariumAutoNickname=니모`(테스트 데이터만),
`-AquariumAutoInput`, `-AquariumAssignmentSeed=1`)으로 찍고, `BEFORE`(기본
`docs/reviews/2026-09-21-m3-wall.png`)와 `hstack`으로 붙여 `-compare.png`를 만든다.
`SKIP_BUILD=1`, `SKIP_CLOSEUP=1`, `SKIP_SCENE=1`로 단계를 건너뛴다.

**성능 재측정 비고.** `measure_m2b_perf.sh`는 산출물 이름을 항상 `<날짜>-m2b-*`로 쓴다.
M4a 재측정본은 실행 후 CSV를 `<날짜>-m4a-frametimes.csv`로 옮기고, 보고서는 M2b 기준선 비교를
더해 `docs/reviews/<날짜>-m4a-perf.md`에 직접 작성했다.

**에디터 Python 호출 형태 — 반드시 `-nullrhi -stdout -FullStdOutLogOutput`.** 위 4·4b·5단계의
명령 형태는 선택이 아니다. `-FullStdOutLogOutput` 없이 `-run=pythonscript`를 돌리면 스크립트는
**아무것도 출력하지 않는다** — `REEF_OK`도, 실패했을 때의 `AssertionError`조차도 보이지 않는다.
그런데도 스크립트는 **실제로 실행되어 레벨을 덮어쓴다.** 게다가 **종료 코드는 항상 1**이다.
무관한 `GameFeatures: Error: Asset manager settings do not include a rule for assets of type
GameFeatureData` 때문이며 스크립트 성패와 관계가 없다. 즉 출력도 종료 코드도 믿을 수 없어,
성공과 조용한 실패가 겉보기에 완전히 똑같아지는 함정이다. **판정은 반드시 `*_OK` 표지 문자열의
grep으로 한다**(`IMPORT_OK` / `REEF_OK` / `SCENE_OK` / `FONT_OK`). 종료 코드는 보지 않는다.

**산호 3장 베이크 비고(M4b).** `make_corals.py`는 이제 다섯 종(`BranchCoral`, `PlateCoral`,
`BrainCoral`, `FanCoral`, `TubeCoral`) 각각에 대해 `T_<종>_{BaseColor,Normal,Roughness}.png`
1024² 세 장을 `fishlib.bake_maps`로 굽는다(2단계, 약 5분). 물고기와 같은 규약으로
러프니스는 `TC_MASKS` + `SAMPLERTYPE_MASKS`로 임포트한다. **Blender 5.2의 Bump 노드 `Distance`
기본값은 1.0이 아니라 0.001**이라 명시하지 않으면 노멀 맵이 조용히 무효가 된다. `bake_maps`의
`min_variance`를 산호에서는 1e-4로 올려 이 경우를 퇴화로 잡는다(기본 바닥값은 분산 2.37e-06짜리
무효 맵도 통과시켰다).

**항목별 성능 토글(M4b).** `build_reef_m1.py`는 M4b에서 추가한 항목을 **하나씩** 끄는 환경 변수를
읽는다. 아무것도 설정하지 않은 상태가 출하 설정이며, 커밋된 레벨은 언제나 그 상태로만 빌드한다.

```bash
AQ_DROP_CURTAINS=near   # 근거리 부유 입자 커튼 1장만 제거
AQ_DROP_CURTAINS=all    # 커튼 3장 전부 제거
AQ_DROP_GOBO=coarse     # 거친층(빛줄기 대비용) 고보 평면 제거
AQ_DROP_GOBO=fine       # 고운층(바닥 물결 무늬용) 고보 평면 제거
AQ_NO_SSAO=1            # 후처리 AO 강도 0
AQ_PROP_COUNT=14        # 프롭 22 → 14
```

볼류메트릭 안개 격자는 레벨이 아니라 `unreal/Aquarium/Config/DefaultEngine.ini`의
`r.VolumetricFog.GridPixelSize=4` / `r.VolumetricFog.GridSizeZ=128` 두 줄이므로 이 줄을 직접
엔진 기본값(8 / 64)으로 바꿔 측정했다. 측정 절차는 `scripts/measure_m2b_perf.sh` 하나로 통일하고
(같은 실행 시간·같은 워밍업), 레벨을 다시 빌드한 뒤 측정한다. **실행 간 노이즈 바닥이 약 1.3 fps**이므로
그보다 작은 차이는 항목의 비용으로 읽지 않는다. 결과는
[`docs/reviews/2026-09-21-m4b-perf.md`](reviews/2026-09-21-m4b-perf.md).

**9단계 비고(M4c).** M4c의 세 항목(무리 행동·소품 회피·수직 롤 제거)은 **전부 시간축 현상이라
스틸에 나타나지 않는다.** 그래서 이 스크립트에서 판정 대상은 클립 두 개이고, 장면 스틸과 비교 이미지는
M4b의 조명·안개·산호가 회귀하지 않았는지 확인하는 용도뿐이다. M4b 하네스에 있던 산호 근접 스틸 단계는
삭제했다 — M4c는 산호 메시도 틴트도 건드리지 않으므로 M4b 스틸을 다시 찍는 것에 불과하다.
산호초 클립은 20초로는 무리가 형성되기 전에 끝나 **45초**로 늘렸고, 수직 전환 클립은 12초 동안 헤딩이
수직 특이점을 여덟 번 지나도록 짠 별도 스크립트를 **60 fps**로 찍는다. `BEFORE` 기본값은 M4b 장면
스틸이며, 스크립트는 `BEFORE`가 `m4c` 산출물을 가리키면 그 자리에서 실패한다(M4b에서 실제로
M4b를 M4b와 비교한 사고가 있었다).

**`-AquariumAutoInput` 토큰 문법 — 반드시 이 형식이어야 한다.** 값은 쉼표로 나뉜 토큰 목록이고,
각 토큰은 **`<방향문자><초>`** 하나다.

- 방향문자: `R`(오른쪽) `L`(왼쪽) `U`(위) `D`(아래) `0`(아무 키도 안 누름). 대소문자는 무시한다.
- 초: 방향문자 **바로 뒤**에 오는 양수. `R3`, `U1.5`, `0 2`(공백은 허용, 트림된다).
- **콜론 구분자는 없다. 대각선 토큰도 없다.** `U:1.5`는 방향 `U`에 나머지 `":1.5"`라 `Atof`가 0이고,
  `UR1.5`는 방향 `U`에 나머지 `"R1.5"`라 역시 `Atof`가 0이다. 둘 다 **버려진다.**
- **파서는 알아볼 수 없는 토큰에 대해 경고만 찍고 조용히 무시한다**
  (`AquariumAutoInput: unknown direction in '…'` / `AquariumAutoInput: bad duration in '…'`).
  즉 오타 하나가 자동 입력 대본을 조용히 줄이거나 통째로 비운다. **그래도 캡처는 정상적으로 돌고
  그럴듯한 클립이 나온다** — M4c에서 계획서의 `U:1.5,UR:1.5,…`가 실제로 "입력이 하나도 없는"
  수직 전환 클립을 만들 뻔했다. 이 때문에 `render_m4c_compare.sh`는 실행 후 두 경고 문자열의 건수가
  모두 0인지 단언한다(`AUTOINPUT_UNKNOWN=0 AUTOINPUT_BAD_DURATION=0`). 새 대본을 쓸 때는
  기억이 아니라 `ADiverPlayerController::BuildAutoInputSteps`를 보고 쓴다.

**`-AquariumAutoClick` 토큰 문법 — 반드시 이 형식이어야 한다.** 값은 쉼표로 나뉜 토큰 목록이고,
각 토큰은 **`<초>@<nx>x<ny>`** 하나다.

- 구분자는 **`@` 하나와 `x` 하나**뿐이다. **콜론 형식은 없다.** `4.0:0.5x0.5`는 버려진다.
- `<초>`: 세션 시작 이후의 양수 초. 0 이하는 버려진다.
- `<nx>`, `<ny>`: 뷰포트 **너비·높이에 대한 비율 0..1**. 픽셀이 아니다. 범위를 벗어나면 버려진다.
- 토큰은 쓴 순서와 무관하게 **시각 순으로 발사**된다.
- 예약 클릭도 실제 마우스와 **같은 `HandleClickAt`** 을 통과한다. 그래서 캡처가 검증하는 경로가
  아이가 실제로 쓰는 경로다.
- **파서는 알아볼 수 없는 토큰에 대해 경고만 찍고 조용히 버린다.** 경고 문자열은 정확히 두 개다
  (`AquariumAutoClick: bad token '…'; entry ignored` /
  `AquariumAutoClick: coords out of range in '…'; entry ignored`).
  그래서 **플래그가 있으면 클릭이 0개여도 언제나** `AquariumAutoClick: armed %d clicks`를 찍는다.
  하네스는 경고 건수 0과 `armed N`이 요청한 개수와 같은지를 **반드시 단언해야 한다** —
  `-AquariumAutoInput`과 똑같이, 오타 하나가 "클릭이 하나도 발사되지 않은 그럴듯한 클립"을 만든다.
  새 대본은 기억이 아니라 `ADiverPlayerController::BuildAutoClicks`를 보고 쓴다.

**`-AquariumAutoDash=<초>`와 `AquariumCatchStat` — 난이도 계측의 유일한 기구.** 시나리오 181행이
"잡기가 실제로 어려운지는 테스트로 알 수 없다"고 못 박았으므로, 난이도 판정은 **실제 RHI 실행의
로그**로만 한다. `-AquariumAutoDash=<초>`는 그 초마다 실제 키와 같은 `HandleDashPressed()`를
부른다(자동 입력은 방향키만 대본으로 쓸 수 있고 스페이스는 쓸 수 없다). 플래그가 붙으면 언제나
`AquariumAutoDash: armed every N s`를 찍으므로 하네스는 그 줄을 단언한다.
`UCatchSubsystem`은 판정이 성립한 틱마다 한 줄을 남긴다:

```
AquariumCatchStat: t=63.70 outcome=catch stamped=17 catches=73 bumps=11 closing=0.30 threshold=0.47
```

`stamped`가 화면 구석의 숫자이고 `closing`/`threshold`가 난이도의 두 숫자다. **별명은 어느 필드에도
들어가지 않는다**(P-03). 측정 예(64초, `ReefM1`, 시드 1):

```bash
"$UE/Engine/Binaries/Mac/UnrealEditor" "$PROJ" ReefM1 -game -windowed -ResX=1280 -ResY=720 -ForceRes \
  -benchmark -fps=30 -seconds=64 -notexturestreaming -unattended -nosplash -stdout -FullStdOutLogOutput \
  -AquariumAutoNickname=테스트 -AquariumAssignmentSeed=1 \
  -AquariumAutoInput="R1,U1,L1,D1" -AquariumAutoDash=1.5 | grep AquariumCatchStat
```

2026-09-22 이 방법으로 잰 값: 방향키만(`R1,U1,L1,D1`, 돌진 없음) 64초 **0마리**, 같은 입력에
돌진을 섞으면 64초 **2~6마리**. 조정 전(`catchSpeedFraction=0.55`)에는 방향키만으로 **17마리**였다.

**`-AquariumClickLog=<csv 절대경로>`.** 클릭 시도마다 한 행을 남기고 EndPlay에서
`AquariumClickLog: wrote %d clicks to %s`를 찍는다. 열은
`index,time_s,ndc_x,ndc_y,hit_plane_x,hit,state_before`이며 **별명은 어느 열에도 들어가지 않는다**(P-03).
`-nullrhi`에는 게임 뷰포트가 없어 `DeprojectScreenPositionToWorld`가 돌지 않으므로 Automation 테스트는
`HandleClickRay`에서 시작한다. **이 CSV가 역투영 경로의 유일한 검증 수단이다.** 그래서
`render_m5_click.sh`는 행 수뿐 아니라 **적중과 비적중이 섞여 있는지**까지 단언한다 — 전부 비적중이면
역투영이 망가진 채로도 클립은 멀쩡해 보이고, 전부 적중이면 타원 판정이 너무 후하다는 뜻이다.
**물고기는 작고 움직이는 표적이라, 조준점은 기억으로 정하면 안 된다**: M5에서 계획서가 적어 둔
좌표 일곱 개는 전부 빈 물을 맞혔고(7발 0적중) 단언이 그것을 잡아냈다. 실제 좌표는 클릭을 격자로
뿌리는 프로브 캡처를 한 번 돌려 `hit_plane_x`가 실제 평면 값을 돌려준 점만 골라 정했다.

**`-AquariumNoFlee`.** 클릭 처리와 도망 층을 통째로 끈다(성능 귀속용).

**11단계 비고 — `EXTRA_ARGS`(M4c에서 추가).** `scripts/measure_m2b_perf.sh`에는 원래 엔진 명령줄에
인자를 덧붙일 수단이 없었다. M4c에서 `EXTRA_ARGS`를 추가해 같은 실행 시간·같은 워밍업·같은 스크립트로
항목별 토글을 측정할 수 있게 했다. 개발 전용 토글은 두 개다.

```bash
-AquariumNoSchooling   # 무리 행동(보이즈)을 끈다. BuildSortedNeighbors와 SchoolingSteer를 건너뛰고,
                       # 이웃 스냅샷은 UFishSchoolSubsystem::Neighbors() 안에서 지연 생성이므로 함께 생략된다
-AquariumNoPropAvoid   # 소품 회피 조향을 끈다
```

아무것도 주지 않은 상태가 출하 설정이며, 커밋된 레벨과 보고 수치는 언제나 그 상태다.
이 스크립트는 산출물 이름을 항상 `<날짜>-m2b-*`로 쓰므로 실행 후 CSV를 `<날짜>-m4c-frametimes.csv`로
옮긴다(M5는 `<날짜>-m5-frametimes.csv`). **실행 간 노이즈 바닥은 약 1.3 fps**이고, M4c 측정에서는 세 토글 차이가 전부 그 아래인 데다
부호까지 반대여서 **점추정이 아니라 상한으로만** 읽었다. 결과는
[`docs/reviews/2026-09-21-m4c-perf.md`](reviews/2026-09-21-m4c-perf.md).

**머티리얼 컴파일 검사의 한계.** Automation 테스트 `Aquarium.Content.PropMaterialsCompile`은
`-nullrhi`에 `FMaterialResource`가 없어 **셰이더 컴파일 오류를 검사하지 못한다**(로그에
`checked 51 texture samplers and 0 of 13 material resources`로 그대로 드러난다). 이 테스트가 실제로
막아 주는 것은 RHI와 무관한 **샘플러 타입·텍스처 압축 설정 불일치**(M4a에서 두 번 당한
"Sampler type is Linear Grayscale" 사고)뿐이다. 셰이더 컴파일 실패의 1차 방어선은 여전히
**실제 게임 실행 로그 grep**이며, `-nullrhi`가 아닌 실행이어야 한다.

```bash
"$UE/Engine/Binaries/Mac/UnrealEditor" "$PWD/unreal/Aquarium/Aquarium.uproject" ReefM1 \
  -game -windowed -ResX=1280 -ResY=720 -ForceRes -benchmark -fps=30 -seconds=12 \
  -notexturestreaming -unattended -nosplash -log -AquariumAssignmentSeed=1
grep -inE "Failed to compile Material|Sampler type|Default Material|WorldGridMaterial" \
  ~/Library/Logs/Aquarium/Aquarium.log    # 0건이어야 한다
```

게임 실행 로그는 `unreal/Aquarium/Saved/Logs`가 아니라 **`~/Library/Logs/Aquarium/Aquarium.log`**에
쌓인다(`-log`를 줘도 터미널에는 런처 잡음만 나온다).

주의: GUI 에디터를 강제 종료한 뒤 다음 실행이 "패키지 복구" 프롬프트에 걸리면 `unreal/Aquarium/Saved/Autosaves`를 지운다. MCP `CaptureViewport`로 뷰포트 이미지를 얻는 절차는 위 "Unreal 공식 MCP" 절 참고.

## 출처

- [Blender MCP 원본과 설치법](https://github.com/ahujasid/mcp-for-blender)
- [PyPI 배포본](https://pypi.org/project/mcp-for-blender/2.0.0/)
- [Codex MCP 설정](https://developers.openai.com/codex/mcp)
- [Epic Mac 개발 요구사항](https://dev.epicgames.com/documentation/en-us/unreal-engine/macos-development-requirements-for-unreal-engine)
- [Unreal 공식 MCP](https://dev.epicgames.com/documentation/unreal-engine/unreal-mcp-in-unreal-editor)
- [Apple Xcode 26.1.1 다운로드 목록](https://developer.apple.com/download/all/?q=Xcode%2026.1.1)
