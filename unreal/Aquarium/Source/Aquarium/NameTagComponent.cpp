#include "NameTagComponent.h"

#include "NameTagWidget.h"

UNameTagComponent::UNameTagComponent()
{
	SetWidgetSpace(EWidgetSpace::Screen);
	SetDrawAtDesiredSize(true);
	SetWidgetClass(UNameTagWidget::StaticClass());
	// Bottom-center pivot: the label's bottom edge sits at the component's projected location.
	SetPivot(FVector2D(0.5f, 1.0f));
	SetRelativeLocation(FVector(0.f, 0.f, 20.f));
}

void UNameTagComponent::SetDisplayedName(const FText& InName)
{
	Name = InName;
	if (GetUserWidgetObject() == nullptr)
	{
		InitWidget();
	}
	if (UNameTagWidget* Widget = Cast<UNameTagWidget>(GetUserWidgetObject()))
	{
		Widget->SetName(Name);
	}
}
