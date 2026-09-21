# M4c 설계 — 무리(보이즈) 행동, 소품 충돌, 수직 전환 롤 제거

작성일: 2026-09-21 · 상태: 사용자 위임, 결정은 이 문서에 기록

## 왜 이것이 다음인가

M4a는 물고기를, M4b는 산호·조명·후처리를 고쳤다. 둘 다 **정지 화면**의 문제였다. M4c에 남은 세
항목은 전부 **움직임**의 문제이고, 셋 다 이전 단계에서 "알려진 한계"로 적힌 채 넘어온 것이다.

- **배경 물고기 36마리가 서로를 모른다.** 각자 `WanderBehavior`로 독립된 무작위 목표를 향해
  간다. 그래서 화면은 "물고기 36마리"가 아니라 "따로 노는 점 36개"로 읽힌다. 실제 산호초에서
  같은 종은 느슨한 무리로 몰려다닌다. M2b에서 종·크기·시드를 흩뿌린 작업이 화면에서 군집으로
  보이지 않는 이유가 이것이다.
- **물고기가 산호와 바위를 그대로 통과한다.** M2b 이후 열려 있는 항목이다. 프롭 22개는
  바닥에 서 있고 물고기 평면은 그 위를 지나가므로, 캡처에서 물고기가 뇌 산호를 뚫고 나오는
  순간이 실제로 나온다. 한 번 보이면 그 뒤로는 계속 보인다.
- **수직으로 방향을 바꿀 때 약 0.3초짜리 롤이 돈다.** M2 시각 검토에 적힌 뒤 M2b·M3·M4a·M4b를
  그대로 통과했다. 이번에 원인을 특정했다(아래 "수직 롤 진단").

판정 방법은 M4a·M4b와 같다 — 같은 맵·같은 시드·같은 카메라·같은 `-AquariumAutoInput` 패턴·
같은 t. 다만 **이번 세 항목은 전부 시간 축의 현상이라 스틸로는 보이지 않는다.** 무리가
생겼는지, 물고기가 산호를 피해 도는지, 롤이 사라졌는지는 정지 화면에서 판단할 수 없다.
그래서 M4c는 **클립이 1차 산출물이고 스틸이 보조**다(검증 표에 그대로 적었다).

## 결정 사항 (아쿠부장 판단, 2026-09-21)

| 항목 | 결정 | 이유 |
|---|---|---|
| 보이즈의 위치 | **규칙 계층에 순수 함수로 넣는다.** `rules/include/aquarium/Boids.h`에 `SchoolingSteer(pos, depth, species, neighbors, count, params)` → 단위 조향 벡터. Catch2 테스트를 붙인다. Unreal 쪽은 이웃 목록을 공급하고 결과 벡터를 소비할 뿐이다 | `Steering`·`Wander`·`Bounds`·`Motion`이 전부 이 모양이다. 엔진 안에 행동을 넣으면 검증할 방법이 캡처를 눈으로 보는 것밖에 남지 않는다. 분리·정렬·응집은 위치와 속도만 있으면 계산되는 순수 함수이므로 엔진에 둘 이유가 애초에 없다 |
| 무리 계산 좌표계 | **공유 유영 좌표계 `(worldY, worldZ)`** 에서 계산하고, 평면 깊이(`worldX`)는 이웃 판정의 게이트로만 쓴다(`depthRadius = 120 cm`) | 배경 물고기는 X = 330~700의 **서로 다른 평면 36장**에 흩어져 있다. 그런데 모든 평면이 `right = +Y`, `up = +Z`로 같기 때문에, 평면 원점을 더해 준 좌표는 그대로 공통 좌표가 되고 그 안의 방향 벡터는 어떤 물고기든 변환 없이 쓸 수 있다. 깊이를 무시하면 화면에서 겹쳐 보이는 3 m 떨어진 두 마리가 서로를 끌어당겨, 옆에서 보면 말이 안 되는 무리가 생긴다 |
| 종별 무리 | **정렬·응집은 동종에만, 분리는 전 종에.** 종 판정은 `FishMesh` 에셋 이름에서 **파생**한다(새 프로퍼티를 만들어 배치 스크립트가 다시 적게 하지 않는다) | 블루탱은 블루탱을 따라가야 한다. 반대로 "다른 종은 아예 없는 것으로 친다"로 하면 클라운피시가 블루탱 몸을 관통한다. 따라가는 것(정렬·응집)과 비키는 것(분리)은 성격이 다르므로 적용 범위도 달라야 한다. 종 키를 액터 프로퍼티로 새로 두지 않는 이유는 M4b에서 프롭 반경 표를 두 군데 베껴 두었다가 검증이 결함을 한 번도 못 잡은 사고 그대로이기 때문이다 |
| 플레이어 물고기와 무리 | **플레이어 물고기는 절대 보이즈가 아니다**(무리 규칙을 전혀 받지 않는다). 반대로 배경 물고기에게 플레이어 물고기는 **분리 전용 이웃**(`avoidOnly`)이고 분리 반경은 일반의 2.4배(45 → 110 cm)다. 정렬·응집 대상은 절대 되지 않는다 | 방향키로 모는 물고기는 아이에게 완전히 예측 가능해야 한다. 여기에 무리 힘이 조금이라도 섞이면 "키를 눌렀는데 다르게 간다"가 되고, 이건 F-05의 위반이다. 반대 방향(배경 → 플레이어)은 선택지가 셋이었다: **끌어당김**은 아이가 보려는 바로 그 물고기를 배경 물고기가 둘러싸 가린다 — 탈락. **완전 무시**는 다른 종 몸을 관통하게 둔다는 뜻이고 위 결정과 모순이다 — 탈락. 남는 것이 **넓은 반경의 회피 전용**이고, 이건 화면에서 "내 물고기가 지나가면 무리가 갈라진다"로 읽혀 오히려 아이에게 보상이 된다 |
| 보이즈 비용 | 프레임당 **스냅샷 1회 + 전쌍(all-pairs)**. 공간 분할을 만들지 않는다. 이웃 상한 `maxNeighbors = 6`, 제곱거리 조기 탈출, 깊이 게이트 선검사 | n = 36이다. 36 × 35 = 1260쌍이고 쌍마다 뺄셈 2·곱셈 2·비교 몇 개다. 균일 격자를 만들면 격자 유지 비용이 그 1260번보다 크다. **측정 전에 자료구조를 만드는 것이 이 항목에서 가장 흔한 낭비**다. 실측에서 이 항목이 노이즈 바닥(약 1.3 fps) 위로 올라오면 그때 균일 격자를 넣는다 — 계획에 토글 플래그를 두는 이유가 그것이다 |
| 스냅샷 소유자 | 새 `UFishSchoolSubsystem`(`UWorldSubsystem`). 물고기가 `BeginPlay`/`EndPlay`에 등록·해제하고, 스냅샷은 **그 프레임에 처음 요청한 물고기가 만든다**(지연 재구성) | 틱 순서에 의존하지 않기 위해서다. `UTickableWorldSubsystem`으로 만들면 서브시스템 틱과 액터 틱의 순서가 보장되지 않아 한 프레임 묵은 데이터를 읽는 경우가 생긴다. 지연 재구성은 순서와 무관하게 "그 프레임의 스냅샷"을 보장하고, 등록 순서가 고정이라 결정성도 유지된다(`FishActor` 결정성 테스트가 이미 있다) |
| 소품 회피 방식 | **`SteerAlongBoundary`와 같은 모양.** `rules/include/aquarium/Obstacles.h`의 `SteerAroundObstacles`가 접촉 **전에** 옆으로 미끄러지게 하고, **원래 크기를 보존해 돌려준다**. 물리 응답(충돌체·반발)은 쓰지 않는다 | M3 설계에 기록된 교훈 그대로다 — 장애물 앞에서 속도 성분을 0으로 죽이면 속도가 0을 통과해 감속·재가속하고, 0을 지나는 속도에는 안정된 heading이 없어 방향이 한 스텝에 180° 뒤집혔다. `SteerAlongBoundary`가 원래 크기로 재정규화하는 이유가 그것이고, 같은 버그를 소품에서 다시 만들지 않는다. 물리를 쓰지 않는 이유는 별개다: 36 × 22 충돌체는 비싸고, 물리 반발은 접촉 **후** 반응이라 정의상 튐(jitter)을 만든다 |
| 소품 목록의 출처 | **런타임에 액터에서 파생한다.** `build_reef_m1.py`가 프롭에 액터 태그 `AquariumProp`을 붙이고, 서브시스템이 그 태그로 액터를 모아 `GetComponentsBoundingBox`에서 중심·반경을 계산한다. `PROP_HALF_EXTENTS` 표를 C++로 베끼지 않는다 | M4b의 잠복 버그 3번이 정확히 이것이었다 — 같은 규칙이 `build_reef_m1.py`와 `verify_scene.py` 양쪽에 있어서 검증이 결함을 한 번도 잡지 못했다. 프롭의 실제 반경은 메시 바운드 × 액터 스케일이고, 그건 배치된 액터가 이미 알고 있다. 표를 세 번째로 베끼는 순간 같은 사고가 예약된다 |
| 소품 → 원판 변환 | 프롭 하나를 **반경 = 수평 반폭, 중심을 Z를 따라 쌓은 원판 최대 4장**으로 근사한다. 물고기 평면 X와 `|prop.X - planeX| <= prop.extentX + 40 cm`로 먼저 거른다 | 원판 하나로 줄이면 선택지가 둘 다 나쁘다: 반경을 반폭으로 잡으면 키 큰 산호의 위아래가 비고, 반높이로 잡으면 폭 60 cm 산호가 반경 150 cm의 벽이 되어 물고기가 멀리서부터 피한다. 원판을 쌓으면 실루엣에 맞고 비용은 원판 몇 개 늘어나는 정도다. **평면 X는 물고기마다 고정**이므로 이 필터는 초기화 때 딱 한 번 돌고, 남는 원판은 보통 평면당 3~10개다. 즉 틱당 실제 비용은 물고기당 원판 몇 개 검사이고 22개 전수 검사는 아예 일어나지 않는다 |
| 플레이어 물고기와 소품 | **소품 회피는 플레이어 물고기에도 적용한다**(무리와 달리). 다만 실제로 걸릴 일이 거의 없다 | 무리와 결론이 갈리는 이유는 결함의 크기가 다르기 때문이다. 무리 힘은 아이가 누른 키와 다른 방향으로 물고기를 끌어 예측 가능성을 깨지만, 소품 회피는 **아이 자신의 물고기가 바위를 뚫고 지나가는 것**을 막는다. 화면에서 가장 크고 가장 오래 보이는 물고기가 관통하는 것은 이 장면에서 가능한 가장 눈에 띄는 결함이다. 게다가 프롭은 `PROP_CLEAR_RADIUS_Y` 레인 규칙 때문에 카메라 축에서 이미 밀려나 있어, 플레이어 평면(X=220, 반폭 130)과 겹치는 프롭은 사실상 없다 — 즉 이 규칙은 보험이고 평소에는 아무 일도 하지 않는다 |
| 규칙 적용 순서 | `Wander/입력` → **무리 혼합** → **소품 회피** → **경계 규칙** → `StepMotion` → `ClampToArea` | 벽이 항상 이긴다. 무리나 소품 회피가 물고기를 평면 밖으로 밀 수 있으면 M3에서 고정한 "이탈 없음"이 깨진다. 소품 회피가 경계보다 앞인 이유는, 벽 근처의 소품을 피하려다 벽으로 나가는 경우 경계 규칙이 마지막에 한 번 더 걸러 주기 때문이다 |
| 무리 혼합 비율 | `BlendSteering(wanderDir, schoolDir, 0.55)` — 무리 쪽이 조금 더 세다. 1.0(무리만)으로 하지 않는다 | 1.0이면 한 종 전체가 한 덩어리로 굳어 화면에서 "떼"가 아니라 "블록"이 된다. `Wander`를 절반 남기면 무리 안에서 개체가 계속 흔들려 느슨한 무리로 읽힌다. 이 값은 캡처를 보고 조정할 손잡이이며, 클립을 보고 조정한다 |
| 수직 롤의 원인 | **세기·슬루 속도 문제가 아니라 좌표 프레임의 위상(topology) 문제다.** 아래 "수직 롤 진단"에 근거와 수치를 적었다. 결론: 수직을 사이에 둔 두 heading의 목표 프레임이 **전방축에 대한 180° 비틀림(twist)** 만큼 차이 나고, `QInterpConstantTo`가 그 180°를 `MaxFacingTurnRate`(540°/s)로 소비해 **180 / 540 = 0.333초**가 걸린다. 기록된 증상 "0.3초"와 일치한다 | M4b의 빛줄기와 같은 방식으로 접근했다 — 뻔한 설명(슬루가 느리다)을 채택하지 않고 실제 프레임 계산을 따라갔다. 슬루를 빠르게 하는 "고침"은 롤을 더 빠르게 돌릴 뿐 없애지 못하고, 대신 벽 반전 시 방향 튐을 되살린다 |
| 수직 롤의 수정 | **회전을 swing(코끝 겨냥)과 twist(몸통 축 회전)로 분해해 각각 다른 속도로 제한한다.** 새 규칙 계층 헤더 `Facing.h`가 `MaxSwingStepDeg`와 `MaxTwistStepDeg`를 순수 함수로 준다. twist 속도는 heading이 가파를수록(`\|forward.z\|`가 1에 가까울수록) 빨라진다: 완만할 때 540°/s, 가파를 때 2880°/s, 그 사이는 smoothstep | 이 180°는 **없앨 수 없다.** 평면에 묶인 물고기가 모든 heading에서 등지느러미를 위로 두려면 수직 두 점에서 프레임이 불연속일 수밖에 없다(위상적 사실이다). 그래서 "없애기"가 아니라 **"안 보이는 곳에서 치르기"** 가 옳은 목표다. heading이 수직에 가까우면 물고기는 고정된 수평 카메라에 거의 정면(끝단)으로 서 있고, 그 자세에서의 축 회전은 좌우 대칭인 몸을 좌우로 뒤집을 뿐이라 시각적으로 거의 차이가 없다. 2880°/s면 180°가 **62 ms(60 fps에서 약 4프레임)** 에 끝난다 |
| `FacingIsContinuous`의 재작성 | 이 테스트는 **고쳐 쓴다**. 지금은 스텝당 쿼터니언 각거리 하나를 45°로 묶는데, swing/twist 분리 후에는 twist가 그 한도를 의도적으로 넘는다. 새 단언은 두 개다: (1) **전방축의 각변화 ≤ `MaxSwingStepDeg`**(지금은 아예 검사하지 않는 항목이다), (2) **twist ≤ 그 스텝의 `MaxTwistStepDeg`**, 둘 다 규칙 계층 함수에서 **파생해** 계산한다 | 테스트를 통과시키려고 한도를 늘리는 것이 아니다. 지금 한도는 swing과 twist를 뭉뚱그린 느슨한 값이고, 새 단언은 **보고 싶은 축(코끝의 움직임)에 대해 오히려 더 엄격하다.** 기대값을 규칙 함수에서 파생하는 이유는 위의 "베끼지 말고 파생하라"와 같다 |
| `UpVectorStaysUpright`·`BoneAnglesAreContinuous` | **손대지 않는다.** 둘 다 그대로 통과해야 한다 | 진단 결과 이 180°는 **거의 수평인 up 벡터를 유지한 채** 도는 회전이라(아래 참조) `UpVectorStaysUpright`가 애초에 이 결함을 잡지 못하고 있었다. 수정 후에도 잡을 일이 없어야 정상이다 — 다만 twist가 완만한 heading까지 새어 나오면 이 테스트가 빨간불이 되므로, **이 테스트는 이제 수정의 안전망 역할을 한다.** 몸통 굽힘 입력은 전체 각거리가 아니라 **swing 각도**에서 뽑도록 바꾼다(비틀림은 몸을 굽힐 이유가 없다). 그 결과 `BoneAnglesAreContinuous`의 기존 한도는 그대로 성립한다 |
| 성능 | 보이즈·소품 회피 각각을 끄는 **개발 전용 플래그 2개**(`-AquariumNoSchooling`, `-AquariumNoPropAvoid`, `#if !UE_BUILD_SHIPPING`)를 만들고 M4b와 같은 항목별 토글 실측을 한다. 기준선은 M4b(평균 63.8 fps / p95 16.58 ms). **실행 간 노이즈는 약 1.3 fps이므로 1.3 fps 미만 차이는 신호로 취급하지 않는다** | M4b에서 "반투명 커튼이 비쌀 것"이라는 설계 단계 예측이 측정으로 반증되었다. 예측이 아니라 토글로 재는 방식만 신뢰한다. 노이즈 바닥을 문서에 못 박는 이유는, 여유가 6 %밖에 없는 상태에서 0.8 fps 차이를 보고 기능을 자르는 실수를 막기 위해서다 |
| 예산 초과 시 | 평균이 60 fps 미만이면 **아래 컷 목록 순서대로** 되돌리고 되돌릴 때마다 재측정한다. `GridSizeZ` 128 → 64(+17.1 fps) 레버는 **컷 목록의 마지막**이고 기본으로 쓰지 않는다 | M4b가 일부러 남겨 둔 레버다. 먼저 쓰면 M4c가 실제로 얼마나 비싼지 영원히 알 수 없게 되고, M5·M6이 쓸 예산도 사라진다. 화질을 깎는 것은 알고리즘을 고친 뒤의 마지막 수단이다 |
| 자동 테스트 | 규칙 계층 66 → **90**(보이즈 10, 소품 8, Facing 6). Unreal Automation 38 → **47**. 모든 신규 테스트는 **일부러 빨간불이 켜지는 것을 확인한 뒤** 초록으로 만든다 | M4b의 `PropMaterialsCompile`이 38/38 Success를 보고하면서 `checked 0 of 13`을 찍고 있었다. "통과했다"는 "검사했다"가 아니다. 이번 계획은 태스크마다 RED 확인 단계를 명령과 기대 출력까지 적는다 |

### 컷 목록 (평균 60 fps 미만일 때 이 순서로 되돌린다)

1. `BoidsParams::maxNeighbors` 6 → 3, `neighborRadius` 140 → 100 cm
2. 무리 스냅샷을 2프레임에 1회만 재구성(위치 외삽 없이 묵은 값 사용)
3. 프롭당 원판 상한 4 → 2
4. 무리 행동을 카메라에서 먼 평면(X > 550)의 물고기에서 끔
5. `r.VolumetricFog.GridSizeZ` 128 → 64 — **M4b가 남겨 둔 예비 레버, 마지막에만 쓴다**

## 수직 롤 진단

**증상:** M2 이후 "수직으로 방향을 바꿀 때 짧은 롤(0.3초)".

**추적한 경로:** `AFishActor::StepSwim`은 목표 자세를
`FRotationMatrix::MakeFromXZ(Fwd, FVector::UpVector)`로 만든다. 유영 평면은 `right = +Y`,
`up = +Z`이므로 `Fwd`는 항상 YZ 평면 안에 있다. 거의 수직인 두 heading을 넣어 보면:

- `Fwd = (0, +ε, 1)` → 직교화된 local Z ≈ `(0, -1, 0)`
- `Fwd = (0, -ε, 1)` → 직교화된 local Z ≈ `(0, +1, 0)`

즉 **가로 성분의 부호가 바뀌는 순간 local Z가 뒤집힌다.** 두 프레임 사이의 최소 회전은
**월드 +Z(= 거의 전방축)에 대한 180°** 다. `QInterpConstantTo`는 이것을
`MaxFacingTurnRate = 540°/s`로 소비하므로 **180 / 540 = 0.333초.** 기록된 "0.3초"와 맞는다.

**세 가지 확인 사항:**

1. **0.9995 수직 밴드는 원인도 해결책도 아니다.** 밴드 폭은 약 1.8°라서 불연속을 1.8° 뒤로
   미룰 뿐, 밴드를 빠져나오는 순간 목표 프레임은 이미 뒤집혀 있다.
2. **`UpVectorStaysUpright`가 왜 지금도 통과하는가** — 이 180°의 회전축이 거의 수직이기 때문에
   up 벡터는 `(0,-1,0) → (±1,0,0) → (0,1,0)`로 **수평면 안에서** 돈다. `up.Z`는 내내 0 근처이고
   테스트 허용치 -0.05를 넘지 않는다. 즉 **이 테스트는 이 결함을 잡도록 만들어진 적이 없다.**
   이것은 M4b의 "통과했지만 아무것도 검사하지 않던 테스트"와 같은 부류이며, M4c에서 전용 테스트를
   새로 만드는 이유다.
3. **관찰자에게 왜 "롤"로 보이는가** — 회전축이 수직이고 물고기의 장축도 거의 수직이다. 월드
   축으로는 요(yaw)지만, **물고기 자신의 몸축으로는 롤**이다. 그래서 눈에는 제자리 회전(피루엣)으로
   보인다.

**따라서 고칠 대상은 "회전이 느리다"가 아니라 "이 180°를 언제 치르는가"다.** swing/twist 분해로
180° 전체를 twist 성분으로 정확히 분리할 수 있고, heading이 가파른 구간(전방축이 수직에 가까워
물고기가 카메라에 끝단으로 서 있는 구간)에서 2880°/s로 태워 버리면 62 ms 만에 끝난다.

**반증 절차(계획 Task 4).** 위 설명이 맞다면, heading을 수직 아래에서 위로 쓸어 넘기는 전용
자동화 테스트에서 **twist 누적이 180°에 가깝게 측정되고, 그 소요 시간이 약 0.33초** 여야 한다.
계획은 이 테스트를 **수정 전에 먼저 돌려 실제 숫자를 기록**하게 한다. 숫자가 다르면 설명이 틀린
것이므로 그 자리에서 멈추고 다시 진단한다.

## 검토한 대안

- **보이즈를 Unreal `AController`/`Blueprint`에 두기** — 테스트 수단이 캡처밖에 남지 않는다. 이
  프로젝트가 규칙 계층을 분리한 이유 전체와 충돌한다.
- **소품 충돌을 UE 물리(콜리전 + 블로킹)로 처리** — 36 × 22개의 충돌 질의를 매 틱 돌리는 비용도
  문제지만, 더 큰 문제는 물리 반발이 정의상 접촉 **후** 반응이라 튐을 만든다는 점이다. 화면에서
  물고기가 산호에 부딪혀 튀는 것은 "통과하는 것"보다 나쁘다.
- **장애물 앞에서 진행 방향 성분을 0으로** — M3에서 이미 당한 버그다. 속도가 0을 통과하면
  heading이 뒤집힌다. `SteerAroundObstacles`는 항상 같은 크기의 벡터를 돌려준다.
- **수직 밴드를 0.9995에서 0.94(약 20°)로 넓히기** — 롤을 없애지 못하고 위치만 옮긴다. 밴드를
  나오는 지점에서 같은 180°를 똑같이 치른다.
- **평면 법선을 가로축에 고정해 프레임을 만들기** — 코드 주석에 이미 근거가 있다. 이렇게 하면
  heading 부호에 따라 local Z가 뒤집혀 **절반의 heading에서 물고기가 배를 위로 하고 헤엄친다.**
  수직 두 점의 불연속이 heading 절반의 상시 결함으로 바뀌는 것이라 명백히 더 나쁘다.
- **desired direction의 피치를 ±70° 등으로 제한해 수직에 못 가게 하기** — 규칙 계층 한 줄로
  끝나 매력적이지만 두 가지로 탈락한다. (1) 위로 가는 heading과 아래로 가는 heading 사이를
  이동하려면 어차피 수직을 지나야 하므로 불연속을 없애지 못한다. (2) 위 방향키를 누른 아이의
  물고기가 비스듬히 간다 — F-05·F-07의 예측 가능성을 정면으로 깬다.
- **가장 가까운 쪽 프레임(뒤집힌 쌍둥이)을 목표로 골라 비틀림을 아예 만들지 않기** — 수직 교차
  자체는 매끄러워지지만, 그 대가로 물고기가 배를 위로 한 채 수평 비행을 계속하게 된다. 결국
  어딘가에서 180°를 치러야 하고, 위상적으로 피할 수 없다.
- **보이즈용 균일 격자/공간 해시** — n = 36에서 격자 유지 비용이 전쌍 검사보다 크다. 측정 후
  필요해지면 넣는다(컷 목록 밖의 별도 최적화 항목).
- **무리를 Niagara나 애니메이션으로 흉내내기** — 배경 물고기는 이미 개별 액터이고 규칙 계층이
  구동한다. 별도 시스템을 얹을 이유가 없다.

## 구성 요소

### `rules/include/aquarium/Boids.h` (신규)

- `struct BoidNeighbor { Vec2 position; Vec2 velocity; float depth; int species; bool avoidOnly; }`
  — 위치·속도는 **공유 유영 좌표계**`(worldY, worldZ)`, `depth`는 월드 X.
- `struct BoidsParams` — `neighborRadius 140`, `depthRadius 120`, `separationRadius 45`,
  `avoidOnlyRadius 110`, `separationWeight 1.7`, `alignmentWeight 0.6`, `cohesionWeight 0.35`,
  `maxNeighbors 6`.
- `struct BoidsResult { Vec2 steer; int consideredCount; int avoidCount; }`.
- `bool IsSchoolMate(int selfSpecies, const BoidNeighbor&)`.
- `BoidsResult SchoolingSteer(Vec2 pos, float depth, int species, const BoidNeighbor*, size_t, const BoidsParams&)`.
- `Vec2 BlendSteering(Vec2 ownDir, Vec2 schoolDir, float weight)` — 항상 단위 벡터(또는 영벡터)를
  돌려주므로 호출자가 속력을 건드릴 일이 없다.

### `rules/include/aquarium/Obstacles.h` (신규)

- `struct Obstacle { Vec2 center; float radius; }` — 한 유영 평면과 만나는 소품의 단면 원판.
  평면 국소 좌표계다.
- `struct ObstacleParams { float lookAhead = 150.f; float margin = 20.f; }`.
- `Vec2 SteerAroundObstacles(Vec2 pos, Vec2 dir, const Obstacle*, size_t, const ObstacleParams&)`
  — **입력 크기를 반드시 보존한다.** 정면(lateral == 0)일 때는 결정적으로 왼쪽을 고른다.

### `rules/include/aquarium/Facing.h` (신규)

- `struct FacingParams { maxTurnRateDegPerSec 540; uprightRollRateDegPerSec 540; steepRollRateDegPerSec 2880; steepBeginSin 0.70; }`
- `float Steepness(float verticalSin, const FacingParams&)` — smoothstep, 0..1.
- `float MaxSwingStepDeg(const FacingParams&, float dt)`
- `float MaxTwistStepDeg(float verticalSin, const FacingParams&, float dt)`

### `unreal/Aquarium/Source/Aquarium/FishSchoolSubsystem.{h,cpp}` (신규)

- `UFishSchoolSubsystem : public UWorldSubsystem`.
- `Register/Unregister(AFishActor*)`, `RegisteredCount()`.
- `const std::vector<aquarium::BoidNeighbor>& Neighbors()` — 프레임당 1회 지연 재구성
  (`GFrameCounter` 비교).
- `void BuildObstaclesForPlane(const FVector& PlaneOrigin, float PlaneHalfDepth, std::vector<aquarium::Obstacle>& Out)`
  — 태그 `AquariumProp` 액터의 **실제 바운드**에서 원판을 쌓아 평면 국소 좌표로 돌려준다.
- `bSchoolingEnabled` / `bPropAvoidanceEnabled` — `#if !UE_BUILD_SHIPPING`에서 명령줄로 끈다.
- `static bool ParseDisableFlag(const TCHAR* CmdLine, const TCHAR* Flag)` — 순수 함수, 테스트 대상.

### `unreal/Aquarium/Source/Aquarium/FishActor.{h,cpp}` (수정)

- `StepSwim`이 `Wander/입력 → 무리 혼합 → 소품 회피 → 경계 → StepMotion → ClampToArea → 월드 변환 → 자세 → ApplyBodyWave` 순서가 된다.
- 자세 단계가 `QInterpConstantTo` 하나에서 **swing/twist 분해 + 개별 제한**으로 바뀐다.
- 몸통 굽힘 입력이 전체 각거리가 아니라 **swing 각도**에서 나온다.
- `SpeciesKey`를 `FishMesh` 이름에서 파생해 `InitializeSwim`에서 캐시한다.
- 평면 국소 장애물 목록을 `InitializeSwim`에서 **한 번** 만든다(평면 X가 고정이므로).
- 새 UPROPERTY: `SteepRollRate`(2880), `SteepBeginSin`(0.70), `SchoolWeight`(0.55).

### `unreal/Aquarium/Scripts/build_reef_m1.py` (수정)

- 프롭 액터에 `prop.tags = [unreal.Name("AquariumProp")]` 추가. 그 외 배치 로직·시드는
  **바꾸지 않는다**(전/후 비교가 성립해야 한다).
- `REEF_OK` 줄에 `tagged=<n>`을 추가한다.

### `unreal/Aquarium/Scripts/verify_scene.py` (수정)

- 태그 `AquariumProp`을 가진 액터 수가 프롭 수와 같은지 단언. 반경은 계속 실제 메시 바운드에서
  계산한다(표를 베끼지 않는다).

### Unreal Automation (C++, 9개 추가 → 47개)

`FacingHasNoLongTwistAcrossVertical`, `SchoolMatesPullTogether`, `OtherSpeciesDoNotPull`,
`PlayerFishIsAvoidedNotFollowed`, `PlayerFishIgnoresSchooling`, `ObstaclesDerivedFromPropBounds`,
`SwimsAroundPropInsteadOfThrough`, `SpeedSurvivesPropAvoidance`, `DevTogglesParse`.
기존 `FacingIsContinuous`는 위 결정대로 재작성한다.

### `scripts/render_m4c_compare.sh` (신규)

`render_m4b_compare.sh`와 같은 구조. 다른 점 네 가지:
1. **클립이 1차 산출물이다.** 45초(`-m4c-reef.mp4`) — 무리 형성·소품 회피·수직 전환이 전부
   시간 축 현상이라 20초로는 사건이 충분히 담기지 않는다.
2. **수직 전환 전용 클립** `-m4c-vertical.mp4`(12초)를 `-AquariumAutoInput`이 위·위오른쪽·
   위왼쪽을 반복하도록 짠 패턴으로 따로 찍는다. 롤 결함은 이 전환에서만 나온다.
3. `BEFORE` 기본값이 `docs/reviews/2026-09-21-m4b-scene.png`.
4. 실행 후 **게임 로그에서 `Failed to compile Material`이 0인지 단언**하고 아니면 비-0으로 종료.

## 검증

| 계층 | 검증 |
|---|---|
| 규칙 계층 | **66 → 90개 통과.** 보이즈 10(분리·정렬·응집 각각, 동종만 따라감, `avoidOnly`는 정렬·응집에서 제외, 깊이 게이트, 이웃 상한, 이웃 0, 결정성), 소품 8(크기 보존, 장애물 없음/뒤/옆, 정면 결정성, 원판 안, 반경 0, lookAhead 밖), Facing 6(dt<=0, 수평, 수직, 단조, 부호 무관, `steepBeginSin` 경계). `-Wall -Wextra -Wshadow` 경고 0 |
| RED 증명 | **모든 신규 테스트가 빨간불이 켜지는 것을 먼저 본다.** 규칙 테스트는 헤더 부재 컴파일 실패 → 스텁 실패 → 구현 통과. Automation 신규 9개는 구현 전에 돌려 실패 메시지와 **측정된 수치**(특히 twist 180° / 0.33초)를 계획에 기록 |
| Unreal | **38 → 47개 Success.** `FacingIsContinuous`는 재작성 후에도 통과하고, 새 단언(전방축 swing 한도)이 실제로 걸리는지 일부러 `SteepRollRate`를 `MaxSwingStepDeg` 경로에 잘못 물려 빨간불을 한 번 확인한다. `UpVectorStaysUpright`와 `BoneAnglesAreContinuous`는 **수정 없이** 통과 |
| 씬 계약 | `verify_scene.py`의 `SCENE_OK`가 `AquariumProp` 태그 22개를 보고하고, 프롭 수와 일치 |
| 머티리얼 컴파일 | M4c는 머티리얼을 건드리지 않지만 회귀 방어선은 유지한다 — 캡처 실행 뒤 `~/Library/Logs/Aquarium/Aquarium.log`에 `Failed to compile Material` **0회**를 grep으로 단언 |
| 성능 | `scripts/measure_m2b_perf.sh` 그대로 → `docs/reviews/<날짜>-m4c-perf.md`. M4b 기준선(평균 63.8 fps / p95 16.58 ms) 대비. **항목별 토글**(무리 끔 / 소품 회피 끔 / 둘 다 끔) 4회 측정. **1.3 fps 미만 차이는 노이즈로 기록하고 신호로 해석하지 않는다.** 평균 60 fps 미만이면 컷 목록 순서대로 되돌리고 되돌릴 때마다 재측정 |
| 시각 — **클립이 1차** | `-m4c-reef.mp4`(45초)와 `-m4c-vertical.mp4`(12초)를 **직접 재생해 보고** 판정한다. 스틸은 보조다 — M4c의 세 항목 중 **정지 화면에 나타나는 것은 하나도 없다.** 그래도 회귀 확인용으로 `-m4c-scene.png`를 M4b 스틸과 `hstack`해 `-compare.png`를 만든다(무리·회피가 조명·산호를 망가뜨리지 않았는지) |

판정 기준(클립을 볼 때 확인할 것):
1. 같은 종 물고기가 느슨한 무리로 몰려다니는가 — 한 덩어리로 굳지도, 완전히 따로 놀지도 않는가
2. 물고기가 산호·바위를 관통하지 않고, 앞에서 **미리** 돌아가는가 (부딪힌 뒤 튕기는 것이 아니라)
3. 소품을 피할 때 속력이 죽거나 방향이 뒤집히지 않는가 (M3 버그의 재발 여부)
4. 수직으로 방향을 바꿀 때 제자리 회전이 보이지 않는가
5. 배경 물고기가 플레이어 물고기를 둘러싸 가리지 않고, 오히려 길을 비키는가
6. M4b의 빛줄기·안개·산호 색이 그대로인가 (회귀 없음)

## 데이터·오류 처리

- `SchoolingSteer`와 `SteerAroundObstacles`는 **이웃/장애물이 0개이거나 포인터가 null이어도 정의된
  값을 돌려준다**(각각 영벡터, 입력 그대로). 예외를 던지지 않는다 — 매 틱 호출되는 경로다.
- `BlendSteering`은 두 방향이 정확히 반대여서 합이 0이 될 때 **물고기 자신의 의도(`ownDir`)를
  유지한다.** 영벡터를 돌려주면 `StepMotion`이 감속시키고, 감속한 속도가 0을 통과하면 M3 버그가
  재현된다.
- `SteerAroundObstacles`는 **모든 반환 경로에서 입력 크기를 보존한다.** 이 성질은 Catch2 테스트
  하나가 전담해 지킨다(계획 Task 2, `magnitude preserved on every path`).
- 물고기가 이미 소품 원판 **안에** 들어가 있으면(레벨 편집으로 겹쳤거나 스케일이 바뀐 경우)
  바깥으로 밀어내되, 역시 크기를 보존한 방향 회전으로만 한다. 순간이동시키지 않는다.
- 태그 `AquariumProp` 액터가 0개면 소품 회피는 **조용히 비활성**된다(장애물 목록이 비어 있을 뿐).
  다만 `verify_scene.py`가 `SCENE_OK`에서 태그 수를 단언하므로 레벨 빌드 단계에서 잡힌다 —
  런타임에 조용히 죽는 경로를 만들지 않기 위해서다.
- `UFishSchoolSubsystem`이 없는 월드(순수 단위 테스트 월드)에서는 물고기가 무리·소품 회피를
  건너뛰고 M3와 동일하게 동작한다. 기존 38개 테스트가 그대로 통과해야 하는 이유이기도 하다.
- 스냅샷의 약참조(`TWeakObjectPtr`)가 죽은 항목은 재구성 때 목록에서 제거한다. `EndPlay`가
  호출되지 않는 경로(강제 종료)에서도 댕글링이 생기지 않는다.
- 종 키는 `FishMesh`의 `FName` 해시다. **프로세스 내 동등 비교에만 쓰고 저장하거나 로그에 남기지
  않는다.** 메시가 없는 물고기(테스트 스폰)는 키 0이고 서로 같은 종으로 취급된다 — 의도된 동작이며
  테스트에서 명시적으로 고정한다.
- 성능 측정은 M4b와 같이 **한 번에 하나씩만 끄고 잰다.** 두 개를 같이 끄면 무엇이 비쌌는지 모른다.

## 범위 밖

- 클릭 도망(F-09~F-13) — M5. 패키징 빌드 기준 성능 판정 — M6.
- **산호 형상 재작업**(TubeCoral 말뚝 울타리, FanCoral 실루엣, PlateCoral·BrainCoral 흰 덩어리,
  종별 틴트 딕셔너리) — 전부 M4b 이월 항목이다. M4c에서 산호를 다시 만들면 무리·회피가
  화면에서 어떻게 보이는지 판단할 수 없다.
- M4a 이월 항목(비늘 이방성 셀, 가슴지느러미 위치, 나비고기 `pecStray=16`, 고정 바운드의
  `BoundsScale`, 미사용 `SK_*_PhysicsAsset`).
- 물고기끼리의 **실제 충돌 해소**(겹침 금지). 분리(separation)는 겹침을 줄이지만 보장하지 않는다.
  보장하려면 반복 해소기가 필요하고 그건 물리이며, 이 장면에서 값어치가 없다.
- 소품의 **정확한** 충돌 형상(볼록 분해, 메시 충돌). 원판 스택 근사로 충분하다는 것이 이번 결정이고,
  부족하면 클립을 보고 다음 반복에서 다룬다.
- 물고기 애니메이션(꼬리 파동·굽힘) 자체의 변경. 굽힘 **입력원**만 swing으로 바꾼다.
- 수면 메시·물 굴절·흔들리는 해초.
