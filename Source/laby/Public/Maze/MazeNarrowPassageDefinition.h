#pragma once

#include "CoreMinimal.h"

// Deterministic topology selection and physical traversal dimensions.
struct FMazeNarrowPassageDefinition
{
	// Soft target over eligible straight corridor cells. Placement constraints take priority.
	static constexpr float CoverageFraction = 0.05f;
	static constexpr int32 MinCells = 2;
	static constexpr int32 MaxCells = 3;
	static constexpr int32 MinSpacingCells = 3;
	static constexpr float ClearWidthCm = 145.f;
	static constexpr float TaperLengthCm = 120.f;
	// Keep horizontal prism caps behind the floor and ceiling to avoid coplanar rendering artifacts.
	static constexpr float FloorCeilingOverlapCm = 1.f;
};
