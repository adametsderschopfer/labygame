#pragma once

#include "CoreMinimal.h"
#include "Layout/Geometry.h"

class FSlateWindowElementList;

struct FMazeLayout;

struct FMazeMapSignal
{
	FVector2D Position = FVector2D::ZeroVector;
	float Progress = 0.f;
	bool bLocal = false;
};

struct FMazeMapFootprint
{
	FVector2D Position = FVector2D::ZeroVector;
	float Opacity = 0.f;
	float Strength = 0.f;
};

// Borrowed presentation inputs, consumed synchronously by one paint call. No world state is retained.
struct FMazeMapPaintView
{
	FVector2D Origin;
	FVector2D Size;
	FVector2D Center;
	FVector2D Player;
	FVector2D Start;
	float Step = 20.f;
	float Cell = 462.5f;
	float Yaw = 0.f;
	bool bFull = false;
	bool bCompass = true;
	bool bPreview = false;
	const FMazeLayout* Layout = nullptr;
	TConstArrayView<uint8> Seen;
	TConstArrayView<FMazeMapSignal> Signals;
	TConstArrayView<FMazeMapFootprint> Footprints;
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	TConstArrayView<int32> DevelopmentRoute;
#endif
};

FSlateRect MazeMapContentRect(const FMazeMapPaintView& View);
int32 PaintMazeMap(const FGeometry& Geometry,
                   FSlateWindowElementList& Elements,
                   int32 Layer,
                   const FLinearColor& Tint,
                   const FMazeMapPaintView& View);
