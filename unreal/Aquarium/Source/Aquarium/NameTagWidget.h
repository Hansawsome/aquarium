#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NameTagWidget.generated.h"

class UTextBlock;

// A single centered text block in the Korean UI font; content of the player fish name tag (F-04).
UCLASS()
class AQUARIUM_API UNameTagWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetName(const FText& InName);
	FText DisplayedName() const { return PendingName; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY() TObjectPtr<UTextBlock> Label = nullptr;
	// Kept so a name set before the Slate widget exists is applied on rebuild.
	FText PendingName;
};
