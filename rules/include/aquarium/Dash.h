#pragma once

namespace aquarium {

// 돌진. 시나리오: "가속/돌진 같은 것 하나만 있어도 잘하는 아이와 못하는 아이가 갈린다."
//
// **거절하지 않는다.** 연타하면 약해질 뿐 항상 무언가는 일어난다 -- VoiceLimiter와
// 같은 원칙이다(금지된 것은 무시이지 작아짐이 아니다). 그래서 Press에는 실패를
// 표현할 반환값이 없고, '남은 횟수'를 돌려주는 함수도 없다. 화면에 게이지를 띄우지
// 않는 이유도 같다 -- 게이지는 곧 **잃는 자원**이고 그것은 벌이다.
struct DashParams {
    float burstScale = 2.4f;        // 최대 속도 배율의 꼭대기
    // **0.9초인 이유도 실측이다.** 0.35초였을 때 돌진은 아무것도 하지 않았다:
    // 실제 실행에서 돌진을 0.4초마다 눌러도 접근 속도의 최댓값이 0.43으로,
    // 돌진 없는 0.44와 **구분되지 않았다.** 이유는 구조적이다 -- 돌진은 속도
    // 상한만 올리는데 플레이어 가속은 140 cm/s^2이라, 상한이 0.35초 만에 도로
    // 내려오면 속도가 거기까지 따라붙을 시간이 없다. 0.9초는 상한선과 가속선이
    // 만나는 지점(약 0.45초) 뒤까지 창을 열어 두어, 문턱을 넘은 채로 있는 시간이
    // 0.4초 이상이 된다(tests/test_dash.cpp가 그 창을 직접 잰다).
    float burstDuration = 0.9f;    // 그 꼭대기에서 1로 내려오는 시간
    float rechargeDuration = 1.3f;  // 0에서 1까지 차는 시간(연속)
    // 완전히 방전됐을 때도 이만큼은 나간다. 0으로 두면 그것이 곧 거절이다.
    float minChargeFraction = 0.35f;
};

class DashDrive {
public:
    // 누르면 반드시 무언가 일어난다. 반환값이 없다 -- 구조적으로 거절이 불가능하다.
    void Press(const DashParams& p) {
        const float c = charge_ < p.minChargeFraction ? p.minChargeFraction : charge_;
        peak_ = 1.f + (p.burstScale - 1.f) * c;
        timer_ = p.burstDuration;
        charge_ = 0.f;
    }

    void Step(float dt, const DashParams& p) {
        if (dt <= 0.f) return;
        if (timer_ > 0.f) {
            timer_ -= dt;
            if (timer_ < 0.f) timer_ = 0.f;
        }
        if (p.rechargeDuration > 1e-4f) {
            charge_ += dt / p.rechargeDuration;
            if (charge_ > 1.f) charge_ = 1.f;
        } else {
            charge_ = 1.f;
        }
    }

    // 최대 속도에 곱할 배율. 꼭대기에서 1로 선형으로 내려온다 -- 한 번에 1로
    // 떨어뜨리면 브레이크를 밟은 것처럼 보인다(FleeStateMachine이 같은 이유로
    // 회복 구간을 선형으로 둔다).
    float SpeedScale(const DashParams& p) const {
        if (timer_ <= 0.f || p.burstDuration <= 1e-4f) return 1.f;
        const float t = timer_ / p.burstDuration;    // 1에서 0으로
        return 1.f + (peak_ - 1.f) * t;
    }

    // 0..1 연속값. '몇 발 남음'으로 그릴 수 없는 모양인 것이 의도다.
    float Charge() const { return charge_; }

private:
    float charge_ = 1.f;
    float timer_ = 0.f;
    float peak_ = 1.f;
};

} // namespace aquarium
