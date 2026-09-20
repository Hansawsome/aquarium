#include "DiverPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraActor.h"
#include "Components/InputComponent.h"
#include "HudWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"

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
	switch (Result)
	{
	case EBeginSessionResult::EmptyCatalog:    return EEntryError::NoFishAvailable;
	case EBeginSessionResult::InvalidNickname: return UEntryWidget::ClassifyNickname(Raw);
	case EBeginSessionResult::Ok:
	case EBeginSessionResult::AlreadyActive:
	default:                                   return EEntryError::None;
	}
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
