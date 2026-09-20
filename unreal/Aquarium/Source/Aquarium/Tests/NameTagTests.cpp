#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "AquariumGameMode.h"
#include "FishActor.h"
#include "NameTagComponent.h"
#include "UiFont.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNameTagOnlyOnPlayerFish, "Aquarium.NameTag.OnlyPlayerFishHasTag", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FNameTagOnlyOnPlayerFish::RunTest(const FString&)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AFishActor* Background = World->SpawnActor<AFishActor>(AFishActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, P);
	Background->InitializeSwim();
	AAquariumGameMode* GM = World->SpawnActor<AAquariumGameMode>(AAquariumGameMode::StaticClass(), FTransform::Identity, P);
	TArray<FFishSpecies> Catalog; FFishSpecies S; S.DisplayName = FText::FromString(TEXT("블루탱"));
	S.Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Fish/BlueTang/SK_BlueTang.SK_BlueTang"))); Catalog.Add(S);
	GM->SetCatalogForTest(Catalog, 5);
	TestEqual(TEXT("begin ok"), GM->BeginSession(TEXT("니모")), EBeginSessionResult::Ok);
	AFishActor* Player = GM->PlayerFish();
	if (!TestNotNull(TEXT("player fish"), Player)) return false;
	UNameTagComponent* Tag = Player->FindComponentByClass<UNameTagComponent>();
	if (!TestNotNull(TEXT("player has name tag"), Tag)) return false;
	TestEqual(TEXT("tag shows nickname"), Tag->DisplayedName().ToString(), FString(TEXT("니모")));
	TestTrue(TEXT("tag sits above the fish"), Tag->GetRelativeLocation().Z > 0.f);
	TestNull(TEXT("background fish has no tag"), Background->FindComponentByClass<UNameTagComponent>());
	GM->EndSession();
	int32 Tags = 0;
	for (TActorIterator<AFishActor> It(World); It; ++It) if (IsValid(*It) && !It->IsActorBeingDestroyed() && It->FindComponentByClass<UNameTagComponent>()) ++Tags;
	TestEqual(TEXT("no tags after end"), Tags, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUiFontIsKorean, "Aquarium.NameTag.UiFontResolvesKoreanFace", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FUiFontIsKorean::RunTest(const FString&)
{
	const FSlateFontInfo Info = FUiFont::Get(22);
	TestEqual(TEXT("size"), Info.Size, 22.f);
	TestTrue(TEXT("font object resolved (not engine default)"), FUiFont::IsKoreanFontLoaded());
	return true;
}
#endif
