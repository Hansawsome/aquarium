#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Sound/SoundWave.h"
#include "AquariumAudioSubsystem.h"
#include "aquarium/Audio.h"

namespace
{
UAquariumAudioSubsystem* MakeAudio(UWorld*& OutWorld)
{
	OutWorld = FAutomationEditorCommonUtils::CreateNewMap();
	return OutWorld ? OutWorld->GetSubsystem<UAquariumAudioSubsystem>() : nullptr;
}
}

// 에셋이 정말로 있는지. import_audio.py를 안 돌린 채 "재생 호출은 했다"로 초록불이
// 켜지는 것이 이 마일스톤에서 가장 쉬운 공허한 테스트다.
//
// 앰비언스(A_Underwater)는 여기서 단언하지 않는다. CC0 녹음이라 아직 저장소에
// 없고, 그 빨간불의 주인은 scripts/check_ambience.sh다. 여기에 단언을 두면
// 한 가지 결함을 두 곳이 보고하면서 이 테스트 전체가 그때까지 빨간불이 된다.
// 앰비언스가 들어오는 Task 6에서 단언이 여기로 온다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioAssetsLoad, "Aquarium.Audio.AssetsLoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAudioAssetsLoad::RunTest(const FString&)
{
	UWorld* World = nullptr;
	UAquariumAudioSubsystem* Audio = MakeAudio(World);
	if (!TestNotNull(TEXT("audio subsystem"), Audio)) return false;
	TestNotNull(TEXT("S_Swim"), Audio->SwimSound());
	int32 Loaded = 0;
	for (uint8 i = 0; i < static_cast<uint8>(EAquariumCue::Count); ++i)
	{
		if (Audio->SoundFor(static_cast<EAquariumCue>(i)) != nullptr) ++Loaded;
	}
	// 개수를 리터럴로 베끼지 않는다: Count가 곧 기대값이다.
	TestEqual(TEXT("every cue has a sound"), Loaded, static_cast<int32>(EAquariumCue::Count));
	// 헤엄 소리는 루프여야 한다. import_audio.py가 설정한 플래그를 여기서 되읽는다.
	if (const USoundWave* Wave = Cast<USoundWave>(Audio->SwimSound()))
	{
		TestTrue(TEXT("swim sound loops"), Wave->bLooping);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioSwimFollowsSpeed, "Aquarium.Audio.SwimFollowsSpeed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAudioSwimFollowsSpeed::RunTest(const FString&)
{
	UWorld* World = nullptr;
	UAquariumAudioSubsystem* Audio = MakeAudio(World);
	if (!TestNotNull(TEXT("audio subsystem"), Audio)) return false;
	const float MaxSpeed = 40.f;
	Audio->UpdateSwim(0.f, MaxSpeed, 0.016f);
	const float RestPitch = Audio->LastSwimPitch();
	TestEqual(TEXT("silent at rest"), Audio->LastSwimVolume(), 0.f, 1e-4f);
	Audio->UpdateSwim(MaxSpeed, MaxSpeed, 0.016f);
	TestTrue(TEXT("pitch rises with speed"), Audio->LastSwimPitch() > RestPitch + 0.1f);
	TestTrue(TEXT("audible at speed"), Audio->LastSwimVolume() > 0.f);
	// 기대값은 규칙 계층에서 파생한다(엔진에 두 번째 사본을 만들지 않는다).
	aquarium::AudioParams P;
	P.speedForMaxPitch = MaxSpeed;
	TestEqual(TEXT("engine agrees with the rules layer"),
		Audio->LastSwimPitch(), aquarium::SwimPitch(MaxSpeed, P), 1e-4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioCuePitchVaries, "Aquarium.Audio.CuePitchVaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAudioCuePitchVaries::RunTest(const FString&)
{
	UWorld* World = nullptr;
	UAquariumAudioSubsystem* Audio = MakeAudio(World);
	if (!TestNotNull(TEXT("audio subsystem"), Audio)) return false;
	TArray<float> Pitches;
	for (int32 i = 0; i < 12; ++i)
	{
		Audio->PlayCue(EAquariumCue::Startle, static_cast<uint32>(i * 7919 + 13));
		Pitches.Add(Audio->LastCuePitch());
	}
	int32 Distinct = 0;
	for (int32 i = 1; i < Pitches.Num(); ++i) { if (!FMath::IsNearlyEqual(Pitches[i], Pitches[i - 1])) ++Distinct; }
	TestTrue(TEXT("pitch differs between plays"), Distinct >= 10);
	TestEqual(TEXT("every play was counted"), Audio->CuePlayCount(EAquariumCue::Startle), 12);
	return true;
}

// 시나리오 장면 2 요구사항 4를 엔진 쪽에서 한 번 더 못 박는다: 연타해도
// 재생 횟수가 요청 횟수와 정확히 같다. 하나라도 삼켜지면 빨간불이다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioRapidCuesAreNeverDropped, "Aquarium.Audio.RapidCuesAreNeverDropped",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAudioRapidCuesAreNeverDropped::RunTest(const FString&)
{
	UWorld* World = nullptr;
	UAquariumAudioSubsystem* Audio = MakeAudio(World);
	if (!TestNotNull(TEXT("audio subsystem"), Audio)) return false;
	const int32 Requested = 40;
	float MinGain = 2.f;
	for (int32 i = 0; i < Requested; ++i)
	{
		Audio->PlayCue(EAquariumCue::Nibble, static_cast<uint32>(i));
		MinGain = FMath::Min(MinGain, Audio->LastCueVolume());
	}
	TestEqual(TEXT("no cue was dropped"), Audio->CuePlayCount(EAquariumCue::Nibble), Requested);
	aquarium::AudioParams P;
	TestTrue(TEXT("ducked but never silent"), MinGain >= P.minVoiceGain - 1e-4f && MinGain < 1.f);
	return true;
}

// 잡았을 때의 「쿵」은 놀람의 「퍽」과 **다른 에셋**이어야 한다. 같은 파일을
// 두 번 꽂아 두면 아이는 '비켰다'와 '잡았다'를 소리로 구분하지 못한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioThudCueExists, "Aquarium.Audio.ThudCueExists",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAudioThudCueExists::RunTest(const FString&)
{
	UWorld* World = nullptr;
	UAquariumAudioSubsystem* Audio = MakeAudio(World);
	if (!TestNotNull(TEXT("audio subsystem"), Audio)) return false;
	USoundBase* Thud = Audio->SoundFor(EAquariumCue::Thud);
	USoundBase* Startle = Audio->SoundFor(EAquariumCue::Startle);
	TestNotNull(TEXT("thud wave loaded"), Thud);
	TestNotNull(TEXT("startle wave loaded"), Startle);
	TestTrue(TEXT("thud is a different asset from startle"), Thud != Startle);
	// 큐를 더했는데 이 값이 안 늘면 어딘가를 빼먹은 것이다. 유일한 리터럴이고,
	// 의도한 변경일 때만 사람이 손으로 고치라는 뜻이다.
	TestEqual(TEXT("cue count"), static_cast<int32>(EAquariumCue::Count), 5);
	return true;
}

#endif
