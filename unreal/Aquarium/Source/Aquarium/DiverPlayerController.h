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

	// Arrow-key state, sampled every tick and turned into a swim-plane direction (F-05).
	struct FArrowKeys
	{
		bool bUp = false;
		bool bDown = false;
		bool bLeft = false;
		bool bRight = false;
	};
	// Screen-relative direction for a key state (x = right, y = up). Opposing keys cancel and
	// diagonals are normalized; delegates to aquarium::SteeringVector so the rule stays testable.
	static FVector2D DirectionFor(const FArrowKeys& Keys);

	// Entry screen up, HUD hidden, keyboard focus in the nickname box.
	void ShowEntry();
	// HUD up, entry hidden; the game keeps receiving keys (Esc) while the HUD button stays clickable.
	void ShowSession();
	// Ends the active session (button or Esc) and returns to the entry screen; no-op when idle.
	void RequestExit();

	// Clears every held arrow-key flag. Both ShowEntry() and ShowSession() install a UI-capturing
	// input mode (FInputModeUIOnly / FInputModeGameAndUI over a widget), which can swallow the
	// IE_Released for a key a child is still holding when the mode switches (e.g. Esc or the HUD
	// exit button while Right is held). Without this, a stale bRight=true survives into the next
	// session and drives the new fish hard right from frame one with no key actually held.
	void ResetArrowKeys();

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
	// Parses -AquariumAutoInput=<pattern>; false when absent or empty. Dev-only scripted arrow-key
	// playback, e.g. "R3,U2,L3,D2,0 1".
	static bool ParseAutoInput(const TCHAR* CmdLine, FString& OutPattern);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	// Test-only access to ArrowKeys, so Aquarium.Controller.ClearsHeldKeysOnSessionChange can set a
	// held key without a real input event and observe ShowEntry()/ShowSession() clear it.
	friend class FControllerClearsHeldKeysOnSessionChange;

	UPROPERTY() TObjectPtr<UEntryWidget> Entry = nullptr;
	UPROPERTY() TObjectPtr<UHudWidget> Hud = nullptr;
	FTimerHandle AutoSubmitTimer;
	FTimerHandle AutoExitTimer;
	FArrowKeys ArrowKeys;
	FDelegateHandle ActivationChangedHandle;

	void HandleSubmitted(const FString& Raw);
	void PressUp()    { ArrowKeys.bUp = true; }
	void ReleaseUp()  { ArrowKeys.bUp = false; }
	void PressDown()  { ArrowKeys.bDown = true; }
	void ReleaseDown(){ ArrowKeys.bDown = false; }
	void PressLeft()  { ArrowKeys.bLeft = true; }
	void ReleaseLeft(){ ArrowKeys.bLeft = false; }
	void PressRight() { ArrowKeys.bRight = true; }
	void ReleaseRight(){ ArrowKeys.bRight = false; }
	// Window focus gained/lost: releases every held key and pauses/resumes the player fish (F-06).
	void HandleApplicationActivationChanged(bool bIsActive);
	void ApplyInputToPlayerFish(float DeltaSeconds);
	void StartAutoInputIfRequested();
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
	// Dev-only scripted arrow-key playback: (keys, duration in seconds), looped.
	struct FAutoInputStep
	{
		FArrowKeys Keys;
		float Duration = 0.f;
	};
	TArray<FAutoInputStep> AutoInputSteps;
	float AutoInputElapsed = 0.f;
	// Overwrites ArrowKeys from the scripted pattern; no-op when no pattern was given.
	void AdvanceAutoInput(float DeltaSeconds);
	// Parses "R3,U2,0 1" into steps; malformed entries are skipped with a warning.
	static TArray<FAutoInputStep> BuildAutoInputSteps(const FString& Pattern);
};
