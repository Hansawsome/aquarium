#pragma once
#include <cstdint>

#include "aquarium/Reaction.h"   // ReactionHash, PlayerReactionSin
#include "aquarium/Vec2.h"

namespace aquarium {

// 한 개의 기포. 액터가 아니다 -- 엔진은 이것의 배열을 들고 인스턴스 메시 한 개로
// 그린다. 기포 200개가 떠 있어도 틱은 하나다.
struct Bubble {
    Vec2 position;        // 공유 유영 프레임(x = 월드 Y, y = 월드 Z)
    float depth = 0.f;    // 월드 X. 클릭이 맞힌 평면에서 온다
    float riseSpeed = 0.f;
    float swayPhase = 0.f;
    float radius = 0.f;
    float age = 0.f;
    float baseX = 0.f;    // 좌우 흔들림의 기준선. 표류를 구조적으로 막는다
    // **이 기포가** 화면 위를 넘는 높이. 기포마다 다르다: 카메라가 원근이라
    // 깊은 평면일수록 화면 위 끝이 더 높다. 전체에 하나의 높이를 쓰면 깊은
    // 물고기에서 난 기포가 화면 한가운데에서 사라진다 -- 시선의 약속이 깨지는
    // 자리가 정확히 여기다(계획의 BubbleTopZ가 이 함정에 빠져 있었다).
    float topY = 0.f;
};

struct BubbleParams {
    float riseSpeed = 62.f;        // cm/s
    float riseSpeedJitter = 0.30f; // +-30%
    float swayAmplitude = 7.f;     // cm
    float swayHz = 0.9f;
    float radius = 3.2f;           // cm
    float radiusJitter = 0.45f;
};

inline float BubbleUnit(std::uint32_t seed) {
    return static_cast<float>(ReactionHash(seed) >> 8) / 16777216.f;   // [0,1)
}

inline Bubble MakeBubble(Vec2 at, float depth, std::uint32_t seed, const BubbleParams& p) {
    Bubble b;
    b.position = at;
    b.baseX = at.x;
    b.depth = depth;
    b.riseSpeed = p.riseSpeed * (1.f + (BubbleUnit(seed * 3u + 1u) * 2.f - 1.f) * p.riseSpeedJitter);
    b.radius = p.radius * (1.f + (BubbleUnit(seed * 5u + 7u) * 2.f - 1.f) * p.radiusJitter);
    b.swayPhase = BubbleUnit(seed * 11u + 3u) * 6.2831853f;
    return b;
}

// 위치를 한 걸음 옮긴다. **수명이 없다.** 지우는 조건은 BubbleIsGone 하나뿐이다.
inline void StepBubble(Bubble& b, float dt, const BubbleParams& p) {
    if (dt <= 0.f) return;
    b.age += dt;
    b.position.y += b.riseSpeed * dt;
    // 좌우는 기준선 주위의 사인이다. 속도를 누적하지 않으므로 표류가 생길 수 없다.
    const float phase = b.swayPhase + 6.2831853f * p.swayHz * b.age;
    b.position.x = b.baseX + p.swayAmplitude * PlayerReactionSin(phase);
}

// 화면 위를 넘었을 때만 참이다. 시간으로는 절대 사라지지 않는다 -- 시나리오
// 장면 2 요구사항 3의 '시선의 약속'이 여기 한 줄로 표현돼 있다. topY는 물고기
// 유영 영역의 반높이에서 **파생**해 엔진이 넘긴다(숫자를 베끼지 않는다).
inline bool BubbleIsGone(const Bubble& b, float topY) {
    return b.position.y >= topY;
}

// 기포가 스스로 가진 높이로 판정한다. 엔진은 이쪽을 쓴다 -- 클릭이 맞힌 평면의
// 깊이에서 화면 위 끝을 계산해 기포마다 심어 두기 때문이다.
inline bool BubbleIsGone(const Bubble& b) { return b.position.y >= b.topY; }

} // namespace aquarium
