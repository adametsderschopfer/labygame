#pragma once

#include "Maze/MazeInterior.h"
#include "Maze/MazeRoomGeometry.h"

struct FMazeChunkData
{
	FIntPoint Coordinate;
	FMazeSurface Walls;
	FMazeSurface Water;
	FMazeInterior Interior;
	TArray<FTransform> Floors;
	TArray<FTransform> Ceilings;
	TArray<FTransform> RoomSurfaces[static_cast<int32>(EMazeRoomSurface::Count)];
	int64 GetGeometryBytes() const;
};
