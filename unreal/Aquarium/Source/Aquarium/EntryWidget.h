#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EntryWidget.generated.h"

class UEditableTextBox;
class UButton;
class UTextBlock;

UENUM()
enum class EEntryError : uint8
{
	None,
	Empty,
	TooLong,
	InvalidCharacter,
	NoFishAvailable
};

DECLARE_DELEGATE_OneParam(FOnNicknameSubmitted, const FString& /*RawNickname*/);

// Nickname entry screen (F-01). Built in code; no widget blueprint.
UCLASS()
class AQUARIUM_API UEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Fired once per accepted submit with the raw (untrimmed) input; the owner decides the outcome
	// and calls ShowError or ResetForEntry. The button stays disabled until then (F-02).
	FOnNicknameSubmitted OnSubmitted;

	// Maps the rules-layer verdict to a UI error. Pure; no widget instance needed.
	static EEntryError ClassifyNickname(const FString& Raw);
	// Korean user-facing message for an error; empty for None.
	static FText MessageFor(EEntryError Error);

	void ShowError(EEntryError Error);
	// Clears text and error, re-enables the button, focuses the box.
	void ResetForEntry();
	// Disables the button while a submit is in flight (F-02).
	void SetSubmitting(bool bBusy);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY() TObjectPtr<UEditableTextBox> Input = nullptr;
	UPROPERTY() TObjectPtr<UButton> EnterButton = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock> ErrorText = nullptr;

	UFUNCTION() void HandleEnterClicked();
	UFUNCTION() void HandleTextCommitted(const FText& Text, ETextCommit::Type Method);
	void Submit();
};
