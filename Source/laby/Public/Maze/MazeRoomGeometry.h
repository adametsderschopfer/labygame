#pragma once

#include "CoreMinimal.h"
#include "Maze/MazeLayout.h"

enum class EMazeRoomSurface : uint8
{
	ShallowFloor,
	Ceramic,
	PoolTile,
	Water,
	Metal,
	Count
};

struct FMazeRoomBox
{
	FTransform Transform;
	EMazeRoomSurface Surface = EMazeRoomSurface::Ceramic;
	bool bCollision = true;
};

// Immutable presentation/collision boxes derived from canonical room topology.
struct FMazeRoomGeometry
{
	TArray<FTransform> Ceilings;
	TArray<FMazeRoomBox> Boxes;

	static FMazeRoomGeometry Build(const FMazeLayout& Layout, float Cell, float WallThickness, float DefaultWallHeight);
};
