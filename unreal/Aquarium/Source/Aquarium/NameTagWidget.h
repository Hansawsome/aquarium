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
	// 도장 모양으로 바꾼다: 더 작고 더 옅다. 36마리가 같은 크기의 흰 글자를 이고
	// 다니면 화면이 글자밭이 되고, 그러면 내 물고기가 어느 것인지도 안 보인다.
	void SetStampStyle(bool bInStamp);
	bool IsStampStyle() const { return bStampStyle; }
	int32 FontSize() const { return bStampStyle ? StampFontSize() : OwnerFontSize(); }
	float Opacity() const { return bStampStyle ? 0.62f : 1.f; }
	static int32 OwnerFontSize() { return 22; }
	static int32 StampFontSize() { return 14; }
	// **실제로 그려지는 값**을 읽는다. 상태 변수만 보는 단언은 글꼴 갱신을 지우는
	// 변이에 초록불인 채로 남는다 -- M7의 뮤트 버튼에서 정확히 그 일이 있었다.
	int32 AppliedFontSize() const;
	float AppliedOpacity() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY() TObjectPtr<UTextBlock> Label = nullptr;
	// Kept so a name set before the Slate widget exists is applied on rebuild.
	FText PendingName;
	bool bStampStyle = false;
};
