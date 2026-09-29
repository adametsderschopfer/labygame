#pragma once

#include "CoreMinimal.h"

// Seeded placement limits for occasional doors between corridor cells.
struct FMazeCorridorDoorDefinition
{
	static constexpr int32 FloorCellsPerDoor = 450;
	static constexpr int32 MinDoorSpacingCells = 6;
	static constexpr int32 MinRoomDoorDistanceCells = 3;
	static constexpr int32 MinEntranceDistanceCells = 6;
	static constexpr int32 MinExitDistanceCells = 4;
	static constexpr int32 MinHoleDistanceCells = 1;
};
