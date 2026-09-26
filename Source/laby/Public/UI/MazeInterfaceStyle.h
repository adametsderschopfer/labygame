#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"

class SWidget;

struct FMazePreparationStatus;

// Presentation-only values. All gameplay and local preference ownership stays with its existing owner.
namespace MazeInterfaceStyle
{
	struct FPalette
	{
		FLinearColor Ink, Muted, Accent, Line, Edge, Glass;
		FLinearColor BackgroundLow, BackgroundMid, BackgroundHigh;
		FLinearColor HoverSurface, HoverAccent, Pressed, OnAccent, Track, Room;
	};

	// Active MI_LabVinylSatin's inherited FloorColor, in linear RGB.
	// Shared value helper: no material loads or cross-module mutable state.
	inline FPalette Palette()
	{
		const FLinearColor Floor(0.20f, 0.265f, 0.235f);
		const auto Shade = [&Floor](float Strength)
		{
			return FLinearColor(Floor.R * Strength, Floor.G * Strength, Floor.B * Strength, 1.f);
		};

		return {FMath::Lerp(Floor, FLinearColor::White, .78f),
		        FMath::Lerp(Floor, FLinearColor::White, .12f),
		        Shade(1.6f),
		        Shade(.52f),
		        Shade(.90f),
		        Shade(.035f),
		        Shade(.018f),
		        Shade(.06f),
		        Shade(.11f),
		        Shade(.18f),
		        FMath::Lerp(Shade(1.6f), FLinearColor::White, .18f),
		        Shade(1.05f),
		        Shade(.02f),
		        Shade(.014f),
		        Shade(1.1f)};
	}

	// Compatibility for the inactive legacy authoring tools; Ward uses Palette() directly.
	inline const FLinearColor Ink = Palette().Ink;
	inline const FLinearColor Muted = Palette().Muted;
	inline const FLinearColor Accent = Palette().Accent;
	inline const FLinearColor Line = Palette().Line;
	inline const FLinearColor Glass = Palette().Glass.CopyWithNewOpacity(244.f / 255.f);
	LABY_API FSlateFontInfo Font(int32 Size, int32 LetterSpacing = 120);
	LABY_API TSharedRef<SWidget> MakeCursor();
	// Preserve the no-argument export used by editor tools and existing Live Coding sessions.
	LABY_API TSharedRef<SWidget> MakeLoadingScreen();
	LABY_API TSharedRef<SWidget> MakeLoadingScreen(const TSharedPtr<SWidget>& Previous);
	// Only for a widget returned by MakeLoadingScreen; updates a thread-safe presentation snapshot.
	LABY_API void UpdateLoadingScreen(const TSharedRef<SWidget>& Screen, const FMazePreparationStatus& Status);
}
