#pragma once
#include <algorithm>
#include <vector>

namespace aquarium {

// 아이가 찍은 도장의 장부. 시나리오 결정표: 개수 제한 없음, **떼기 기능 없음**,
// 세션 동안 유지, 나가기로 리셋.
//
// 떼기가 없다는 것을 주석이 아니라 **타입**으로 보장한다 -- 한 마리를 지우는 함수가
// 이 클래스에 아예 없다. 있는 것은 세션 전체를 버리는 Reset()뿐이고 그것은
// "나가기"와 같은 사건이라 실수로 잃는 일이 될 수 없다.
class StampBook {
public:
    // 처음 찍은 것이면 true. 이미 찍은 놈이면 false지만 **실패가 아니다** --
    // 숫자를 두 번 올리지 않을 뿐이고, 호출자는 아무 일도 하지 않으면 된다.
    bool Stamp(int fishId) {
        if (Has(fishId)) return false;
        ids_.push_back(fishId);
        return true;
    }

    bool Has(int fishId) const {
        return std::find(ids_.begin(), ids_.end(), fishId) != ids_.end();
    }

    // 화면 구석의 숫자. 세션 안에서 **오르기만 한다.**
    int Count() const { return static_cast<int>(ids_.size()); }

    // total은 실제로 존재하는 물고기 수에서 **파생**해 넘긴다. 36을 여기 박지 않는다.
    bool IsComplete(int total) const { return total > 0 && Count() >= total; }

    // 완주의 순간을 정확히 한 번만 돌려준다. 바다가 조용히 달라지는 일이 매 틱
    // 다시 일어나면 그것은 연출이 아니라 고장이다.
    bool ConsumeJustCompleted(int total) {
        if (!IsComplete(total) || announced_) return false;
        announced_ = true;
        return true;
    }

    // 나가기. 한 마리씩이 아니라 통째로다.
    void Reset() { ids_.clear(); announced_ = false; }

private:
    std::vector<int> ids_;
    bool announced_ = false;
};

} // namespace aquarium
