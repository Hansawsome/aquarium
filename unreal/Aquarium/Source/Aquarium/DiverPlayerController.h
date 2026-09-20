#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "AquariumGameMode.h"
#include "EntryWidget.h"
#include "DiverPlayerController.generated.h"

class UHudWidget;

// Looks through the actor tagged "DiverCamera"; never follows the fish (P-06).
// Owns the entry screen and the in-session HUD and drives the entry <-> session flow (F-01..F-03).
UCLASS()
class AQUARIUM_API ADiverPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	ADiverPlayerController();

	// Entry screen up, HUD hidden, keyboard focus in the nickname box.
	void ShowEntry();
	// HUD up, entry hidden; the game keeps receiving keys (Esc) while the HUD button stays clickable.
	void ShowSession();
	// Ends the active session (button or Esc) and returns to the entry screen; no-op when idle.
	void RequestExit();

	// Maps a BeginSession verdict to the entry-screen error. Pure, so it is testable headless.
	static EEntryError EntryErrorFor(EBeginSessionResult Result, const FString& Raw);
	// Parses -AquariumAutoNickname=<name> [-AquariumAutoExitAfter=<sec>]; false when no nickname given.
	static bool ParseAutoReplay(const TCHAR* CmdLine, FString& OutName, float& OutExitAfter);

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	UPROPERTY() TObjectPtr<UEntryWidget> Entry = nullptr;
	UPROPERTY() TObjectPtr<UHudWidget> Hud = nullptr;
	FTimerHandle AutoSubmitTimer;
	FTimerHandle AutoExitTimer;

	void HandleSubmitted(const FString& Raw);
	AAquariumGameMode* GameMode() const;
	void StartAutoReplayIfRequested();
};
