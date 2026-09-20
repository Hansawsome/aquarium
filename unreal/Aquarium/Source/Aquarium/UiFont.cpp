#include "UiFont.h"

#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Fonts/CompositeFont.h"
#include "Styling/CoreStyle.h"
#include "UObject/Package.h"

namespace
{
	TWeakObjectPtr<UFont> GRuntimeFont;

	UFont* BuildRuntimeFont()
	{
		UFontFace* Face = LoadObject<UFontFace>(nullptr, TEXT("/Game/UI/FF_NotoSansKR.FF_NotoSansKR"));
		if (Face == nullptr)
		{
			static bool bWarned = false;
			if (!bWarned)
			{
				bWarned = true;
				UE_LOG(LogTemp, Warning, TEXT("Noto Sans KR font face missing; using engine default font"));
			}
			return nullptr;
		}
		UFont* Font = NewObject<UFont>(GetTransientPackage(), TEXT("F_NotoSansKR_Runtime"), RF_Transient);
		Font->FontCacheType = EFontCacheType::Runtime;
		// FTypefaceEntry has no (Name, FFontData) constructor in 5.8; assign the face data explicitly.
		FTypefaceEntry Entry(TEXT("Regular"));
		Entry.Font = FFontData(Face);
		Font->GetMutableInternalCompositeFont().DefaultTypeface.Fonts.Add(MoveTemp(Entry));
		Font->AddToRoot();
		return Font;
	}

	UFont* ResolveFont()
	{
		if (!GRuntimeFont.IsValid())
		{
			GRuntimeFont = BuildRuntimeFont();
		}
		return GRuntimeFont.Get();
	}
}

FSlateFontInfo FUiFont::Get(int32 Size)
{
	if (UFont* Font = ResolveFont())
	{
		return FSlateFontInfo(Font, Size, TEXT("Regular"));
	}
	return FCoreStyle::GetDefaultFontStyle("Regular", Size);
}

bool FUiFont::IsKoreanFontLoaded()
{
	return GRuntimeFont.IsValid();
}
