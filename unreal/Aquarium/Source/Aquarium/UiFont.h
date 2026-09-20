#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"

// Single source for the UI font (F-04). UE 5.8 Python cannot compose a UFont, so only the
// Noto Sans KR FontFace asset exists in Content; this composes one runtime UFont from it.
struct AQUARIUM_API FUiFont
{
	// Slate font info for the Korean UI face at the given size. Falls back to the engine
	// default font (with a one-time warning) when the face asset cannot be loaded.
	static FSlateFontInfo Get(int32 Size);
	// True when the last Get() resolved the Noto Sans KR face rather than the fallback.
	static bool IsKoreanFontLoaded();
};
