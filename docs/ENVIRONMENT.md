# 개발 환경 확인

확인일: 2026-09-19

| 항목 | 확인 결과 |
|---|---|
| 하드웨어 | Apple M5, CPU 10코어, GPU 10코어, 통합 메모리 24GB |
| 작업 경로 | `/Users/hans/dev/aquarium` |
| Git 원격 | `https://github.com/Hansawsome/aquarium` (공개 저장소) |
| 기존 파일 | 초기 커밋 `a5c902b`, `.gitkeep` |
| GitHub CLI | 설치됨, 네트워크 허용 환경에서 인증 및 저장소 조회 성공 |
| C++ 컴파일러 | Apple clang 21.0.0, arm64 |
| 선택된 개발자 경로 | `/Applications/Xcode.app/Contents/Developer` (2026-09-20 확인) |
| 정식 Xcode | **27.0 (27A266a)** 설치, macOS 27.0 SDK. Metal Toolchain 27A266a는 2026-09-20 `xcodebuild -downloadComponent MetalToolchain`으로 추가 설치, `xcrun metal --version` 동작 확인. **주의: Epic 문서상 UE 5.8 권장은 26.1.1, 최소 26.0이며 27.x는 언급 없음(26.4는 명시적 비호환)** |
| Unreal Engine | 미설치. Epic 로그인 및 엔진 다운로드 필요 |
| Epic Games Launcher | Homebrew cask 20.1.4 설치, 첫 실행 업데이트 완료, 사용자 보고로 Epic 로그인 완료(2026-09-20). `LauncherInstalled.dat`의 설치 목록은 비어 있음 |
| Blender | 5.2.2 LTS, `/Applications/Blender.app`, 실행 확인 |
| Homebrew | `/opt/homebrew/bin/brew` |
| CMake | 4.4.3 설치 완료 |
| Git LFS | 3.8.0 설치, aquarium 저장소에 `git lfs install --local` 적용 |
| Superpowers | using-superpowers, brainstorming, writing-plans, test-driven-development, verification-before-completion, systematic-debugging, executing-plans 설치 |
| Blender MCP | `mcp-for-blender==2.0.0` 설치 및 Codex 등록, MCP 초기화·31개 도구 조회·장면 조회·Python 실행 성공 |
| Unreal MCP | UE 5.8의 공식 내장 플러그인 사용 예정. 아직 미연결 |

## 2026-09-19 설치 작업

사용자가 Unreal·Blender·정식 Xcode 설치와 MCP 연결을 명시적으로 승인했다. 제품 설계 문서의 후속 검토와 별개로 개발 도구 준비를 진행한다.

설치 및 검증 상세: [SETUP.md](SETUP.md).

## 후속 준비

1. Unreal 안정 버전과 해당 버전의 Xcode 호환성을 확인하고 함께 고정한다.
2. Unreal·Blender 및 필요한 개발 도구를 설치한다. 계정 로그인 등이 요구되면 사용자 참여가 필요할 수 있다.
3. 빈 Unreal 프로젝트의 에디터 실행·C++ 컴파일·macOS 패키징을 확인한다.
4. 실제 사용한 버전과 실행 명령을 이 문서에 갱신한다.

공식 요구사항은 엔진 버전에 따라 달라진다. 최신 페이지 기준만으로 설치된 도구의 호환성을 추정하지 않는다.

참고: [Epic의 macOS 개발 요구사항](https://dev.epicgames.com/documentation/en-us/unreal-engine/macos-development-requirements-for-unreal-engine).
