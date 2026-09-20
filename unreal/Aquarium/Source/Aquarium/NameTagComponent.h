#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "NameTagComponent.generated.h"

// Screen-space name tag hovering above a fish (F-04). Attached only to the player's fish.
// The displayed text is held in memory only; it is never logged.
UCLASS()
class AQUARIUM_API UNameTagComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UNameTagComponent();

	// Gap (cm) between the top of the fish body bounds and the bottom edge of the label.
	UPROPERTY(EditAnywhere, Category = "NameTag") float HeightMargin = 6.f;

	void SetDisplayedName(const FText& InName);
	// The stored text; valid even when no widget exists (e.g. a test world without a local player).
	FText DisplayedName() const { return Name; }

private:
	FText Name;
};
