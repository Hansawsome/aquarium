#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "BubbleSubsystem.h"
#include "InstancedFieldActor.h"
#include "FishActor.h"
#include "AquariumGameMode.h"
#include "aquarium/Reaction.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBubblesRiseAndLeaveTheTop, "Aquarium.Bubbles.RiseAndLeaveTheTop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBubblesRiseAndLeaveTheTop::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	UBubbleSubsystem* Bubbles = World->GetSubsystem<UBubbleSubsystem>();
	if (!TestNotNull(TEXT("bubble subsystem"), Bubbles)) return false;
	Bubbles->SetTopZ(300.f);
	Bubbles->Spawn(FVector(400.f, 0.f, 100.f), 9, 5u);
	TestEqual(TEXT("nine bubbles"), Bubbles->ActiveCount(), 9);
	// 화면 위에 닿기 전에는 한 개도 줄지 않는다(시선의 약속).
	for (int32 i = 0; i < 30; ++i) { Bubbles->Tick(1.f / 60.f); }
	TestEqual(TEXT("none vanished early"), Bubbles->ActiveCount(), 9);
	// 그리고 결국은 전부 나간다(영원히 쌓이지 않는다).
	for (int32 i = 0; i < 60 * 20; ++i) { Bubbles->Tick(1.f / 60.f); }
	TestEqual(TEXT("all gone eventually"), Bubbles->ActiveCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBubblesUseOneInstancedActor, "Aquarium.Bubbles.UseOneInstancedActor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBubblesUseOneInstancedActor::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	UBubbleSubsystem* Bubbles = World->GetSubsystem<UBubbleSubsystem>();
	if (!TestNotNull(TEXT("bubble subsystem"), Bubbles)) return false;
	Bubbles->SetTopZ(2000.f);
	for (int32 i = 0; i < 40; ++i) { Bubbles->Spawn(FVector(400.f, i * 3.f, 0.f), 9, static_cast<uint32>(i)); }
	Bubbles->Tick(1.f / 60.f);
	TestEqual(TEXT("360 bubbles"), Bubbles->ActiveCount(), 360);
	// 액터는 여전히 하나다. 기포마다 액터를 만들면 여기서 빨간불이 켜진다.
	int32 FieldActors = 0;
	for (TActorIterator<AInstancedFieldActor> It(World); It; ++It) { ++FieldActors; }
	TestEqual(TEXT("exactly one field actor"), FieldActors, 1);
	if (AInstancedFieldActor* Field = Bubbles->Field())
	{
		TestEqual(TEXT("one instance per bubble"), Field->InstanceCount(), 360);
		TestFalse(TEXT("the field actor never ticks"), Field->PrimaryActorTick.bCanEverTick);
	}
	return true;
}

// 시선의 약속을 **깊이**까지 밀어붙인다. 카메라가 원근이라 깊은 평면일수록
// 화면 위 끝이 높다. 배경 물고기는 X 330~700에 있고 플레이어 평면은 X 220이라,
// 소멸 높이를 플레이어 평면 하나로 계산하면 가장 깊은 물고기의 기포는 화면
// 1/3 높이에서 사라진다. 계획이 그렇게 쓰여 있었고, 이 테스트가 그것을 잡는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBubbleTopFollowsDepth, "Aquarium.Bubbles.TopFollowsDepth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBubbleTopFollowsDepth::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	AAquariumGameMode* GM = World->SpawnActor<AAquariumGameMode>();
	if (!TestNotNull(TEXT("game mode"), GM)) return false;
	// build_reef_m1.py의 SCHOOL_X 범위 양 끝. 숫자를 베끼는 것이 아니라 "플레이어
	// 평면보다 훨씬 깊은 평면이 존재한다"는 사실을 재현하는 것이다.
	const float PlayerDepth = static_cast<float>(GM->PlaneOrigin.X);
	const float DeepDepth = PlayerDepth * 3.f;
	const float NearTop = GM->ScreenTopZAt(PlayerDepth);
	const float DeepTop = GM->ScreenTopZAt(DeepDepth);
	TestTrue(FString::Printf(TEXT("a deeper plane has a higher screen top (%.1f > %.1f)"), DeepTop, NearTop),
		DeepTop > NearTop + 1.f);
	// 그리고 그 높이는 그 깊이에서 실제로 보이는 위 끝보다 위여야 한다.
	const FVector2D Visible = AAquariumGameMode::VisibleHalfExtents(DeepDepth, GM->ViewFovDeg, GM->ViewAspect);
	TestTrue(FString::Printf(TEXT("above the visible top edge (%.1f >= %.1f)"),
		DeepTop, static_cast<float>(GM->PlaneOrigin.Z) + Visible.Y),
		DeepTop >= static_cast<float>(GM->PlaneOrigin.Z) + Visible.Y);

	// 깊은 곳의 기포는 얕은 소멸 높이를 지나고도 살아 있어야 한다.
	UBubbleSubsystem* Bubbles = World->GetSubsystem<UBubbleSubsystem>();
	if (!TestNotNull(TEXT("bubble subsystem"), Bubbles)) return false;
	Bubbles->Spawn(FVector(DeepDepth, 0.f, GM->PlaneOrigin.Z), 9, 3u, DeepTop);
	bool bSeenAboveNearTop = false;
	for (int32 i = 0; i < 60 * 60; ++i)
	{
		Bubbles->Tick(1.f / 60.f);
		if (Bubbles->ActiveCount() == 0) { break; }
		if (Bubbles->HighestZ() > NearTop) { bSeenAboveNearTop = true; }
	}
	TestTrue(TEXT("deep bubbles keep rising past the shallow plane's top"), bSeenAboveNearTop);
	TestEqual(TEXT("and they do leave eventually"), Bubbles->ActiveCount(), 0);
	return true;
}

// 매번 조금씩 다르게 놀란다(시나리오 장면 2 요구사항 2).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartleStyleVariesBetweenClicks, "Aquarium.Fish.StartleStyleVaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FStartleStyleVariesBetweenClicks::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// 한 마리를 연달아 찍는다 -- 아이의 실제 사용법이고, 매번 달라져야 하는 것도
	// 여러 마리 사이가 아니라 **연속된 클릭 사이**다.
	AFishActor* Fish = World->SpawnActor<AFishActor>(AFishActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SP);
	Fish->Seed = 7u;
	Fish->PlaneOrigin = FVector(300.f, 0.f, 100.f);
	Fish->InitializeSwim();
	TSet<int32> Styles;
	for (int32 i = 0; i < 24; ++i)
	{
		Fish->ApplyFleeFrom(FVector(300.f, 30.f, 100.f));
		Styles.Add(static_cast<int32>(Fish->StartleStyle()));
		Fish->StepSwim(1.f / 60.f);
	}
	TestEqual(TEXT("all three styles appear across repeated clicks"), Styles.Num(), 3);
	return true;
}

// **정확성 요구**: 내 물고기는 클릭당 조종권을 한 프레임도 잃지 않는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOwnFishKeepsControlWhenClicked, "Aquarium.Fish.OwnFishKeepsControl",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOwnFishKeepsControlWhenClicked::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// 두 마리를 같은 조건으로 만들고, 한쪽만 클릭한다. 위치 궤적이 완전히 같아야 한다.
	auto Make = [&]() {
		AFishActor* F = World->SpawnActor<AFishActor>(AFishActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SP);
		F->Seed = 11u; F->PlaneOrigin = FVector(220.f, 0.f, 100.f);
		F->InitializeSwim();
		F->bPlayerControlled = true;
		F->SetInputDirection(FVector2D(1.f, 0.f));   // 아이가 오른쪽 키를 누르고 있다
		return F;
	};
	AFishActor* Clicked = Make();
	AFishActor* Control = Make();
	for (int32 i = 0; i < 60; ++i)
	{
		// 연타도 함께 본다: 초당 서너 번 찍어도 한 프레임도 밀리면 안 된다.
		if (i >= 10 && i % 15 == 0) { Clicked->ApplyFleeFrom(FVector(220.f, 0.f, 100.f)); }
		Clicked->StepSwim(1.f / 60.f);
		Control->StepSwim(1.f / 60.f);
		// 한 프레임도 예외가 없다는 뜻이므로 매 프레임 본다.
		const FVector Step = Clicked->GetActorLocation() - Control->GetActorLocation();
		if (Step.Size() >= 0.01f)
		{
			AddError(FString::Printf(TEXT("clicking my own fish moved it on frame %d (%f cm)"), i, Step.Size()));
			return false;
		}
	}
	TestTrue(TEXT("and it is still not fleeing"), Clicked->FleeState() == aquarium::BehaviorState::Normal);
	// 그래도 반응은 **있어야** 한다 -- 아무 일도 안 일어나면 그건 재롱이 아니라 무시다.
	TestTrue(TEXT("but it did react"), Clicked->PlayerReactionActive());
	return true;
}

#endif
