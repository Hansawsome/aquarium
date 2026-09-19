#include "DiverPlayerController.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"

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
}
