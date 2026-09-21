#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Tests/AutomationEditorCommon.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "AquariumAudioSubsystem.h"
#include "HudWidget.h"

namespace
{
UHudWidget* MakeHud(UWorld*& OutWorld)
{
	OutWorld = FAutomationEditorCommonUtils::CreateNewMap();
	UHudWidget* Hud = OutWorld ? CreateWidget<UHudWidget>(OutWorld, UHudWidget::StaticClass()) : nullptr;
	if (Hud) { Hud->TakeWidget(); }   // RebuildWidget을 실제로 돌린다
	return Hud;
}
}

// 아이콘 텍스처가 실제로 존재하는지. 없으면 버튼이 빈 사각형으로 그려지는데
// 에디터에서는 그게 "투명한 버튼"으로 보여 멀쩡해 보인다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMuteIconsExist, "Aquarium.Hud.MuteIconsExist",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMuteIconsExist::RunTest(const FString&)
{
	UTexture2D* On = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/T_SoundOn.T_SoundOn"));
	UTexture2D* Off = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/T_SoundOff.T_SoundOff"));
	TestNotNull(TEXT("T_SoundOn"), On);
	TestNotNull(TEXT("T_SoundOff"), Off);
	TestTrue(TEXT("icons differ"), On != Off);
	return true;
}

// 시나리오: "글자는 쓰지 않는다". 뮤트 버튼 안에 UTextBlock이 있으면 빨간불이다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMuteButtonHasNoText, "Aquarium.Hud.MuteButtonHasNoText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMuteButtonHasNoText::RunTest(const FString&)
{
	UWorld* World = nullptr;
	UHudWidget* Hud = MakeHud(World);
	if (!TestNotNull(TEXT("hud"), Hud)) return false;
	TestNotNull(TEXT("mute icon widget"), Hud->MuteIcon());
	int32 TextInsideMute = 0;
	Hud->WidgetTree->ForEachWidget([&](UWidget* W)
	{
		if (W && W->IsA<UTextBlock>() && W->GetName().Contains(TEXT("Mute"))) ++TextInsideMute;
	});
	TestEqual(TEXT("no text in the mute button"), TextInsideMute, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMuteButtonTogglesAudio, "Aquarium.Hud.MuteButtonTogglesAudio",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMuteButtonTogglesAudio::RunTest(const FString&)
{
	UWorld* World = nullptr;
	UHudWidget* Hud = MakeHud(World);
	if (!TestNotNull(TEXT("hud"), Hud)) return false;
	UAquariumAudioSubsystem* Audio = World->GetSubsystem<UAquariumAudioSubsystem>();
	if (!TestNotNull(TEXT("audio"), Audio)) return false;
	bool bFired = false;
	Hud->OnToggleMute.BindLambda([&]() { bFired = true; Audio->ToggleMuted(); });
	TestFalse(TEXT("starts unmuted"), Audio->IsMuted());
	Hud->SimulateMuteClick();
	TestTrue(TEXT("delegate fired"), bFired);
	TestTrue(TEXT("muted after one click"), Audio->IsMuted());
	// 그림이 상태를 따라가야 한다. 안 따라가면 아이는 꺼졌는지 알 수 없다.
	UTexture2D* OffTex = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/T_SoundOff.T_SoundOff"));
	TestEqual(TEXT("icon shows off"), Hud->CurrentMuteTexture(), OffTex);
	// 그리고 실제로 그려지는 브러시가 그 텍스처여야 한다 -- 상태 변수만 바뀌고
	// 그림이 그대로면 아이 눈에는 아무 일도 없었던 것과 같다.
	TestEqual(TEXT("brush follows the state"),
		Cast<UTexture2D>(Hud->MuteIcon()->GetBrush().GetResourceObject()), OffTex);
	Hud->SimulateMuteClick();
	TestFalse(TEXT("unmuted after a second click"), Audio->IsMuted());
	// 뮤트는 소리만 끈다. 게임은 계속 돈다 -- 벌을 만들지 않는다.
	Audio->SetMuted(true);
	Audio->UpdateSwim(40.f, 40.f, 0.016f);
	TestTrue(TEXT("rules still run while muted"), Audio->LastSwimPitch() > 1.f);
	return true;
}

#endif
