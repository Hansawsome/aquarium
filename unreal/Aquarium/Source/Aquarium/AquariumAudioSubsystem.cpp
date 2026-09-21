#include "AquariumAudioSubsystem.h"

#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Sound/SoundBase.h"

namespace
{
	// /Game/Audio 아래 import_audio.py가 만든 에셋들. 순서는 EAquariumCue와 짝이다.
	const TCHAR* kCuePaths[] = {
		TEXT("/Game/Audio/S_Startle.S_Startle"),
		TEXT("/Game/Audio/S_Thud.S_Thud"),
		TEXT("/Game/Audio/S_Nibble.S_Nibble"),
		TEXT("/Game/Audio/S_Split.S_Split"),
		TEXT("/Game/Audio/S_Bubble.S_Bubble"),
	};
	static_assert(UE_ARRAY_COUNT(kCuePaths) == static_cast<int32>(EAquariumCue::Count),
		"EAquariumCue and kCuePaths must stay in step");
	constexpr float kAmbienceVolume = 0.42f;
	constexpr float kAmbienceFadeIn = 2.0f;
}

USoundBase* UAquariumAudioSubsystem::LoadSound(const TCHAR* Path)
{
	// 조용히 nullptr이 되는 경로다. 테스트 Aquarium.Audio.AssetsLoad가 이걸 본다.
	return LoadObject<USoundBase>(nullptr, Path);
}

void UAquariumAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
#if !UE_BUILD_SHIPPING
	bAudioEnabled = !FParse::Param(FCommandLine::Get(), TEXT("AquariumNoAudio"));
	bMuted = FParse::Param(FCommandLine::Get(), TEXT("AquariumMuted"));
#endif
	SwimWave = LoadSound(TEXT("/Game/Audio/S_Swim.S_Swim"));
	// 앰비언스는 아직 저장소에 없을 수 있다(CC0 녹음, scripts/check_ambience.sh).
	// 없으면 nullptr이고 StartAmbience가 조용히 아무 일도 하지 않는다.
	AmbienceWave = LoadSound(TEXT("/Game/Audio/A_Underwater.A_Underwater"));
	CueWaves.Reset();
	for (const TCHAR* Path : kCuePaths)
	{
		CueWaves.Add(LoadSound(Path));
	}
	PlayCounts.Init(0, static_cast<int32>(EAquariumCue::Count));
}

void UAquariumAudioSubsystem::Deinitialize()
{
	if (SwimComponent) { SwimComponent->Stop(); SwimComponent = nullptr; }
	if (AmbienceComponent) { AmbienceComponent->Stop(); AmbienceComponent = nullptr; }
	Super::Deinitialize();
}

USoundBase* UAquariumAudioSubsystem::SoundFor(EAquariumCue Cue) const
{
	const int32 Index = static_cast<int32>(Cue);
	return CueWaves.IsValidIndex(Index) ? CueWaves[Index].Get() : nullptr;
}

int32 UAquariumAudioSubsystem::CuePlayCount(EAquariumCue Cue) const
{
	const int32 Index = static_cast<int32>(Cue);
	return PlayCounts.IsValidIndex(Index) ? PlayCounts[Index] : 0;
}

void UAquariumAudioSubsystem::UpdateSwim(float Speed, float MaxSpeed, float DeltaSeconds)
{
	Voices.Step(DeltaSeconds);
	// 속도 기준은 물고기에서 파생한다. AFishActor::MaxSpeed를 C++에 두 번 적지 않는다.
	AudioParamsValue.speedForMaxPitch = MaxSpeed > 1e-3f ? MaxSpeed : AudioParamsValue.speedForMaxPitch;
	LastSwimPitchValue = aquarium::SwimPitch(Speed, AudioParamsValue);
	LastSwimVolumeValue = aquarium::SwimVolume(Speed, AudioParamsValue);
	if (!bAudioEnabled || SwimWave == nullptr)
	{
		return;
	}
	if (SwimComponent == nullptr)
	{
		// bAutoDestroy=false: 이 컴포넌트는 세션 내내 살아 있고 음량만 움직인다.
		// 매번 새로 스폰하면 방향키를 누를 때마다 루프가 처음부터 시작해 딱딱 끊긴다.
		SwimComponent = UGameplayStatics::CreateSound2D(this, SwimWave, 0.f, 1.f, 0.f,
			nullptr, /*bPersistAcrossLevelTransition*/ false, /*bAutoDestroy*/ false);
		if (SwimComponent)
		{
			SwimComponent->Play();
		}
	}
	if (SwimComponent)
	{
		SwimComponent->SetPitchMultiplier(LastSwimPitchValue);
		SwimComponent->SetVolumeMultiplier(bMuted ? 0.f : LastSwimVolumeValue);
	}
}

void UAquariumAudioSubsystem::PlayCue(EAquariumCue Cue, uint32 Seed)
{
	const int32 Index = static_cast<int32>(Cue);
	if (!PlayCounts.IsValidIndex(Index))
	{
		return;
	}
	// 횟수·음높이·음량은 오디오 디바이스가 없어도 항상 기록된다. 이것이 헤드리스
	// 검증이 닿는 한계이고, 테스트는 정확히 여기까지만 주장한다.
	++PlayCounts[Index];
	LastCuePitchValue = aquarium::JitterPitch(Seed, AudioParamsValue);
	// 거절이 아니라 음량 완화다. Admit은 실패를 돌려줄 수 없다.
	LastCueVolumeValue = Voices.Admit(AudioParamsValue);
	if (!bAudioEnabled || bMuted)
	{
		return;
	}
	USoundBase* Sound = SoundFor(Cue);
	if (Sound == nullptr)
	{
		return;
	}
	UGameplayStatics::SpawnSound2D(this, Sound, LastCueVolumeValue, LastCuePitchValue);
}

void UAquariumAudioSubsystem::StartAmbience()
{
	if (!bAudioEnabled || AmbienceWave == nullptr || AmbienceComponent != nullptr)
	{
		return;
	}
	AmbienceComponent = UGameplayStatics::CreateSound2D(this, AmbienceWave, kAmbienceVolume, 1.f, 0.f,
		nullptr, false, /*bAutoDestroy*/ false);
	if (AmbienceComponent)
	{
		// 페이드 인: 입장 순간 앰비언스가 툭 켜지면 그것만 의식된다. 앰비언스는
		// 의식되지 않아야 성공이다(시나리오 결정표).
		AmbienceComponent->FadeIn(kAmbienceFadeIn, bMuted ? 0.f : kAmbienceVolume);
	}
}

void UAquariumAudioSubsystem::StopAmbience()
{
	if (AmbienceComponent)
	{
		AmbienceComponent->Stop();
		AmbienceComponent = nullptr;
	}
}

void UAquariumAudioSubsystem::SetMuted(bool bInMuted)
{
	bMuted = bInMuted;
	ApplyMute();
}

void UAquariumAudioSubsystem::ApplyMute()
{
	// 정지가 아니라 음량 0이다. 앰비언스를 정지시켰다 다시 켜면 루프가 처음으로
	// 돌아가 "끊겼다 이어졌다"가 들리고, 그건 의식되는 앰비언스가 된다.
	if (AmbienceComponent) { AmbienceComponent->SetVolumeMultiplier(bMuted ? 0.f : kAmbienceVolume); }
	if (SwimComponent) { SwimComponent->SetVolumeMultiplier(bMuted ? 0.f : LastSwimVolumeValue); }
}
