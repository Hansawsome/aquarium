# Aquarium

초등학생이 산호초 바다에서 자기 물고기 한 마리를 조종하는 macOS 아쿠아리움 게임.

## 현재 상태

요구사항과 설계 초안을 작성한 단계입니다. 실행 가능한 게임, Unreal 프로젝트, 다운로드한 3D 에셋은 아직 없습니다. 테스트와 빌드는 아직 실행하지 않았습니다.

## 문서

- [PRD — 제품 요구사항](docs/PRD.md)
- [SRS — 동작 및 검증 기준](docs/SRS.md)
- [설계 — 구성과 기술 결정](docs/superpowers/specs/2026-09-19-aquarium-design.md)
- [TASK — 진행 상태와 단계별 완료 기준](docs/TASK.md)
- [개발 환경 확인 결과](docs/ENVIRONMENT.md)
- [설치 및 MCP 연결 상태](docs/SETUP.md)
- [에셋 출처 기록](docs/ASSETS.md)
- [개발 에이전트 지침](AGENTS.md)

## 방향

Unreal Engine 5 + Blueprint를 중심으로 구성하고, 테스트할 게임 규칙은 C++로 분리합니다. Blender로 무료 에셋을 수정하거나 필요한 모델과 애니메이션을 제작합니다. macOS가 첫 출시 대상이며 태블릿은 후속 단계입니다.

그래픽 목표는 실사에 가까운 수중 경험입니다. 실제 품질과 성능은 M5 맥북에서 실행·측정한 결과로 판단합니다.
