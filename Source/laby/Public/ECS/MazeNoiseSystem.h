#pragma once
#include "ECS/MazeNoise.h"
struct FMazePlayerPoseFragment;
struct FMazeGenerationFragment;

struct FMazeNoiseSystem
{
	static void Synchronize(FMazeNoiseFragment& Noise,
	                        FMassEntityHandle Maze,
	                        const FMazeGenerationFragment& Generation);
	static void Update(FMazeNoiseFragment& Noise, const FMazePlayerPoseFragment& Pose, float DeltaSeconds, bool bAlive);
	static void Emit(FMazeNoiseFragment& Noise, EMazeNoiseSource Source);
	static FMazeNoiseSnapshot Snapshot(const FMazeNoiseFragment& Noise, bool bForReplication);
	static bool Receive(FMazeNoiseFragment& Noise, const FMazeNoiseSnapshot& Snapshot);
};
