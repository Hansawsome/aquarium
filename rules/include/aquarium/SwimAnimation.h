#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace aquarium {

struct SwimAnimParams {
    int boneCount = 6;
    float idleAmplitudeDeg = 2.f;
    float amplitudePerSpeedDeg = 0.1f;
    float maxAmplitudeDeg = 15.f;
    float idleFrequencyHz = 0.5f;
    float frequencyPerSpeedHz = 0.01f;
    float phaseStepRad = 0.8f;
    float tailGain = 0.5f;
    float bendPerTurnRateDeg = 0.2f;
    float maxBendDeg = 20.f;
};

// Procedural body wave (F-13): amplitude and frequency follow speed, bend follows turn rate.
struct SwimAnimation {
    static float Amplitude(float speed, const SwimAnimParams& p) {
        return std::min(p.idleAmplitudeDeg + p.amplitudePerSpeedDeg * speed, p.maxAmplitudeDeg);
    }
    static float Frequency(float speed, const SwimAnimParams& p) {
        return p.idleFrequencyHz + p.frequencyPerSpeedHz * speed;
    }
    static float Bend(float turnRateDegPerSec, const SwimAnimParams& p) {
        return std::clamp(p.bendPerTurnRateDeg * turnRateDegPerSec, -p.maxBendDeg, p.maxBendDeg);
    }

    // Angles in degrees for bones head(0) .. tail(boneCount-1).
    static std::vector<float> BoneAngles(float speed, float turnRateDegPerSec, float timeSec,
                                         const SwimAnimParams& p) {
        constexpr float kTwoPi = 6.283185307f;
        const float amp = Amplitude(speed, p);
        const float omega = kTwoPi * Frequency(speed, p);
        const float bend = Bend(turnRateDegPerSec, p);
        std::vector<float> out(static_cast<size_t>(std::max(p.boneCount, 0)));
        for (int i = 0; i < p.boneCount; ++i) {
            const float gain = 1.f + static_cast<float>(i) * p.tailGain;
            out[static_cast<size_t>(i)] =
                amp * gain * std::sin(omega * timeSec - p.phaseStepRad * static_cast<float>(i)) + bend;
        }
        return out;
    }
};

} // namespace aquarium
