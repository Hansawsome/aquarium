#include "HudWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Components/TextBlock.h"
#include "UiFont.h"

#define LOCTEXT_NAMESPACE "Aquarium"

namespace
{
	constexpr int32 kLabelFontSize = 20;
	constexpr float kEdgeMargin = 16.f;
	const FMargin kLabelPadding(24.f, 6.f);
	// Translucent light button / dark label: legible over the underwater scene without hiding it.
	const FLinearColor kButtonTint(0.98f, 0.9f, 0.7f, 0.75f);
	const FLinearColor kLabelTint(0.05f, 0.05f, 0.08f);
	// 나가기 버튼과 같은 여백, 그 **왼쪽**에 놓는다. 나가기는 우상단 구석에
	// 그대로 두고(M2 설계: 작고 눈에 안 띄는 자리), 뮤트는 그 옆이다.
	constexpr float kMuteSize = 44.f;
	constexpr float kMuteGap = 92.f;   // 나가기 버튼 너비 + 여백
}

void UHudWidget::HandleExitClicked()
{
	OnExit.ExecuteIfBound();
}

TSharedRef<SWidget> UHudWidget::RebuildWidget()
{
	if (WidgetTree && WidgetTree->RootWidget == nullptr)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		ExitButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ExitButton"));
		ExitButton->OnClicked.AddDynamic(this, &UHudWidget::HandleExitClicked);
		ExitButton->SetBackgroundColor(kButtonTint);

		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ExitLabel"));
		Label->SetFont(FUiFont::Get(kLabelFontSize));
		Label->SetColorAndOpacity(FSlateColor(kLabelTint));
		Label->SetJustification(ETextJustify::Center);
		Label->SetText(LOCTEXT("HudExit", "나가기"));
		if (UButtonSlot* LabelSlot = Cast<UButtonSlot>(ExitButton->AddChild(Label)))
		{
			LabelSlot->SetPadding(kLabelPadding);
		}

		if (UCanvasPanelSlot* ButtonSlot = Root->AddChildToCanvas(ExitButton))
		{
			ButtonSlot->SetAnchors(FAnchors(1.f, 0.f));
			ButtonSlot->SetAlignment(FVector2D(1.f, 0.f));
			ButtonSlot->SetPosition(FVector2D(-kEdgeMargin, kEdgeMargin));
			ButtonSlot->SetAutoSize(true);
		}

		SoundOnTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/T_SoundOn.T_SoundOn"));
		SoundOffTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/T_SoundOff.T_SoundOff"));

		MuteButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("MuteButton"));
		MuteButton->OnClicked.AddDynamic(this, &UHudWidget::HandleMuteClicked);
		MuteButton->SetBackgroundColor(kButtonTint);

		// 그림만 넣는다. 글자는 한 자도 넣지 않는다(시나리오: 아이는 글자를 읽지 않는다).
		MuteImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("MuteIconImage"));
		MuteImage->SetBrushSize(FVector2D(kMuteSize * 0.62f, kMuteSize * 0.62f));
		if (UButtonSlot* IconSlot = Cast<UButtonSlot>(MuteButton->AddChild(MuteImage)))
		{
			IconSlot->SetPadding(FMargin(6.f));
		}
		SetMutedVisual(bMutedVisual);

		if (UCanvasPanelSlot* MuteSlot = Root->AddChildToCanvas(MuteButton))
		{
			MuteSlot->SetAnchors(FAnchors(1.f, 0.f));
			MuteSlot->SetAlignment(FVector2D(1.f, 0.f));
			MuteSlot->SetPosition(FVector2D(-kEdgeMargin - kMuteGap, kEdgeMargin));
			MuteSlot->SetSize(FVector2D(kMuteSize, kMuteSize));
		}

		// 화면 구석의 숫자 하나. **왼쪽 아래**다 -- 나가기·뮤트가 오른쪽 위에 있고,
		// 물고기가 가장 적게 지나가는 구석이다.
		CountLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CatchCount"));
		CountLabel->SetFont(FUiFont::Get(30));
		CountLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.82f)));
		CountLabel->SetShadowOffset(FVector2D(1.f, 1.f));
		CountLabel->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
		CountLabel->SetText(FText::FromString(TEXT("0")));
		// 글자가 클릭을 먹으면 그 자리의 물고기를 놀래킬 수 없다(F-09).
		CountLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UCanvasPanelSlot* CountSlot = Root->AddChildToCanvas(CountLabel))
		{
			CountSlot->SetAnchors(FAnchors(0.f, 1.f));
			CountSlot->SetAlignment(FVector2D(0.f, 1.f));
			CountSlot->SetPosition(FVector2D(kEdgeMargin, -kEdgeMargin));
			CountSlot->SetAutoSize(true);
		}
	}
	return Super::RebuildWidget();
}

#undef LOCTEXT_NAMESPACE

void UHudWidget::HandleMuteClicked()
{
	OnToggleMute.ExecuteIfBound();
	SetMutedVisual(!bMutedVisual);
}

void UHudWidget::SetMutedVisual(bool bInMuted)
{
	bMutedVisual = bInMuted;
	UTexture2D* Tex = bMutedVisual ? SoundOffTexture.Get() : SoundOnTexture.Get();
	if (MuteImage && Tex) { MuteImage->SetBrushFromTexture(Tex, /*bMatchSize*/ false); }
}

UTexture2D* UHudWidget::CurrentMuteTexture() const
{
	return bMutedVisual ? SoundOffTexture.Get() : SoundOnTexture.Get();
}

bool UHudWidget::IsPointerOverButton() const
{
	return (ExitButton != nullptr && ExitButton->IsHovered())
		|| (MuteButton != nullptr && MuteButton->IsHovered());
}

void UHudWidget::SetCatchCount(int32 Count)
{
	if (CountLabel)
	{
		// FText::AsNumber는 로캘에 따라 천 단위 구분자를 넣는다. 최대 두 자리라
		// 지금은 문제가 없지만, 테스트가 "8"을 기대하므로 로캘에 기대지 않는다.
		CountLabel->SetText(FText::FromString(FString::FromInt(Count)));
	}
}

FString UHudWidget::CountText() const
{
	return CountLabel ? CountLabel->GetText().ToString() : FString();
}

bool UHudWidget::CountIsHitTestable() const
{
	return CountLabel != nullptr && CountLabel->GetVisibility() == ESlateVisibility::Visible;
}
