#pragma once
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
#include "Maze/MazeRoomIdentity.h"
struct FMazeGenerationFragment;

struct FMazeDevelopmentLocation
{
	FVector Position = FVector::ZeroVector;
	FIntPoint Cell = FIntPoint::ZeroValue;
	int32 RoomIndex = INDEX_NONE;
	bool bInside = false;
};

struct FMazeDevelopmentTeleport
{
	// Floor reference, not capsule origin. The engine adapter resolves floor/capsule fit.
	FVector FloorLocation = FVector::ZeroVector;
	uint32 Revision = 0;
};

// Development-only pure diagnostics/target selection; no engine adapters or side effects.
struct FMazeDevelopmentSystem
{
	static FMazeDevelopmentLocation Locate(const FMazeGenerationFragment& Maze, const FVector& Position);
	static bool Teleport(const FMazeGenerationFragment& Maze,
	                     const FVector& Position,
	                     bool bExit,
	                     FMazeDevelopmentTeleport& Out);
};
#endif
