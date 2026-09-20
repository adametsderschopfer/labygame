#pragma once

#include "CoreMinimal.h"
#include "Layout/Geometry.h"

class FSlateWindowElementList;

struct FMazeLayout;

// Borrowed presentation inputs, consumed synchronously by one paint call. No world state is retained.
struct FMazeMapPaintView
{
	FVector2D Origin;
	FVector2D Size;
	FVector2D Center;
	FVector2D Player;
	FVector2D Start;
	float Step = 20.f;
	float Yaw = 0.f;
	bool bFull = false;
	bool bCompass = true;
	bool bPreview = false;
	const FMazeLayout* Layout = nullptr;
	TConstArrayView<uint8> Seen;
};

FSlateRect MazeMapContentRect(const FMazeMapPaintView& View);
int32 PaintMazeMap(const FGeometry& Geometry,
                   FSlateWindowElementList& Elements,
                   int32 Layer,
                   const FLinearColor& Tint,
                   const FMazeMapPaintView& View);
