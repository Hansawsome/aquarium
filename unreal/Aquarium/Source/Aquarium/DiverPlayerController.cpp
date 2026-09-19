#include "DiverPlayerController.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"

void ADiverPlayerController::BeginPlay()
{
	Super::BeginPlay();
	TArray<AActor*> Cameras;
	UGameplayStatics::GetAllActorsOfClassWithTag(GetWorld(), ACameraActor::StaticClass(), FName(TEXT("DiverCamera")), Cameras);
	if (Cameras.Num() > 0) SetViewTarget(Cameras[0]);
	bShowMouseCursor = false;
}
