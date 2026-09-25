#pragma once
#include <cstdint>

#include "aquarium/Flee.h"
#include "aquarium/Vec2.h"

namespace aquarium {

// 시나리오 장면 2 요구사항 2: "어떤 놈은 빙글 돌고, 어떤 놈은 삐끗 뒤집히고,
// 어떤 놈은 그 자리에서 파르르 떤다." 세 가지면 충분하다 -- 아이가 세는 것이
// 아니라 "매번 다르네"를 느끼면 되는 것이고, 여기에 재생별 음높이까지 겹치면
// 같은 반응이 두 번 오는 일이 사실상 없다.
enum class ReactionStyle : int { Dart = 0, Spin = 1, Tumble = 2 };

// 결정적 해시. Audio.h의 JitterPitch와 같은 splitmix32를 쓰되 상수를 달리해
// 같은 시드에서 스타일과 음높이가 붙어 다니지 않게 한다.
inline std::uint32_t ReactionHash(std::uint32_t seed) {
    std::uint32_t x = seed + 0x85EBCA6Bu;
    x ^= x >> 16; x *= 0x7FEB352Du;
    x ^= x >> 15; x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

inline ReactionStyle PickReactionStyle(std::uint32_t seed) {
    return static_cast<ReactionStyle>(static_cast<int>(ReactionHash(seed) % 3u));
}

// <cmath>를 끌어오지 않는 5차 사인 근사. |오차| < 0.001. Bubbles.h도 이것을 쓴다 --
// 같은 근사를 두 번 적지 않기 위해 클래스 밖에 둔다.
inline float PlayerReactionSin(float x) {
    const float twoPi = 6.2831853f;
    while (x > 3.14159265f) x -= twoPi;
    while (x < -3.14159265f) x += twoPi;
    const float x2 = x * x;
    return x * (1.f - x2 * (0.16666667f - x2 * (0.00833333f - x2 * 0.00019841f)));
}

// 한 번의 놀람이 몸으로 보이는 모양.
//
// **2026-09-21 사양 변경(대상 나이 7살 -> 초등 5~6학년)**: 과장의 방향이 뒤집혔다.
// "1.5배로 팍 부풀기"는 유아용 카툰 문법이라 폐기됐고, 이 나이대에는 그 신호가
// 게임을 끄는 이유가 된다. 그래서 여기에는 **크기 배율이 아예 없다** -- 과장은
// 전부 speedScale, 즉 "순간적으로 튀어 나가는 속도"로만 표현한다. 속도 배율은 FleeParams에서 **파생**한다 --
// 도망 속도를 두 군데 적지 않기 위해서다(규약 7).
struct StartleShape {
    float speedScale = 1.f;      // 최대 속도 배율
    float spinDegPerSec = 0.f;   // 제자리 회전(부호 있음). 0이면 회전 없음
    float wobbleDeg = 0.f;       // 파르르 떠는 진폭
};

inline StartleShape ShapeFor(ReactionStyle style, const FleeParams& p) {
    StartleShape s;
    switch (style) {
    case ReactionStyle::Dart:    // 그냥 쌩 달아난다. 가장 빠르고 몸짓은 떨림뿐.
        s.speedScale = p.fleeSpeedScale * 1.15f;
        s.spinDegPerSec = 0.f;
        s.wobbleDeg = 6.f;
        break;
    case ReactionStyle::Spin:    // 빙글 돌면서 달아난다.
        s.speedScale = p.fleeSpeedScale;
        s.spinDegPerSec = 520.f;
        s.wobbleDeg = 0.f;
        break;
    case ReactionStyle::Tumble:  // 삐끗 뒤집히며 흔들린다. 가장 덜 빠르고 가장 우습다.
        s.speedScale = p.fleeSpeedScale * 0.85f;
        s.spinDegPerSec = -260.f;
        s.wobbleDeg = 22.f;
        break;
    }
    return s;
}

struct PlayerReactionParams {
    float duration = 0.55f;
    float spinDegTotal = 360.f;      // Spin일 때 한 바퀴. 그 이상은 고장으로 보인다
    float shiverAmplitudeDeg = 20.f; // Dart/Tumble일 때 파르르
    float shiverHz = 9.f;
};

// **내 물고기 전용.** 시나리오 결정표: "도망가지 않는다. 그 자리에서 파르르
// 떨거나 빙글 돌고 기포를 낸다. 조종권을 한 순간도 잃지 않는다."
//
// 그것을 주석이 아니라 타입으로 보장한다: 이 클래스에는 방향·속도·입력을
// 돌려주는 함수가 하나도 없다. 있는 것은 **시각 오프셋 각도** 하나뿐이라,
// 이 반응이 조종을 건드리는 코드를 쓰는 것 자체가 불가능하다.
class PlayerReaction {
public:
    void Touch(std::uint32_t seed) {
        style_ = PickReactionStyle(seed);
        elapsed_ = 0.f;
        active_ = true;
        ++touchCount_;
    }

    // 기본 파라미터로 만료를 판정하는 편의 형태(테스트와 단순한 호출자용).
    void Step(float dt) {
        const PlayerReactionParams p;
        Update(dt, p);
    }

    void Update(float dt, const PlayerReactionParams& p) {
        if (!active_ || dt <= 0.f) return;
        elapsed_ += dt;
        if (elapsed_ >= p.duration) { active_ = false; elapsed_ = 0.f; }
    }

    bool Active() const { return active_; }
    ReactionStyle Style() const { return style_; }
    int TouchCount() const { return touchCount_; }

    // 몸을 돌리는 **시각** 각도(도). 조종과 무관하고, 끝나면 정확히 0으로 돌아온다.
    //
    // 램프 인이 없다: 시나리오 장면 2 요구사항 1("0.05초 안에 시작")이 재롱에도
    // 걸리므로, 떨림은 사인이 아니라 **코사인**이라 첫 프레임부터 최대 진폭이다.
    // 사인으로 두면 t=0에서 값이 0이라 아이 눈에는 늦게 시작한 것으로 보인다.
    float RollOffsetDeg(const PlayerReactionParams& p) const {
        if (!active_ || p.duration <= 0.f) return 0.f;
        const float t = elapsed_ / p.duration;
        if (t >= 1.f) return 0.f;
        const float fade = 1.f - t;                    // 끝으로 갈수록 잦아든다
        if (style_ == ReactionStyle::Spin) {
            // 한 바퀴를 duration 동안 돈 뒤 정확히 0에서 끝난다.
            const float a = p.spinDegTotal * t;
            return a >= p.spinDegTotal ? 0.f : a;
        }
        // 파르르: 코사인 몇 번, 진폭은 점점 줄어든다.
        const float phase = 6.2831853f * p.shiverHz * elapsed_;
        return p.shiverAmplitudeDeg * fade * PlayerReactionSin(phase + 1.5707963f);
    }

private:
    ReactionStyle style_ = ReactionStyle::Dart;
    float elapsed_ = 0.f;
    bool active_ = false;
    int touchCount_ = 0;
};

} // namespace aquarium
