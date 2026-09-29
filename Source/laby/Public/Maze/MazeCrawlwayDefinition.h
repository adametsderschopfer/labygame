#pragma once

#include "CoreMinimal.h"

// Physical clearance and placement limits for optional crouch-only shortcuts.
struct FMazeCrawlwayDefinition
{
	static constexpr float ClearWidthCm = 150.f;
	static constexpr float ClearHeightCm = 135.f;
	static constexpr int32 FloorCellsPerCrawlway = 150;
	static constexpr int32 MinExistingRouteCells = 6;
	static constexpr int32 MinSpacingCells = 5;
	static constexpr int32 MinEntranceDistanceCells = 5;
	static constexpr int32 MinExitDistanceCells = 4;
};
