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

	// 소리 끄기 버튼이 눌렸다. 소유자가 오디오 서브시스템을 토글한다.
	FSimpleDelegate OnToggleMute;

	// 커서가 HUD 버튼 **아무거나** 위에 있는지. 버튼이 가져가는 클릭이 뒤의
	// 물고기를 놀래키거나 먹이를 뿌리면 안 된다(F-09, 그리고 이제 장면 3도).
	bool IsPointerOverButton() const;

	// 테스트용 접근자. 슬레이트 입력 없이 버튼 동작을 관측한다.
	class UImage* MuteIcon() const { return MuteImage; }
	class UTexture2D* CurrentMuteTexture() const;
	void SimulateMuteClick() { HandleMuteClicked(); }
	// 아이콘을 상태에 맞춰 바꾼다. 소유자가 뮤트를 토글한 뒤 부른다.
	void SetMutedVisual(bool bInMuted);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY() TObjectPtr<UButton> ExitButton = nullptr;
	UPROPERTY() TObjectPtr<UButton> MuteButton = nullptr;
	UPROPERTY() TObjectPtr<class UImage> MuteImage = nullptr;
	UPROPERTY() TObjectPtr<class UTexture2D> SoundOnTexture = nullptr;
	UPROPERTY() TObjectPtr<class UTexture2D> SoundOffTexture = nullptr;
	bool bMutedVisual = false;

	UFUNCTION() void HandleExitClicked();
	UFUNCTION() void HandleMuteClicked();
};
