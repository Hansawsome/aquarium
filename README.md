# Aquarium

초등학생이 산호초 바다에서 자기 물고기 한 마리를 조종하는 macOS 아쿠아리움 게임.

## 현재 상태

규칙 계층(C++17, 테스트 60개)과 Unreal 5.8 프로젝트(`unreal/Aquarium`, Automation 테스트 24개)가 있습니다. M1 수중 장면과 M2(한글 별명 입장 → 두 종 중 무작위 배정 → 앞쪽의 내 물고기 위 이름표 → 나가기)까지 구현했고 검토 영상은 `docs/reviews/`에 있습니다. M2 사용자 검토, 실제 한글 IME 타이핑 확인, M2b(군집 30~40마리·산호초), 방향키(M3)·클릭 도망(M4)은 아직입니다. 물고기 모델은 1차 실루엣이라 실사 기준에는 못 미칩니다.

## 문서

- [PRD — 제품 요구사항](docs/PRD.md)
- [SRS — 동작 및 검증 기준](docs/SRS.md)
- [설계 — 구성과 기술 결정](docs/superpowers/specs/2026-09-19-aquarium-design.md)
- [TASK — 진행 상태와 단계별 완료 기준](docs/TASK.md)
- [M1 설계](docs/superpowers/specs/2026-09-20-m1-underwater-scene-design.md) · [M1 구현 계획](docs/superpowers/plans/2026-09-20-m1-underwater-scene.md)
- [M2 설계](docs/superpowers/specs/2026-09-20-m2-session-design.md) · [M2 구현 계획](docs/superpowers/plans/2026-09-20-m2-session.md)
- [개발 환경 확인 결과](docs/ENVIRONMENT.md)
- [설치 및 MCP 연결 상태](docs/SETUP.md)
- [에셋 출처 기록](docs/ASSETS.md)
- [개발 에이전트 지침](AGENTS.md)

## 방향

Unreal Engine 5 + Blueprint를 중심으로 구성하고, 테스트할 게임 규칙은 C++로 분리합니다. Blender로 무료 에셋을 수정하거나 필요한 모델과 애니메이션을 제작합니다. macOS 단일 플랫폼이 출시 대상입니다. 태블릿 배포는 2026-09-20 논의로 범위에서 제외했습니다.

그래픽 목표는 실사에 가까운 수중 경험입니다. 실제 품질과 성능은 M5 맥북에서 실행·측정한 결과로 판단합니다.
