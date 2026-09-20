#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"

class SWidget;

// Presentation-only values. All gameplay and local preference ownership stays with its existing owner.
namespace MazeInterfaceStyle
{
	inline const FLinearColor Ink = FLinearColor::FromSRGBColor(FColor(208, 219, 214));
	inline const FLinearColor Muted = FLinearColor::FromSRGBColor(FColor(126, 147, 136));
	inline const FLinearColor Accent = FLinearColor::FromSRGBColor(FColor(149, 184, 163));
	inline const FLinearColor Line = FLinearColor::FromSRGBColor(FColor(64, 83, 73));
	inline const FLinearColor Glass = FLinearColor::FromSRGBColor(FColor(7, 17, 13, 224));
	LABY_API FSlateFontInfo Font(int32 Size, int32 LetterSpacing = 120);
	LABY_API TSharedRef<SWidget> MakeCursor();
	LABY_API TSharedRef<SWidget> MakeLoadingScreen();
}
