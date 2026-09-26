#pragma once

#include "CoreMinimal.h"

struct FMazeGenerationFragment;
struct FMazeVitals;

struct FMazeWaterExposure
{
	bool bWading = false;
	bool bSubmerged = false;
};

// Evaluates engine-observed feet and eye positions against immutable maze water.
struct FMazeWaterSystem
{
	static FMazeWaterExposure Evaluate(const FMazeGenerationFragment& Maze,
	                                   const FVector& FeetLocation,
	                                   const FVector& EyeLocation);
	static void ApplySubmersion(const FMazeWaterExposure& Exposure, FMazeVitals& Vitals);
};
