#pragma once

#include "Maze/MazeInterior.h"

struct FMazeChunkData
{
	FIntPoint Coordinate;
	FMazeSurface Walls;
	FMazeInterior Interior;
	TArray<FTransform> Floors;
	FTransform Ceiling;
	int64 GetGeometryBytes() const;
};
