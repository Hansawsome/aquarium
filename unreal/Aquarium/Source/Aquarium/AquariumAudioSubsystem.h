#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "aquarium/Audio.h"

#include "AquariumAudioSubsystem.generated.h"

class UAudioComponent;
class USoundBase;

// 한 번에 재생되는 반응음 한 종류. 순서는 kCuePaths와 짝이고, Count는 개수를
// 세는 유일한 출처다(테스트가 기대 개수를 리터럴로 베끼지 않게 하려는 것).
UENUM()
enum class EAquariumCue : uint8
{
	Startle,   // 「퍽」 배경 물고기가 놀람 (개정 전 「꺅」. 만화 비명은 유치함 신호다)
	Thud,      // 「쿵」 몸으로 들이받아 **잡았다**
	Nibble,    // 「뽁」 먹이를 먹음
	Split,     // 「촤악」 무리가 갈라짐
	Bubble,    // 「뽀글」 내 물고기 재롱
	Count UMETA(Hidden)
};

// 이 게임의 모든 소리를 가진 곳. 사운드 그래프(MetaSound/SoundCue)는 쓰지 않는다 --
// 에디터 파이썬으로 만들 수 없기 때문이고, 그 결정이 이 클래스가 존재하는 이유다.
// 소리는 전부 2D다: 카메라가 고정이라 감쇠가 할 일이 없고, 감쇠 에셋을 만들지
// 않는다는 뜻이기도 하다.
UCLASS()
class AQUARIUM_API UAquariumAudioSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 플레이어 물고기의 현재 속도를 넘긴다. 음높이·음량은 규칙 계층이 정한다.
	// MaxSpeed를 같이 받는 이유: 속도 기준을 C++에 리터럴로 베끼지 않기 위해서다.
	void UpdateSwim(float Speed, float MaxSpeed, float DeltaSeconds);

	// 반응음 한 번. Seed가 재생별 음높이를 정한다(같은 시드 = 같은 음높이).
	// **재생을 거절하지 않는다.** 밀도가 높으면 음량만 내린다(시나리오 장면 2 요구사항 4).
	void PlayCue(EAquariumCue Cue, uint32 Seed);

	// 배경 앰비언스 루프를 시작한다. 이미 돌고 있으면 아무것도 하지 않는다.
	void StartAmbience();
	void StopAmbience();

	// 그림 버튼이 부르는 곳. 소리를 끄는 것이지 게임을 멈추는 것이 아니다.
	void SetMuted(bool bInMuted);
	bool IsMuted() const { return bMuted; }
	void ToggleMuted() { SetMuted(!bMuted); }

	// --- 에셋 접근(테스트가 "에셋이 정말 있는지"를 보게 한다) ---
	USoundBase* SoundFor(EAquariumCue Cue) const;
	USoundBase* SwimSound() const { return SwimWave; }
	USoundBase* AmbienceSound() const { return AmbienceWave; }

	// --- 헤드리스에서 관측 가능한 상태 ---
	// -nullrhi/-nosound 에서는 오디오 디바이스가 없어 UAudioComponent가 안 생길 수
	// 있다. 그래서 규칙 계층이 계산한 값은 컴포넌트와 무관하게 여기 기록된다.
	// 테스트는 이 값을 본다. "소리가 실제로 들렸다"는 주장은 하지 않는다.
	float LastSwimPitch() const { return LastSwimPitchValue; }
	float LastSwimVolume() const { return LastSwimVolumeValue; }
	float LastCuePitch() const { return LastCuePitchValue; }
	float LastCueVolume() const { return LastCueVolumeValue; }
	int32 CuePlayCount(EAquariumCue Cue) const;

	// 개발 전용: 오디오를 통째로 끈다(성능 귀속용). 값을 받지 않는 불리언이라
	// 파싱 실패로 조용히 무시될 토큰이 없다.
	bool bAudioEnabled = true;

private:
	UPROPERTY() TObjectPtr<USoundBase> SwimWave = nullptr;
	UPROPERTY() TObjectPtr<USoundBase> AmbienceWave = nullptr;
	UPROPERTY() TArray<TObjectPtr<USoundBase>> CueWaves;
	UPROPERTY() TObjectPtr<UAudioComponent> SwimComponent = nullptr;
	UPROPERTY() TObjectPtr<UAudioComponent> AmbienceComponent = nullptr;

	aquarium::AudioParams AudioParamsValue;
	aquarium::VoiceLimiter Voices;
	TArray<int32> PlayCounts;
	float LastSwimPitchValue = 0.f;
	float LastSwimVolumeValue = 0.f;
	float LastCuePitchValue = 1.f;
	float LastCueVolumeValue = 1.f;
	bool bMuted = false;

	// 뮤트 상태를 두 컴포넌트에 반영한다. 정지시키지 않고 음량만 0으로 둔다 --
	// 다시 켰을 때 앰비언스가 처음부터 시작하면 "끊겼다 이어졌다"가 들린다.
	void ApplyMute();
	static USoundBase* LoadSound(const TCHAR* Path);
};
