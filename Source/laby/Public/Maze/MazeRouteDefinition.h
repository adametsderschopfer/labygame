#pragma once

#include "CoreMinimal.h"

struct FMazeRouteDefinition
{
	static constexpr int32 OrdinaryStraightCells = 6;
	static constexpr int32 ScenicMinCells = 8;
	static constexpr int32 ScenicMaxCells = 10;
	static constexpr int32 CellsPerScenicCorridor = 2400;
	static constexpr int32 ScenicPlacementAttempts = 80;
	static constexpr int32 ShapingPasses = 4;
	static constexpr float BraidFraction = 0.4f;
	static constexpr float TurnPreference = 0.65f;
};
