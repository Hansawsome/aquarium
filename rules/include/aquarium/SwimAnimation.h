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
    // Clamp applies to the head bone; bone i swings max x (1 + i*tailGain).
    float maxBendDeg = 20.f;
};

// Procedural body wave (F-13): amplitude and frequency follow speed, bend follows turn rate.
// Phase is accumulated by the caller (via AdvancePhase) rather than derived from absolute time,
// so a mid-swim speed change does not pop the wave and float precision does not degrade over time.
struct SwimAnimation {
    static float Amplitude(float speed, const SwimAnimParams& p) {
        speed = std::max(speed, 0.f); // speed is a magnitude
        return std::min(p.idleAmplitudeDeg + p.amplitudePerSpeedDeg * speed, p.maxAmplitudeDeg);
    }
    static float Frequency(float speed, const SwimAnimParams& p) {
        speed = std::max(speed, 0.f); // speed is a magnitude
        return p.idleFrequencyHz + p.frequencyPerSpeedHz * speed;
    }
    static float Bend(float turnRateDegPerSec, const SwimAnimParams& p) {
        return std::clamp(p.bendPerTurnRateDeg * turnRateDegPerSec, -p.maxBendDeg, p.maxBendDeg);
    }

    // Advances an accumulated phase (radians) by 2*pi*Frequency(speed)*dt, wrapped into [0, 2*pi).
    // dt <= 0 leaves the phase unchanged.
    static float AdvancePhase(float phaseRad, float speed, float dt, const SwimAnimParams& p) {
        if (dt <= 0.f) {
            return phaseRad;
        }
        constexpr float kTwoPi = 6.283185307f;
        float next = std::fmod(phaseRad + kTwoPi * Frequency(speed, p) * dt, kTwoPi);
        if (next < 0.f) {
            next += kTwoPi;
        }
        return next;
    }

    // Angles in degrees for bones head(0) .. tail(boneCount-1).
    // phaseRad is the accumulated wave phase (see AdvancePhase), not absolute time.
    static std::vector<float> BoneAngles(float speed, float turnRateDegPerSec, float phaseRad,
                                         const SwimAnimParams& p) {
        const float amp = Amplitude(speed, p);
        const float bend = Bend(turnRateDegPerSec, p);
        std::vector<float> out(static_cast<size_t>(std::max(p.boneCount, 0)));
        for (int i = 0; i < p.boneCount; ++i) {
            const float gain = 1.f + static_cast<float>(i) * p.tailGain;
            out[static_cast<size_t>(i)] =
                amp * gain * std::sin(phaseRad - p.phaseStepRad * static_cast<float>(i)) + bend;
        }
        return out;
    }
};

} // namespace aquarium
