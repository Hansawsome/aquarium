#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/catch_approx.hpp>

#include "aquarium/Audio.h"

using aquarium::AudioParams;
using aquarium::VoiceLimiter;
using Catch::Approx;

namespace {
const AudioParams kA{};
}

TEST_CASE("swim pitch rises with speed") {
    // 시나리오 결정표: "속도가 빠를수록 음이 높아진다". 가만히 있으면 기준 음.
    REQUIRE(aquarium::SwimPitch(0.f, kA) == Approx(kA.pitchAtRest));
    REQUIRE(aquarium::SwimPitch(kA.speedForMaxPitch, kA) == Approx(kA.pitchAtFullSpeed));
    // 사이는 단조 증가여야 한다. "조금 빨라졌는데 음이 내려갔다"는 놀이가 되지 않는다.
    float prev = -1.f;
    for (int i = 0; i <= 20; ++i) {
        const float s = kA.speedForMaxPitch * static_cast<float>(i) / 20.f;
        const float p = aquarium::SwimPitch(s, kA);
        REQUIRE(p > prev);
        prev = p;
    }
}

TEST_CASE("swim pitch is clamped above full speed") {
    // 도망 속도 배율이 플레이어에게 걸릴 일은 없지만, 속도는 어떤 경로로든
    // maxSpeed를 넘을 수 있다. 음높이가 무한정 올라가면 삑 소리가 된다.
    REQUIRE(aquarium::SwimPitch(kA.speedForMaxPitch * 10.f, kA) == Approx(kA.pitchAtFullSpeed));
    REQUIRE(aquarium::SwimPitch(-5.f, kA) == Approx(kA.pitchAtRest));
}

TEST_CASE("swim volume is silent at rest and loud at speed") {
    // 멈춰 있는데 물살 소리가 나면 "내가 한 것"이 아니게 된다 -- 장면 1의 전부가 그것이다.
    REQUIRE(aquarium::SwimVolume(0.f, kA) == Approx(0.f));
    REQUIRE(aquarium::SwimVolume(kA.speedForMaxPitch, kA) == Approx(kA.swimVolumeMax));
    REQUIRE(aquarium::SwimVolume(kA.speedForMaxPitch * 0.5f, kA) > 0.f);
}

TEST_CASE("jittered pitch differs between plays but stays in a musical band") {
    // "매번 조금씩 다르다"(장면 2 요구사항 2)를 소리 쪽에서 담당하는 함수.
    float lo = 99.f, hi = -99.f;
    int distinct = 0;
    float previous = -1.f;
    for (unsigned s = 1; s <= 32; ++s) {
        const float p = aquarium::JitterPitch(s, kA);
        REQUIRE(p >= 1.f - kA.pitchJitter - 1e-4f);
        REQUIRE(p <= 1.f + kA.pitchJitter + 1e-4f);
        if (p != previous) ++distinct;
        previous = p;
        lo = p < lo ? p : lo;
        hi = p > hi ? p : hi;
    }
    // 32번 중 최소 30번은 직전과 달라야 한다. 상수를 돌려주는 구현을 잡는 단언이다.
    REQUIRE(distinct >= 30);
    // 그리고 실제로 대역을 쓰고 있어야 한다(전부 1.0 근처면 "다르다"가 안 들린다).
    REQUIRE(hi - lo > kA.pitchJitter);
}

TEST_CASE("jittered pitch is deterministic for a given seed") {
    REQUIRE(aquarium::JitterPitch(12345u, kA) == Approx(aquarium::JitterPitch(12345u, kA)));
}

TEST_CASE("voice limiter never refuses a play (scenario: 연타가 기본 사용법)") {
    // 시나리오 장면 2 요구사항 4: "직전 클릭 처리 중이라 무시됨"이 되면 즉각성이 무너진다.
    // 그래서 이 타입에는 거절을 표현할 반환값 자체가 없고, 음량은 절대 0이 되지 않는다.
    VoiceLimiter v;
    for (int i = 0; i < 50; ++i) {          // 초당 서너 번이 아니라 한 프레임에 50번
        const float gain = v.Admit(kA);
        REQUIRE(gain >= kA.minVoiceGain);
        REQUIRE(gain <= 1.f);
    }
}

TEST_CASE("voice limiter ducks under a burst and recovers after quiet") {
    VoiceLimiter v;
    REQUIRE(v.Admit(kA) == Approx(1.f));    // 첫 소리는 온전한 음량이다
    for (int i = 0; i < 9; ++i) v.Admit(kA);
    const float burst = v.Admit(kA);
    REQUIRE(burst < 1.f);                   // 연타 중에는 줄어들고
    REQUIRE(burst >= kA.minVoiceGain);      // 그래도 들린다
    v.Step(kA.voiceWindow * 2.f);           // 아이가 손을 뗀다
    REQUIRE(v.Admit(kA) == Approx(1.f));    // 다음 한 번은 다시 온전하다
}
