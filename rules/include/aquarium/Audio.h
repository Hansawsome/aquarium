#pragma once
#include <cmath>
#include <cstdint>

namespace aquarium {

// 소리의 "규칙"만 여기 있다. 어떤 파일을 재생하는지, 어떤 컴포넌트에 꽂는지는
// 엔진의 일이다. 이 헤더는 Unreal을 모르고, 그래서 Catch2가 전부 볼 수 있다.
struct AudioParams {
    // 헤엄 소리: 속도 0에서 pitchAtRest, speedForMaxPitch에서 pitchAtFullSpeed.
    // speedForMaxPitch는 엔진이 물고기의 최대 속도로 덮어쓴다(리터럴을 베끼지 않는다).
    float speedForMaxPitch = 40.f;    // cm/s
    float pitchAtRest = 0.80f;
    float pitchAtFullSpeed = 1.65f;
    float swimVolumeMax = 0.55f;
    // 반응음: 재생마다 1.0 +- pitchJitter 안에서 흔든다.
    float pitchJitter = 0.22f;
    // 연타 완화: voiceWindow가 한 번 흐를 때마다 최근 재생 수의 절반을 잊는다.
    float voiceWindow = 0.45f;        // s (반감기)
    int voiceSoftCap = 3;
    float minVoiceGain = 0.30f;       // 절대 0이 되지 않는다. 무시당한 느낌 금지.
};

inline float Clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }

// 속도를 0..1로 정규화한다. speedForMaxPitch가 0이면 항상 0(0으로 나누지 않는다).
inline float SpeedFraction(float speed, const AudioParams& p) {
    if (p.speedForMaxPitch <= 1e-6f) return 0.f;
    return Clamp01(speed / p.speedForMaxPitch);
}

// 시나리오 결정표: "속도가 빠를수록 음이 높아진다". 선형이면 충분하다 -- 아이는
// 음정을 듣는 게 아니라 "내가 빨라지면 소리가 올라간다"를 듣는다.
inline float SwimPitch(float speed, const AudioParams& p) {
    return p.pitchAtRest + (p.pitchAtFullSpeed - p.pitchAtRest) * SpeedFraction(speed, p);
}

// 멈춰 있으면 완전한 무음이어야 한다. 제곱근 곡선을 쓰는 이유는 사람 귀가
// 음량을 로그로 듣기 때문이다 -- 선형이면 느린 헤엄에서 아무것도 안 들린다.
inline float SwimVolume(float speed, const AudioParams& p) {
    return p.swimVolumeMax * std::sqrt(SpeedFraction(speed, p));
}

// 재생마다 흔드는 음높이. 결정적(같은 시드 = 같은 값)이라 테스트가 붙는다.
// 32비트 정수 해시(splitmix32) 한 번이면 연속된 시드도 완전히 흩어진다 --
// 단순한 곱셈-잉여였다면 1,2,3,4...가 거의 같은 값을 냈을 것이다.
inline float JitterPitch(std::uint32_t seed, const AudioParams& p) {
    std::uint32_t x = seed + 0x9E3779B9u;
    x ^= x >> 16; x *= 0x21F0AAADu;
    x ^= x >> 15; x *= 0x735A2D97u;
    x ^= x >> 15;
    const float unit = static_cast<float>(x >> 8) / 16777216.f;   // [0,1)
    return 1.f + (unit * 2.f - 1.f) * p.pitchJitter;
}

// 연타에서 소리가 겹쳐 지지직거리는 것을 막되, **재생을 거절하지 않는다.**
// 시나리오 장면 2 요구사항 4가 금지한 것은 "무시"이지 "작아짐"이 아니다.
// Admit()에는 실패를 표현할 반환값이 없다 -- 구조상 무시가 불가능하다.
class VoiceLimiter {
public:
    // 조용한 시간이 흐른 만큼 최근 재생 수를 잊는다. 반감기는 voiceWindow이고,
    // 뺄셈이 아니라 반감기인 이유는 연타가 길수록 회복도 그만큼 걸려야 하되
    // 손을 떼면 몇 창 안에 반드시 원래 음량으로 돌아와야 하기 때문이다.
    void Step(float dt) {
        if (dt <= 0.f) return;
        recent_ *= std::exp2(-dt / window_);
        if (recent_ < 1e-4f) recent_ = 0.f;
    }

    // 이번 재생에 쓸 음량 배수를 돌려준다. 항상 (0, 1] 안이다.
    float Admit(const AudioParams& p) {
        window_ = p.voiceWindow > 1e-4f ? p.voiceWindow : 1e-4f;
        const float cap = static_cast<float>(p.voiceSoftCap);
        const float over = recent_ - cap;
        recent_ += 1.f;
        if (over <= 0.f) return 1.f;
        // 초과분 한 건마다 20%씩 줄이되 minVoiceGain 아래로는 절대 안 간다.
        float gain = 1.f / (1.f + 0.2f * over);
        if (gain < p.minVoiceGain) gain = p.minVoiceGain;
        return gain;
    }

    float RecentCount() const { return recent_; }

private:
    float recent_ = 0.f;
    float window_ = 0.45f;
};

} // namespace aquarium
