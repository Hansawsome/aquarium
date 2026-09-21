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

	// True while the cursor is over the exit button. Used by the click handler so a click the
	// button is taking does not also startle a fish behind it (F-09).
	bool IsPointerOverExitButton() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY() TObjectPtr<UButton> ExitButton = nullptr;

	UFUNCTION() void HandleExitClicked();
};
