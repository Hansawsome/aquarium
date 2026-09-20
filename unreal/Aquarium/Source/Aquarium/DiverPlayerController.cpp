#include "DiverPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraActor.h"
#include "Components/InputComponent.h"
#include "HudWidget.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace
{
	constexpr int32 kEntryZOrder = 10;
	constexpr int32 kHudZOrder = 5;
	// Delay before the dev-only auto submit so the scene and fonts are on screen first.
	constexpr float kAutoSubmitDelay = 2.f;
}

ADiverPlayerController::ADiverPlayerController()
{
	// Prevent Possess/RestartPlayer from snapping the view back to the pawn; the fixed
	// DiverCamera view target set below must stick for the whole session (P-06).
	bAutoManageActiveCameraTarget = false;
	// Tick only does work while the dev-only UI capture is active (see StartUiCaptureIfRequested).
	PrimaryActorTick.bCanEverTick = true;
}

void ADiverPlayerController::BeginPlay()
{
	Super::BeginPlay();
	TArray<AActor*> Cameras;
	UGameplayStatics::GetAllActorsOfClassWithTag(GetWorld(), ACameraActor::StaticClass(), FName(TEXT("DiverCamera")), Cameras);
	if (Cameras.Num() > 0)
	{
		SetViewTarget(Cameras[0]);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("No ACameraActor tagged DiverCamera in %s"), *GetWorld()->GetMapName());
	}
	// Fallback for the no-widget path below; ShowEntry/ShowSession turn the cursor on.
	bShowMouseCursor = false;

	Entry = CreateWidget<UEntryWidget>(this, UEntryWidget::StaticClass());
	Hud = CreateWidget<UHudWidget>(this, UHudWidget::StaticClass());
	if (!Entry || !Hud)
	{
		UE_LOG(LogTemp, Error, TEXT("DiverPlayerController: failed to create UI widgets"));
		return;
	}
	Entry->OnSubmitted.BindUObject(this, &ADiverPlayerController::HandleSubmitted);
	Hud->OnExit.BindUObject(this, &ADiverPlayerController::RequestExit);
	Entry->AddToViewport(kEntryZOrder);
	Hud->AddToViewport(kHudZOrder);
	ShowEntry();
	StartAutoReplayIfRequested();
	StartUiCaptureIfRequested();
}

void ADiverPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
#if !UE_BUILD_SHIPPING
	// One UI-inclusive screenshot per tick: with -benchmark -fps=N the timestep is fixed, so the
	// frame index maps to N frames per second. -dumpmovie cannot be used for this because the
	// viewport client forces bShowUI=false while dumping a movie.
	if (!UiCaptureDir.IsEmpty() && !FScreenshotRequest::IsScreenshotRequested())
	{
		FScreenshotRequest::RequestScreenshot(
			FString::Printf(TEXT("%s/UiFrame%05d.png"), *UiCaptureDir, UiCaptureFrame++),
			/*bShowUI*/ true, /*bAddFilenameSuffix*/ false);
	}
#endif
}

void ADiverPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoSubmitTimer);
		World->GetTimerManager().ClearTimer(AutoExitTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void ADiverPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (InputComponent)
	{
		// Raw key binding: works under both the legacy and the Enhanced player input classes
		// without an action mapping or an input mapping context asset.
		InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ADiverPlayerController::RequestExit);
	}
}

void ADiverPlayerController::ShowEntry()
{
	if (!Entry || !Hud)
	{
		return;
	}
	Hud->SetVisibility(ESlateVisibility::Collapsed);
	Entry->SetVisibility(ESlateVisibility::Visible);
	Entry->ResetForEntry();
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Entry->GetInputSlateWidget());
	SetInputMode(Mode);
	bShowMouseCursor = true;
	Entry->FocusInput();
}

void ADiverPlayerController::ShowSession()
{
	if (!Entry || !Hud)
	{
		return;
	}
	Entry->SetVisibility(ESlateVisibility::Collapsed);
	Hud->SetVisibility(ESlateVisibility::Visible);
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	bShowMouseCursor = true;
}

EEntryError ADiverPlayerController::EntryErrorFor(EBeginSessionResult Result, const FString& Raw)
{
	// Exhaustive on purpose so a new EBeginSessionResult value fails to compile here.
	switch (Result)
	{
	case EBeginSessionResult::EmptyCatalog:    return EEntryError::NoFishAvailable;
	case EBeginSessionResult::InvalidNickname: return UEntryWidget::ClassifyNickname(Raw);
	// Ok never reaches this in practice (HandleSubmitted shows the session first); None keeps it total.
	case EBeginSessionResult::Ok:              return EEntryError::None;
	case EBeginSessionResult::AlreadyActive:   return EEntryError::None;
	}
	return EEntryError::None;
}

void ADiverPlayerController::HandleSubmitted(const FString& Raw)
{
	// The nickname is never logged.
	if (!Entry)
	{
		return;
	}
	AAquariumGameMode* GM = GameMode();
	if (!GM)
	{
		Entry->ShowError(EEntryError::NoFishAvailable);
		return;
	}
	const EBeginSessionResult Result = GM->BeginSession(Raw);
	if (Result == EBeginSessionResult::Ok)
	{
		ShowSession();
		return;
	}
	const EEntryError Error = EntryErrorFor(Result, Raw);
	if (Error != EEntryError::None)
	{
		Entry->ShowError(Error);
	}
	else
	{
		// AlreadyActive: nothing to show, just re-enable the button.
		Entry->SetSubmitting(false);
	}
}

void ADiverPlayerController::RequestExit()
{
	AAquariumGameMode* GM = GameMode();
	if (GM && GM->HasActiveSession())
	{
		GM->EndSession();
		ShowEntry();
	}
}

AAquariumGameMode* ADiverPlayerController::GameMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<AAquariumGameMode>() : nullptr;
}

bool ADiverPlayerController::ParseAutoReplay(const TCHAR* CmdLine, FString& OutName, float& OutExitAfter)
{
	OutName.Reset();
	OutExitAfter = 0.f;
	if (!CmdLine || !FParse::Value(CmdLine, TEXT("-AquariumAutoNickname="), OutName) || OutName.IsEmpty())
	{
		return false;
	}
	FParse::Value(CmdLine, TEXT("-AquariumAutoExitAfter="), OutExitAfter);
	return true;
}

void ADiverPlayerController::StartAutoReplayIfRequested()
{
#if !UE_BUILD_SHIPPING
	// Dev-only replay for reproducible captures: the command line supplies a nickname (test data,
	// never a real student's name) that is submitted after a short delay, optionally followed by
	// an automatic exit. Nothing here is logged.
	FString Name;
	float ExitAfter = 0.f;
	if (!ParseAutoReplay(FCommandLine::Get(), Name, ExitAfter))
	{
		return;
	}
	FTimerManager& Timers = GetWorldTimerManager();
	Timers.SetTimer(AutoSubmitTimer, FTimerDelegate::CreateWeakLambda(this, [this, Name]()
	{
		if (Entry)
		{
			Entry->SetSubmitting(true);
		}
		HandleSubmitted(Name);
	}), kAutoSubmitDelay, false);
	if (ExitAfter > 0.f)
	{
		Timers.SetTimer(AutoExitTimer, this, &ADiverPlayerController::RequestExit, kAutoSubmitDelay + ExitAfter, false);
	}
#endif
}

bool ADiverPlayerController::ParseUiCaptureDir(const TCHAR* CmdLine, FString& OutDir)
{
	OutDir.Reset();
	if (!CmdLine || !FParse::Value(CmdLine, TEXT("-AquariumCaptureUI="), OutDir))
	{
		return false;
	}
	OutDir.TrimStartAndEndInline();
	return !OutDir.IsEmpty();
}

void ADiverPlayerController::StartUiCaptureIfRequested()
{
#if !UE_BUILD_SHIPPING
	FString Dir;
	if (!ParseUiCaptureDir(FCommandLine::Get(), Dir))
	{
		return;
	}
	if (!IFileManager::Get().MakeDirectory(*Dir, /*Tree*/ true))
	{
		UE_LOG(LogTemp, Warning, TEXT("AquariumCaptureUI: cannot create %s"), *Dir);
		return;
	}
	UiCaptureDir = Dir;
	UiCaptureFrame = 0;
#endif
}
