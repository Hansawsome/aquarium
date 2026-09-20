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
	// Parses -AquariumCaptureUI=<absolute dir>; false when absent or empty. Dev-only frame capture
	// that, unlike -dumpmovie, includes the Slate/UMG layer in every frame.
	static bool ParseUiCaptureDir(const TCHAR* CmdLine, FString& OutDir);
	// Parses -AquariumAssignmentSeed=<int>; false when absent, non-numeric or 0.
	static bool ParseAssignmentSeed(const TCHAR* CmdLine, int32& OutSeed);
	// Parses -AquariumFrameStats=<absolute csv path>; false when absent or empty. Dev-only
	// per-frame delta time recording, written out as CSV on EndPlay.
	static bool ParseFrameStatsPath(const TCHAR* CmdLine, FString& OutPath);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY() TObjectPtr<UEntryWidget> Entry = nullptr;
	UPROPERTY() TObjectPtr<UHudWidget> Hud = nullptr;
	FTimerHandle AutoSubmitTimer;
	FTimerHandle AutoExitTimer;

	void HandleSubmitted(const FString& Raw);
	AAquariumGameMode* GameMode() const;
	void StartAutoReplayIfRequested();
	void StartUiCaptureIfRequested();
	void ApplyAssignmentSeedIfRequested();
	void StartFrameStatsIfRequested();
	void WriteFrameStats();
	// Non-empty while the dev-only UI capture is active; one screenshot request per tick.
	FString UiCaptureDir;
	int32 UiCaptureFrame = 0;
	// Non-empty while the dev-only frame-time recording is active; one DeltaSeconds per tick.
	FString FrameStatsPath;
	TArray<float> FrameDeltas;
};
