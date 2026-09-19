# macOS 개발 도구와 MCP 연결

상태 기준: 2026-09-20. 게임 코드와 제품 테스트는 아직 없다.

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

### Unreal Engine와 공식 MCP

Epic Launcher 설치와 첫 업데이트를 마쳤고, 2026-09-20 사용자가 Epic 로그인을 완료했다고 보고했다. 같은 날 `LauncherInstalled.dat`의 `InstallationList`는 비어 있어 엔진은 아직 미설치다. 다음은 Launcher에서 Unreal 5.8 안정 릴리스 선택과 macOS 개발 구성요소 설치다. Launcher 설치만으로 엔진 설치 완료로 기록하지 않는다.

엔진 설치 후 수행할 연결:

1. 설치된 엔진에서 `Unreal MCP` (`ModelContextProtocol`)와 `All Toolsets` 플러그인 지원을 확인한다.
2. 개발 프로젝트에서 플러그인을 활성화하고 에디터를 재시작한다.
3. 로컬 포트 충돌을 확인한 뒤 `ModelContextProtocol.StartServer`로 시작한다. 기본 주소는 `http://127.0.0.1:8000/mcp`다.
4. MCP 분석 전송은 `ModelContextProtocol.EnableAnalytics 0`으로 끈다.
5. 실행된 주소를 Codex에 등록하고 initialize → tools/list → 읽기 전용 장면 조회를 실제로 검증한다. 엔진 호출은 순차 수행한다.
6. 게임 배포물에서는 개발용 MCP 서버가 시작되지 않도록 구성·검증한다.

Unreal 공식 MCP는 실험적 기능이다. 엔진과 프로젝트가 준비되지 않아 아직 등록·연결 완료로 표시하지 않는다.

## 출처

- [Blender MCP 원본과 설치법](https://github.com/ahujasid/mcp-for-blender)
- [PyPI 배포본](https://pypi.org/project/mcp-for-blender/2.0.0/)
- [Codex MCP 설정](https://developers.openai.com/codex/mcp)
- [Epic Mac 개발 요구사항](https://dev.epicgames.com/documentation/en-us/unreal-engine/macos-development-requirements-for-unreal-engine)
- [Unreal 공식 MCP](https://dev.epicgames.com/documentation/unreal-engine/unreal-mcp-in-unreal-editor)
- [Apple Xcode 26.1.1 다운로드 목록](https://developer.apple.com/download/all/?q=Xcode%2026.1.1)
