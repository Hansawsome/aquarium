#include "DiverPlayerController.h"

#include "AquariumAudioSubsystem.h"

#include "FishSchoolSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraActor.h"
#include "Components/InputComponent.h"
#include "HudWidget.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "FishActor.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

#include "aquarium/Steering.h"

namespace
{
	constexpr int32 kEntryZOrder = 10;
	constexpr int32 kHudZOrder = 5;
	// Delay before the dev-only auto submit so the scene and fonts are on screen first.
	constexpr float kAutoSubmitDelay = 2.f;
	// Upper bound on the dev-only frame-time buffer (about 55 minutes at 60 fps).
	constexpr int32 kMaxFrameSamples = 200000;
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
	ApplyAssignmentSeedIfRequested();
	StartAutoReplayIfRequested();
	StartUiCaptureIfRequested();
	StartFrameStatsIfRequested();
	StartAutoInputIfRequested();
	StartAutoClickIfRequested();
	StartClickLogIfRequested();

	// F-06: focus loss releases input and pauses the simulation; focus gain resumes it.
	if (FSlateApplication::IsInitialized())
	{
		ActivationChangedHandle = FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(
			this, &ADiverPlayerController::HandleApplicationActivationChanged);
	}
}

FVector2D ADiverPlayerController::DirectionFor(const FArrowKeys& Keys)
{
	aquarium::KeyState State;
	State.up = Keys.bUp;
	State.down = Keys.bDown;
	State.left = Keys.bLeft;
	State.right = Keys.bRight;
	const aquarium::Vec2 V = aquarium::SteeringVector(State);
	return FVector2D(V.x, V.y);
}

void ADiverPlayerController::ResetArrowKeys()
{
	ArrowKeys = FArrowKeys();
}

void ADiverPlayerController::HandleApplicationActivationChanged(bool bIsActive)
{
	// Releasing the keys first means a key held across the focus change cannot stay stuck down:
	// the OS never delivers its IE_Released to us while the window is in the background.
	ResetArrowKeys();
	AAquariumGameMode* GM = GameMode();
	if (GM == nullptr)
	{
		return;
	}
	if (AFishActor* Fish = GM->PlayerFish())
	{
		Fish->SetInputDirection(FVector2D::ZeroVector);
		Fish->SetPaused(!bIsActive);
	}
}

void ADiverPlayerController::ApplyInputToPlayerFish(float DeltaSeconds)
{
	AAquariumGameMode* GM = GameMode();
	if (GM == nullptr || !GM->HasActiveSession())
	{
		return;
	}
	AFishActor* Fish = GM->PlayerFish();
	if (Fish == nullptr)
	{
		return;
	}
	Fish->SetInputDirection(DirectionFor(ArrowKeys));
	// 장면 1: 방향키를 누르는 순간 물살 소리가 나고, 빠를수록 음이 올라간다.
	// 소리 규칙 자체는 규칙 계층에 있고, 여기서는 속도를 건네줄 뿐이다.
	if (UWorld* W = GetWorld())
	{
		if (UAquariumAudioSubsystem* Audio = W->GetSubsystem<UAquariumAudioSubsystem>())
		{
			Audio->UpdateSwim(Fish->CurrentSpeed(), Fish->MaxSpeed, DeltaSeconds);
		}
	}
}

void ADiverPlayerController::HandleClick()
{
	// The HUD exit button sits on top of the scene; a click that the button is taking must not
	// also startle whatever fish happens to be behind it. Slate handles the button itself, but
	// under FInputModeGameAndUI the key still reaches us, so this guard is ours to make.
	if (Hud && Hud->IsPointerOverExitButton())
	{
		return;
	}
	float X = 0.f, Y = 0.f;
	if (!GetMousePosition(X, Y))
	{
		return;
	}
	HandleClickAt(FVector2D(X, Y));
}

bool ADiverPlayerController::HandleClickAt(const FVector2D& ViewportPos)
{
	// The ONE place the engine's projection maths is used. Writing a closed-form screen -> plane
	// formula here would mean copying the FOV, aspect and near plane into a second place, which
	// is the duplicated-rule trap that hid the prop lane bug until M4b.
	FVector WorldOrigin = FVector::ZeroVector;
	FVector WorldDir = FVector::ZeroVector;
	if (!DeprojectScreenPositionToWorld(static_cast<float>(ViewportPos.X), static_cast<float>(ViewportPos.Y),
	                                    WorldOrigin, WorldDir))
	{
		return false;
	}
	const bool bHandled = HandleClickRay(WorldOrigin, WorldDir);
#if !UE_BUILD_SHIPPING
	if (!ClickLogPath.IsEmpty())
	{
		FVector2D Size(1.f, 1.f);
		if (GEngine && GEngine->GameViewport) { GEngine->GameViewport->GetViewportSize(Size); }
		// Columns only: index, time, where on screen, what was hit. Never a nickname (P-03).
		ClickLogRows.Add(FString::Printf(TEXT("%d,%.3f,%.4f,%.4f,%.1f,%d,%s"),
			ClickLogRows.Num() - 1, AutoClickElapsed,
			ViewportPos.X / FMath::Max(Size.X, 1.f), ViewportPos.Y / FMath::Max(Size.Y, 1.f),
			LastClickPlaneX, bHandled ? 1 : 0, *LastClickState));
	}
#endif
	return bHandled;
}

bool ADiverPlayerController::HandleClickRay(const FVector& RayOrigin, const FVector& RayDir)
{
	LastClickPlaneX = 0.f;
	LastClickState = TEXT("None");
	AAquariumGameMode* GM = GameMode();
	if (GM == nullptr || !GM->HasActiveSession())
	{
		return false;   // the entry screen is up; clicking the nickname box startles nobody
	}
	UWorld* W = GetWorld();
	UFishSchoolSubsystem* School = W ? W->GetSubsystem<UFishSchoolSubsystem>() : nullptr;
	if (School == nullptr)
	{
		return false;
	}
	FVector Hit = FVector::ZeroVector;
	AFishActor* Fish = School->PickFrontmostHit(RayOrigin, RayDir, Hit);
	if (Fish == nullptr)
	{
		return false;   // F-09: empty water affects nothing
	}
	LastClickPlaneX = static_cast<float>(Hit.X);
	switch (Fish->FleeState())
	{
	case aquarium::BehaviorState::Fleeing:    LastClickState = TEXT("Fleeing"); break;
	case aquarium::BehaviorState::Recovering: LastClickState = TEXT("Recovering"); break;
	default:                                  LastClickState = TEXT("Normal"); break;
	}
	Fish->ApplyFleeFrom(Hit);   // exactly one fish per click
	return true;
}

void ADiverPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
#if !UE_BUILD_SHIPPING
	AdvanceAutoInput(DeltaSeconds);
	AdvanceAutoClick(DeltaSeconds);
#endif
	ApplyInputToPlayerFish(DeltaSeconds);
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
	if (!FrameStatsPath.IsEmpty() && FrameDeltas.Num() < kMaxFrameSamples)
	{
		FrameDeltas.Add(DeltaSeconds);
	}
#endif
}

void ADiverPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ActivationChangedHandle.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(ActivationChangedHandle);
	}
	ActivationChangedHandle.Reset();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoSubmitTimer);
		World->GetTimerManager().ClearTimer(AutoExitTimer);
	}
#if !UE_BUILD_SHIPPING
	WriteFrameStats();
	WriteClickLog();
#endif
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
		// Arrow keys, pressed and released, so a held key keeps steering the fish (F-05).
		InputComponent->BindKey(EKeys::Up, IE_Pressed, this, &ADiverPlayerController::PressUp);
		InputComponent->BindKey(EKeys::Up, IE_Released, this, &ADiverPlayerController::ReleaseUp);
		InputComponent->BindKey(EKeys::Down, IE_Pressed, this, &ADiverPlayerController::PressDown);
		InputComponent->BindKey(EKeys::Down, IE_Released, this, &ADiverPlayerController::ReleaseDown);
		InputComponent->BindKey(EKeys::Left, IE_Pressed, this, &ADiverPlayerController::PressLeft);
		InputComponent->BindKey(EKeys::Left, IE_Released, this, &ADiverPlayerController::ReleaseLeft);
		InputComponent->BindKey(EKeys::Right, IE_Pressed, this, &ADiverPlayerController::PressRight);
		InputComponent->BindKey(EKeys::Right, IE_Released, this, &ADiverPlayerController::ReleaseRight);
		// F-09. IE_Pressed ONLY. macOS delivers BOTH IE_Pressed and IE_DoubleClick for the second
		// click of a fast double click, so binding the double click as well would make one
		// physical click of a mashing child count twice.
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ADiverPlayerController::HandleClick);
	}
}

void ADiverPlayerController::ShowEntry()
{
	// Clear any held arrow key before FInputModeUIOnly below takes over: that mode can swallow the
	// IE_Released for a key a child is still holding (Esc/HUD exit while Right is held), which
	// would otherwise leave ArrowKeys stuck and drive the next session's fish from frame one.
	ResetArrowKeys();
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
	// Same reasoning as ShowEntry(): FInputModeGameAndUI below can swallow a key-up mid-switch, so
	// start the new session with a clean slate rather than inheriting a stuck key from the moment
	// the widgets were toggled.
	ResetArrowKeys();
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
		// Belt-and-braces: stop steering the outgoing fish before EndSession() destroys it, so
		// nothing is left driven by a key this controller no longer owns. ShowEntry() (called
		// below) is what actually protects the NEXT session by clearing ArrowKeys.
		if (AFishActor* Fish = GM->PlayerFish())
		{
			Fish->SetInputDirection(FVector2D::ZeroVector);
		}
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

bool ADiverPlayerController::ParseAssignmentSeed(const TCHAR* CmdLine, int32& OutSeed)
{
	OutSeed = 0;
	FString Value;
	if (!CmdLine || !FParse::Value(CmdLine, TEXT("-AquariumAssignmentSeed="), Value) || !Value.IsNumeric())
	{
		return false;
	}
	OutSeed = FCString::Atoi(*Value);
	return OutSeed != 0;
}

void ADiverPlayerController::ApplyAssignmentSeedIfRequested()
{
#if !UE_BUILD_SHIPPING
	// Dev-only: pins the species assignment (and the player fish's swim seed) so review captures
	// are reproducible run to run.
	int32 Seed = 0;
	if (!ParseAssignmentSeed(FCommandLine::Get(), Seed))
	{
		return;
	}
	if (AAquariumGameMode* GM = GameMode())
	{
		GM->SetAssignmentSeed(Seed);
	}
#endif
}

bool ADiverPlayerController::ParseFrameStatsPath(const TCHAR* CmdLine, FString& OutPath)
{
	OutPath.Reset();
	if (!CmdLine || !FParse::Value(CmdLine, TEXT("-AquariumFrameStats="), OutPath))
	{
		return false;
	}
	OutPath.TrimStartAndEndInline();
	return !OutPath.IsEmpty();
}

void ADiverPlayerController::StartFrameStatsIfRequested()
{
#if !UE_BUILD_SHIPPING
	// Dev-only: records one DeltaSeconds per tick and dumps them as CSV on EndPlay, so a
	// performance run can be analysed offline. Only timings are written, never a nickname.
	FString Path;
	if (!ParseFrameStatsPath(FCommandLine::Get(), Path))
	{
		return;
	}
	FrameStatsPath = Path;
	FrameDeltas.Reset();
	FrameDeltas.Reserve(kMaxFrameSamples);
#endif
}

void ADiverPlayerController::WriteFrameStats()
{
#if !UE_BUILD_SHIPPING
	if (FrameStatsPath.IsEmpty())
	{
		return;
	}
	FString Csv = TEXT("frame,delta_seconds\n");
	for (int32 Index = 0; Index < FrameDeltas.Num(); ++Index)
	{
		Csv += FString::Printf(TEXT("%d,%.6f\n"), Index, FrameDeltas[Index]);
	}
	if (FFileHelper::SaveStringToFile(Csv, *FrameStatsPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("AquariumFrameStats: wrote %d samples to %s"), FrameDeltas.Num(), *FrameStatsPath);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AquariumFrameStats: cannot write %s"), *FrameStatsPath);
	}
	FrameStatsPath.Reset();
	FrameDeltas.Empty();
#endif
}

bool ADiverPlayerController::ParseAutoInput(const TCHAR* CmdLine, FString& OutPattern)
{
	OutPattern.Reset();
	// bShouldStopOnSeparator=false so an unquoted comma-separated pattern survives intact.
	if (!CmdLine || !FParse::Value(CmdLine, TEXT("-AquariumAutoInput="), OutPattern, /*bShouldStopOnSeparator*/ false))
	{
		return false;
	}
	OutPattern.TrimStartAndEndInline();
	return !OutPattern.IsEmpty();
}

TArray<ADiverPlayerController::FAutoInputStep> ADiverPlayerController::BuildAutoInputSteps(const FString& Pattern)
{
	TArray<FAutoInputStep> Steps;
	TArray<FString> Tokens;
	Pattern.ParseIntoArray(Tokens, TEXT(","), /*CullEmpty*/ true);
	for (FString Token : Tokens)
	{
		Token.TrimStartAndEndInline();
		if (Token.IsEmpty())
		{
			continue;
		}
		const TCHAR Dir = FChar::ToUpper(Token[0]);
		FString Rest = Token.Mid(1);
		Rest.TrimStartAndEndInline();
		const float Seconds = FCString::Atof(*Rest);
		if (Rest.IsEmpty() || !(Seconds > 0.f))
		{
			// The pattern is dev test data, so it is safe to echo; it never carries a nickname.
			UE_LOG(LogTemp, Warning, TEXT("AquariumAutoInput: bad duration in '%s'; entry ignored"), *Token);
			continue;
		}
		FAutoInputStep Step;
		Step.Duration = Seconds;
		switch (Dir)
		{
		case TEXT('R'): Step.Keys.bRight = true; break;
		case TEXT('L'): Step.Keys.bLeft = true; break;
		case TEXT('U'): Step.Keys.bUp = true; break;
		case TEXT('D'): Step.Keys.bDown = true; break;
		case TEXT('0'): break;   // no keys held
		default:
			UE_LOG(LogTemp, Warning, TEXT("AquariumAutoInput: unknown direction in '%s'; entry ignored"), *Token);
			continue;
		}
		Steps.Add(Step);
	}
	return Steps;
}

void ADiverPlayerController::StartAutoInputIfRequested()
{
#if !UE_BUILD_SHIPPING
	// Dev-only: replays a fixed arrow-key script so a capture shows the same swim path every run.
	AutoInputSteps.Reset();
	AutoInputElapsed = 0.f;
	FString Pattern;
	if (!ParseAutoInput(FCommandLine::Get(), Pattern))
	{
		return;
	}
	AutoInputSteps = BuildAutoInputSteps(Pattern);
	if (AutoInputSteps.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("AquariumAutoInput: no usable entries; scripted input disabled"));
	}
#endif
}

void ADiverPlayerController::AdvanceAutoInput(float DeltaSeconds)
{
#if !UE_BUILD_SHIPPING
	if (AutoInputSteps.Num() == 0)
	{
		return;
	}
	float Total = 0.f;
	for (const FAutoInputStep& Step : AutoInputSteps)
	{
		Total += Step.Duration;
	}
	if (!(Total > 0.f))
	{
		return;
	}
	AutoInputElapsed = FMath::Fmod(AutoInputElapsed + DeltaSeconds, Total);
	float Cursor = AutoInputElapsed;
	for (const FAutoInputStep& Step : AutoInputSteps)
	{
		if (Cursor < Step.Duration)
		{
			// Overwrites live key state on purpose: the script owns the input while it runs.
			ArrowKeys = Step.Keys;
			return;
		}
		Cursor -= Step.Duration;
	}
	ArrowKeys = AutoInputSteps.Last().Keys;
#endif
}

bool ADiverPlayerController::ParseAutoClick(const TCHAR* CmdLine, FString& OutPattern)
{
	OutPattern.Reset();
	// bShouldStopOnSeparator=false so an unquoted comma-separated pattern survives intact,
	// exactly as ParseAutoInput does.
	if (!CmdLine || !FParse::Value(CmdLine, TEXT("-AquariumAutoClick="), OutPattern, /*bShouldStopOnSeparator*/ false))
	{
		return false;
	}
	OutPattern.TrimStartAndEndInline();
	return !OutPattern.IsEmpty();
}

TArray<ADiverPlayerController::FAutoClick> ADiverPlayerController::BuildAutoClicks(const FString& Pattern)
{
	TArray<FAutoClick> Clicks;
	TArray<FString> Tokens;
	Pattern.ParseIntoArray(Tokens, TEXT(","), /*CullEmpty*/ true);
	for (FString Token : Tokens)
	{
		Token.TrimStartAndEndInline();
		if (Token.IsEmpty())
		{
			continue;
		}
		FString TimePart, CoordPart, XPart, YPart;
		const bool bSplitAt = Token.Split(TEXT("@"), &TimePart, &CoordPart);
		const bool bSplitX = bSplitAt && CoordPart.Split(TEXT("x"), &XPart, &YPart);
		if (!bSplitX)
		{
			// The token is dev test data, so echoing it is safe; it never carries a nickname.
			UE_LOG(LogTemp, Warning, TEXT("AquariumAutoClick: bad token '%s'; entry ignored"), *Token);
			continue;
		}
		TimePart.TrimStartAndEndInline();
		XPart.TrimStartAndEndInline();
		YPart.TrimStartAndEndInline();
		// IsNumeric() also rejects a second 'x' ("0.5x0.5"), so "4@0.5x0.5x0.5" is dropped here.
		if (!TimePart.IsNumeric() || !XPart.IsNumeric() || !YPart.IsNumeric())
		{
			UE_LOG(LogTemp, Warning, TEXT("AquariumAutoClick: bad token '%s'; entry ignored"), *Token);
			continue;
		}
		const float T = FCString::Atof(*TimePart);
		const float Nx = FCString::Atof(*XPart);
		const float Ny = FCString::Atof(*YPart);
		if (!(T > 0.f) || Nx < 0.f || Nx > 1.f || Ny < 0.f || Ny > 1.f)
		{
			UE_LOG(LogTemp, Warning, TEXT("AquariumAutoClick: coords out of range in '%s'; entry ignored"), *Token);
			continue;
		}
		FAutoClick C;
		C.TimeSeconds = T;
		C.Normalized = FVector2D(Nx, Ny);
		Clicks.Add(C);
	}
	// Fire in time order whatever order they were written in.
	Clicks.Sort([](const FAutoClick& A, const FAutoClick& B) { return A.TimeSeconds < B.TimeSeconds; });
	return Clicks;
}

void ADiverPlayerController::StartAutoClickIfRequested()
{
#if !UE_BUILD_SHIPPING
	AutoClicks.Reset();
	NextAutoClick = 0;
	AutoClickElapsed = 0.f;
	FString Pattern;
	if (!ParseAutoClick(FCommandLine::Get(), Pattern))
	{
		return;
	}
	AutoClicks = BuildAutoClicks(Pattern);
	// ALWAYS logged, even for 0: a harness must be able to assert the armed count rather than
	// discovering after the fact that its whole script was dropped by the parser.
	UE_LOG(LogTemp, Warning, TEXT("AquariumAutoClick: armed %d clicks"), AutoClicks.Num());
#endif
}

void ADiverPlayerController::AdvanceAutoClick(float DeltaSeconds)
{
#if !UE_BUILD_SHIPPING
	if (NextAutoClick >= AutoClicks.Num())
	{
		return;
	}
	AutoClickElapsed += DeltaSeconds;
	FVector2D ViewportSize = FVector2D::ZeroVector;
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}
	if (ViewportSize.X <= 0.f || ViewportSize.Y <= 0.f)
	{
		return;   // no viewport yet (or -nullrhi): nothing to aim at
	}
	while (NextAutoClick < AutoClicks.Num() && AutoClicks[NextAutoClick].TimeSeconds <= AutoClickElapsed)
	{
		const FAutoClick& C = AutoClicks[NextAutoClick++];
		// Same entry point as a real mouse click: one code path, so the capture verifies the
		// thing the child will actually use, deprojection included.
		HandleClickAt(FVector2D(C.Normalized.X * ViewportSize.X, C.Normalized.Y * ViewportSize.Y));
	}
#endif
}

bool ADiverPlayerController::ParseClickLogPath(const TCHAR* CmdLine, FString& OutPath)
{
	OutPath.Reset();
	if (!CmdLine || !FParse::Value(CmdLine, TEXT("-AquariumClickLog="), OutPath))
	{
		return false;
	}
	OutPath.TrimStartAndEndInline();
	return !OutPath.IsEmpty();
}

void ADiverPlayerController::StartClickLogIfRequested()
{
#if !UE_BUILD_SHIPPING
	FString Path;
	if (!ParseClickLogPath(FCommandLine::Get(), Path))
	{
		return;
	}
	ClickLogPath = Path;
	ClickLogRows.Reset();
	ClickLogRows.Add(TEXT("index,time_s,ndc_x,ndc_y,hit_plane_x,hit,state_before"));
#endif
}

void ADiverPlayerController::WriteClickLog()
{
#if !UE_BUILD_SHIPPING
	if (ClickLogPath.IsEmpty())
	{
		return;
	}
	FString Csv;
	for (const FString& Row : ClickLogRows) { Csv += Row + TEXT("\n"); }
	if (FFileHelper::SaveStringToFile(Csv, *ClickLogPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("AquariumClickLog: wrote %d clicks to %s"),
			ClickLogRows.Num() - 1, *ClickLogPath);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AquariumClickLog: cannot write %s"), *ClickLogPath);
	}
	ClickLogPath.Reset();
	ClickLogRows.Empty();
#endif
}
