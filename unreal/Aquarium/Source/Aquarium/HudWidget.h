#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HudWidget.generated.h"

class UButton;

// In-session overlay: a single "나가기" (exit) button in the top-right corner. Built in code.
UCLASS()
class AQUARIUM_API UHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Fired on every click; the owner ends the session and swaps back to the entry screen.
	FSimpleDelegate OnExit;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY() TObjectPtr<UButton> ExitButton = nullptr;

	UFUNCTION() void HandleExitClicked();
};
