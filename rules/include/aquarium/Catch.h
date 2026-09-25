#pragma once
#include <cstddef>

#include "aquarium/SwimPlane.h"   // Vec3
#include "aquarium/Vec2.h"

namespace aquarium {

// 이 게임에서 "부딪혔다"가 무엇인지 정하는 유일한 곳.
//
// **왜 화면 좌표인가.** 물고기는 월드 X가 고정된 평면 위에서만 산다. 내 물고기는
// X=220, 배경 물고기는 X>=330이라 3차원에서는 **절대로 닿을 수 없다.** 평면 구조를
// 깨면 M3의 경계 보장과 M4c의 평면별 장애물 사전계산이 함께 무너진다. 그래서 접촉은
// 카메라에서 본 겹침으로 정의한다 -- 카메라가 고정이고 아이의 머릿속 모형이 2D라는
// 것은 Food.h의 깊이 비대칭에서 이미 내린 결정이고, Flee.h의 PickFrontmostHit가
// 클릭에 대해 **같은 기하**를 이미 쓰고 있다.

// 들이받히는 쪽. ClickTarget과 필드가 겹치는 것은 우연이 아니다 -- 클릭으로 맞힐 수
// 있는 놈은 몸으로도 맞힐 수 있어야 하기 때문이고, 엔진은 둘 다 같은 렌더 바운드에서
// 파생해 채운다.
struct RamTarget {
    float depth = 0.f;        // 월드 X
    Vec2 center;              // 공유 유영 프레임(x = 월드 Y, y = 월드 Z)
    Vec2 velocity;            // 같은 프레임, cm/s
    float halfWidth = 0.f;
    float halfHeight = 0.f;
    bool alreadyStamped = false;   // 관측용. 판정을 바꾸지 않는다(벌을 만들지 않는다)
};

// 들이받는 쪽(= 내 물고기).
struct Rammer {
    float depth = 0.f;
    Vec2 nose;                // 몸 중심이 아니라 **코끝**. NosePoint가 만든다
    Vec2 velocity;
    // **반드시 채운다.** 0으로 두면 잡기 문턱이 0이 되어 "느리게 표류해도 잡힌다"가
    // 되고, 그러면 난이도 테스트가 초록불인 채 아무것도 검증하지 않는다.
    float maxSpeed = 0.f;
};

struct CatchParams {
    // 잡기에 필요한 접근 속도. 플레이어 최대 속도의 비율이라 최대 속도를 조정하면
    // 난이도가 **같이** 따라온다(숫자를 두 군데 두지 않는다).
    //
    // **1.15인 이유는 실측이다(2026-09-22, 실제 RHI 64초 실행).** 0.55일 때는
    // 방향키만 흔드는 자동 입력(R1,U1,L1,D1 · 돌진 없음)이 64초에 17마리를 찍었다 --
    // 기술이 하나도 들어가지 않은 입력이 1분에 17~22마리다. 그러면 돌진 키가 할 일이
    // 없어지고 '숙련의 게임'이라는 축이 통째로 죽는다.
    //
    // 같은 실행에서 방향키만의 접근 속도는 **최대 0.44 화면단위/초**였고(중앙값 0.31),
    // 이는 문턱 환산 1.10에 해당한다. 그래서 1.15는 "방향키만으로는 넘지 못하는 값 중
    // 가장 낮은 값"이다 -- 더 올리면 돌진으로도 못 잡는 쪽(실측: 1.25 + 짧은 돌진에서
    // 64초에 0마리)으로 넘어가고, 시나리오는 아이가 반복 실패를 "어렵다"가 아니라
    // "고장 났다"로 읽는다고 경고한다.
    //
    // 1.15 + DashParams(burstDuration 0.9)에서의 실측: 방향키만 64초 **0마리**,
    // 돌진을 섞으면 64초 **2~6마리**. 즉 돌진이 잡기의 유일한 경로다.
    float catchSpeedFraction = 1.15f;
    // 코끝이 몸 반길이의 어디쯤인가. 1.0이면 정확히 주둥이 끝이라 판정이 너무 뾰족하다.
    float noseFraction = 0.9f;
    // 상대 타원을 이만큼 키워서 본다. 코끝은 점이고 실제 몸은 두께가 있기 때문이다.
    // **1.0 아래로 내리지 않는다** -- 클릭보다 어려워지는 순간 "닿았는데 안 됐다"가 된다.
    float graceScale = 1.15f;
};

enum class RamOutcome : int { Miss = 0, Bump = 1, Catch = 2 };

struct RamResult {
    RamOutcome outcome = RamOutcome::Miss;
    int targetIndex = -1;
    float closingSpeed = 0.f;   // 화면 단위/초. Bump/Catch일 때만 의미가 있다
};

// 카메라에서 본 좌표. 깊이로 나누는 원근 투영 하나뿐이고, 화면 비율이나 시야각은
// 들어오지 않는다 -- 겹쳤는지 아닌지는 그것들과 무관하기 때문이다.
inline Vec2 ToScreen(Vec2 shared, float depth, Vec3 camera) {
    const float d = depth - camera.x;
    if (d <= 1e-3f) return {0.f, 0.f};        // 카메라 뒤 또는 렌즈 위
    return {(shared.x - camera.y) / d, (shared.y - camera.z) / d};
}

// 방향 벡터는 깊이로 나누어도 방향이 바뀌지 않는다(균일 스케일). 속도의 **크기**만
// 화면 단위로 줄어든다.
inline Vec2 ToScreenVelocity(Vec2 velocity, float depth, Vec3 camera) {
    const float d = depth - camera.x;
    if (d <= 1e-3f) return {0.f, 0.f};
    return {velocity.x / d, velocity.y / d};
}

// 잡기에 필요한 접근 속도(화면 단위/초). 플레이어의 최대 속도와 깊이에서 **파생**한다.
inline float CatchThreshold(float playerMaxSpeed, float playerDepth, const CatchParams& p) {
    const float d = playerDepth <= 1e-3f ? 1.f : playerDepth;
    return p.catchSpeedFraction * playerMaxSpeed / d;
}

// 몸 중심과 진행 방향에서 코끝을 만든다. 정지 상태(방향 0)에서는 몸 중심 그대로다.
inline Vec2 NosePoint(Vec2 center, Vec2 heading, float halfLength, const CatchParams& p) {
    const Vec2 h = heading.Normalized();
    if (h.Length() <= 0.f) return center;
    return center + h * (halfLength * p.noseFraction);
}

// 판정에 문턱을 적용해 최종 결과를 만든다. 겹침 계산과 난이도 손잡이를 분리해 두면
// 난이도를 조정해도 기하 테스트가 흔들리지 않는다.
inline RamResult ResolveRam(RamResult r, float playerMaxSpeed, float playerDepth,
                            const CatchParams& p) {
    if (r.targetIndex < 0) { r.outcome = RamOutcome::Miss; return r; }
    r.outcome = (r.closingSpeed >= CatchThreshold(playerMaxSpeed, playerDepth, p))
        ? RamOutcome::Catch : RamOutcome::Bump;
    return r;
}

// 지금 이 순간의 판정. **시간 인자도 각도 인자도 없다** -- 지속 시간 조건과 각도
// 조건을 넣는 것이 구조적으로 불가능하다는 뜻이고, 그 둘을 넣지 않기로 한 결정이
// 주석이 아니라 시그니처로 지켜진다.
inline RamResult EvaluateRam(Vec3 camera, const Rammer& me, const RamTarget* targets,
                             std::size_t count, const CatchParams& p) {
    RamResult r;
    if (targets == nullptr || count == 0) return r;
    const Vec2 myScreen = ToScreen(me.nose, me.depth, camera);
    const Vec2 myVelScreen = ToScreenVelocity(me.velocity, me.depth, camera);
    const Vec2 myDir = myVelScreen.Normalized();

    int best = -1;
    float bestDepth = 0.f;
    for (std::size_t i = 0; i < count; ++i) {
        const RamTarget& t = targets[i];
        if (t.halfWidth <= 0.f || t.halfHeight <= 0.f) continue;
        const float d = t.depth - camera.x;
        if (d <= 1e-3f) continue;                       // 카메라 뒤
        const Vec2 ts = ToScreen(t.center, t.depth, camera);
        // 화면에서 본 반지름. 깊이로 나뉘므로 멀수록 작아진다 -- 보이는 그대로다.
        const float hw = (t.halfWidth / d) * p.graceScale;
        const float hh = (t.halfHeight / d) * p.graceScale;
        const float u = (myScreen.x - ts.x) / hw;
        const float v = (myScreen.y - ts.y) / hh;
        if (u * u + v * v > 1.f) continue;              // 타원이다: 물고기는 길고 얇다
        if (best < 0 || t.depth < bestDepth) {          // 앞엣놈 = 작은 평면 X
            best = static_cast<int>(i);
            bestDepth = t.depth;
        }
    }
    if (best < 0) return r;

    r.targetIndex = best;
    const RamTarget& hit = targets[static_cast<std::size_t>(best)];
    const Vec2 itsVelScreen = ToScreenVelocity(hit.velocity, hit.depth, camera);
    // **접근 속도 = (내 속도 - 상대 속도)를 내 진행 방향에 투영한 값.**
    // 접촉 순간에는 두 점이 겹쳐 '서로를 향한 방향'이 정의되지 않으므로 내 진행
    // 방향을 기준으로 삼는다. 그 결과가 정확히 시나리오의 그림이다: 달아나는 놈을
    // 같은 속도로 따라가면 0이 되어 안 잡히고, 더 빨라야 잡힌다.
    const Vec2 rel = myVelScreen - itsVelScreen;
    r.closingSpeed = rel.x * myDir.x + rel.y * myDir.y;
    return ResolveRam(r, me.maxSpeed, me.depth, p);
}

} // namespace aquarium
