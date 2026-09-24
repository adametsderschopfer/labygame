#pragma once

#include "ECS/MazeECSFragments.h"

struct FMazeTraversalSystem
{
	static bool IsInNarrowPassage(const FMazeGenerationFragment& Maze, const FVector& WorldLocation)
	{
		if (!Maze.Data || Maze.Cell <= 0.f)
			return false;

		const FVector Local = WorldLocation - Maze.Origin;
		const int32 X = FMath::FloorToInt(Local.X / Maze.Cell);
		const int32 Y = FMath::FloorToInt(Local.Y / Maze.Cell);
		const auto& Layout = Maze.Data->Layout;

		return X >= 0 && Y >= 0 && X < Layout.Size && Y < Layout.Size &&
		       Layout.NarrowPassages.IsValidIndex(Y * Layout.Size + X) &&
		       Layout.NarrowPassages[Y * Layout.Size + X] != 0;
	}
};
