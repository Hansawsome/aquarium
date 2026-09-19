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
| 선택된 개발자 경로 | `/Library/Developer/CommandLineTools` |
| 정식 Xcode | `/Applications`에서 확인되지 않음. CLI 도구만 선택된 상태 |
| Unreal / Epic Launcher | `/Applications`, `~/Applications`, `/Users/Shared` 기본 경로에서 확인되지 않음 |
| Blender | 기본 앱 경로 및 PATH에서 확인되지 않음 |
| Homebrew | `/opt/homebrew/bin/brew` |
| CMake | PATH에서 확인되지 않음 |
| Superpowers | using-superpowers, brainstorming, writing-plans, test-driven-development, verification-before-completion, systematic-debugging, executing-plans 설치 |
| 3D MCP | 현재 제공된 도구 목록에서 Blender/Unreal 전용 도구를 확인하지 못함 |

## 후속 준비

1. Unreal 안정 버전과 해당 버전의 Xcode 호환성을 확인하고 함께 고정한다.
2. Unreal·Blender 및 필요한 개발 도구를 설치한다. 계정 로그인 등이 요구되면 사용자 참여가 필요할 수 있다.
3. 빈 Unreal 프로젝트의 에디터 실행·C++ 컴파일·macOS 패키징을 확인한다.
4. 실제 사용한 버전과 실행 명령을 이 문서에 갱신한다.

공식 요구사항은 엔진 버전에 따라 달라진다. 최신 페이지 기준만으로 설치된 도구의 호환성을 추정하지 않는다.

참고: [Epic의 macOS 개발 요구사항](https://dev.epicgames.com/documentation/en-us/unreal-engine/macos-development-requirements-for-unreal-engine).
