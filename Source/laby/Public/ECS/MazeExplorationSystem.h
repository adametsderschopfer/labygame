#pragma once
#include "ECS/MazeECSFragments.h"

struct FMazeExplorationSystem
{
	static bool Visible(const FMazeLayout& Layout, FVector2D From, FVector2D To);
	static void Update(FMazeExplorationFragment& Exploration,
	                   FMassEntityHandle Entity,
	                   const FMazeGenerationFragment& Maze,
	                   const FMazePlayerPoseFragment& Pose,
	                   bool bAlive);
	static void SetOpen(FMazeSessionFragment& Session, bool bOpen);
};
