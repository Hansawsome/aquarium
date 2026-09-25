#pragma once
#include <cstdint>

#include "aquarium/Reaction.h"   // ReactionHash
#include "aquarium/Vec2.h"

namespace aquarium {

// 시나리오: "놈이 **먼저 눈치채고** 옆으로 튄다." 이 헤더가 잡기를 조준이 아니라
// 추격으로 만든다. Flee(클릭에 놀람)와는 다른 사건이다 -- 회피는 아이가 아무것도
// 누르지 않아도 일어나고, 놀람보다 약하다(클릭은 몰이 도구로 남아야 한다).

// 다가오는 쪽을 화면 좌표로 본 것. 깊이 정규화는 Catch.h가 이미 했다.
struct Approach {
    Vec2 screenPos;
    Vec2 screenVel;
};

struct EvadeParams {
    // 눈치채는 화면 반경. 화면 절반 너비가 대략 tan(75/2) = 0.767 화면 단위이므로
    // 0.13은 **화면 너비의 약 8.5%**다 -- 코앞이지 시야 전체가 아니다.
    float noticeRadius = 0.13f;
    // 이보다 느리게 다가오면 추격으로 치지 않는다. 화면 단위/초.
    // 플레이어 최대 속도 90cm/s를 깊이 220으로 나누면 0.41이므로, 0.06은 그 15%다.
    float minClosing = 0.06f;
    float duration = 0.45f;        // 옆으로 튀는 시간
    float dodgeSpeedScale = 1.7f;  // **놀람(2.2)보다 약하다.** 클릭이 더 센 도구로 남는다
};

// 눈치채는가. 두 조건이 **모두** 필요하다: 가깝고, 나를 향해 오고 있다.
// 옆으로 지나가는 것에까지 반응하면 바다 전체가 계속 파닥거려서 "내가 노린 놈이
// 반응했다"가 읽히지 않는다.
inline bool ShouldNotice(Vec2 myScreenPos, const Approach& a, const EvadeParams& p) {
    const Vec2 toMe = myScreenPos - a.screenPos;
    const float dist = toMe.Length();
    if (dist > p.noticeRadius) return false;
    const Vec2 dir = toMe.Normalized();
    if (dir.Length() <= 0.f) return true;            // 정확히 겹쳤다: 당연히 눈치챈다
    const float closing = a.screenVel.x * dir.x + a.screenVel.y * dir.y;
    return closing >= p.minClosing;
}

// 접근선의 수직 방향. 두 수직 중 **자기가 이미 가던 쪽**을 고른다 -- 관성이
// 자연스럽고, 무엇보다 예측 가능해서 아이가 몇 번 해 보면 각을 재게 된다.
// 그것이 이 마일스톤이 말하는 숙련이다.
inline Vec2 DodgeDirection(Vec2 approachDir, Vec2 myVelocity, std::uint32_t seed) {
    Vec2 a = approachDir.Normalized();
    if (a.Length() <= 0.f) a = {1.f, 0.f};           // 절대 0을 돌려주지 않는다
    const Vec2 perp{-a.y, a.x};
    const float lean = myVelocity.x * perp.x + myVelocity.y * perp.y;
    if (lean > 1e-4f) return perp;
    if (lean < -1e-4f) return perp * -1.f;
    // 관성이 없을 때만 해시로 고른다(결정적이다).
    return (ReactionHash(seed) & 1u) ? perp : perp * -1.f;
}

// 한 마리의 회피 상태. FleeStateMachine과 같은 모양이라 엔진 쪽 사용법이 같다.
class EvadeBehavior {
public:
    // 다시 눈치채면 **다시 겨누고 타이머를 새로 채운다.** 무시하지 않는다 --
    // F-11에서 배운 것과 같다(무시는 아이에게 '고장났다'로 읽힌다).
    void Notice(Vec2 approachDir, Vec2 myVelocity, std::uint32_t seed, const EvadeParams& p) {
        dir_ = DodgeDirection(approachDir, myVelocity, seed);
        timer_ = p.duration;
        ++noticeCount_;
    }

    void Step(float dt) {
        if (dt <= 0.f || timer_ <= 0.f) return;
        timer_ -= dt;
        if (timer_ < 0.f) timer_ = 0.f;
    }

    bool Active() const { return timer_ > 0.f; }
    Vec2 Direction() const { return dir_; }
    int NoticeCount() const { return noticeCount_; }
    float SpeedScale(const EvadeParams& p) const { return Active() ? p.dodgeSpeedScale : 1.f; }

private:
    Vec2 dir_{1.f, 0.f};
    float timer_ = 0.f;
    int noticeCount_ = 0;
};

} // namespace aquarium
