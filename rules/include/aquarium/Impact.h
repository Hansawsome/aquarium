#pragma once

#include "aquarium/Reaction.h"   // PlayerReactionSin

namespace aquarium {

// 부딪힌 순간의 연출값. 시나리오: "짧은 화면 흔들림. **타격감의 9할이 여기서 나온다.**"
//
// 여기에는 **크기 배율이 없다.** 개정된 연출 문법이 부풀기를 유치함 신호로 못 박았고,
// Reaction.h가 같은 이유로 speedScale만 가진 것과 같은 결정이다.
struct ImpactParams {
    float duration = 0.20f;      // 짧다. 길면 '흔들림'이 아니라 '지진'이 된다
    float amplitudeCm = 6.0f;    // 카메라가 흔들리는 최대 거리
    float hz = 24.f;             // 빠르게 떨어야 '툭'이지 '흔들흔들'이 아니다
    float displacementMax = 1.f; // 물 밀림 왜곡의 최대 가중치(머티리얼로 간다)
};

struct ShakeOffset {
    float y = 0.f;   // 월드 Y(화면 가로)
    float z = 0.f;   // 월드 Z(화면 세로)
};

class ImpactShake {
public:
    // 다시 맞으면 **새로 시작한다.** 겹쳐 쌓으면 연타할 때 아이가 멀미한다.
    void Hit(const ImpactParams& p) { elapsed_ = 0.f; duration_ = p.duration; active_ = p.duration > 0.f; }

    void Step(float dt) {
        if (!active_ || dt <= 0.f) return;
        elapsed_ += dt;
    }

    // 시간까지 본다. active_만 보면 수명이 끝나도 참이라 카메라 흔들림이 영원히
    // 켜져 있는 것으로 읽힌다.
    bool Active() const { return active_ && elapsed_ < duration_; }

    // 감쇠하는 진동. 코사인이라 **첫 프레임부터 최대 진폭**이다 -- 사인으로 두면
    // t=0에서 0이라 아이 눈에 늦게 시작한 것으로 보인다(PlayerReaction과 같은 이유).
    ShakeOffset Offset(const ImpactParams& p) const {
        ShakeOffset o;
        if (!active_ || p.duration <= 0.f) return o;
        const float t = elapsed_ / p.duration;
        if (t >= 1.f) return o;
        const float fade = 1.f - t;
        const float w = 6.2831853f * p.hz * elapsed_;
        const float quarter = 1.5707963f;
        o.y = p.amplitudeCm * fade * PlayerReactionSin(w + quarter);
        // 세로축은 조금 다른 주파수와 위상이다. 같으면 대각선으로만 흔들려
        // '툭 튕겼다'가 아니라 '미끄러졌다'가 된다.
        o.z = p.amplitudeCm * 0.72f * fade * PlayerReactionSin(w * 1.37f);
        return o;
    }

    // 물 밀림 왜곡의 가중치. **진동하지 않고 내려가기만 한다** -- 깜빡이면 화면이
    // 지글거린다. 흔드는 것은 카메라 하나로 충분하다.
    float DisplacementWeight(const ImpactParams& p) const {
        if (!active_ || p.duration <= 0.f) return 0.f;
        const float t = elapsed_ / p.duration;
        if (t >= 1.f) return 0.f;
        const float fade = 1.f - t;
        return p.displacementMax * fade * fade;
    }

private:
    float elapsed_ = 0.f;
    float duration_ = 0.f;
    bool active_ = false;
};

} // namespace aquarium
