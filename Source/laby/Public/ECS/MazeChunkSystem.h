#pragma once
#include "ECS/MazeECSFragments.h"
#include "Maze/MazeChunk.h"
#include "Async/Future.h"

// Transport for owned worker data; never contains fragment views or UObjects.
struct FMazeChunkJob
{
	FMassEntityHandle Entity;
	TSharedPtr<const FMazeGeneratedData, ESPMode::ThreadSafe> Source;
	uint32 Revision = 0;
	FIntPoint Coordinate;
	TFuture<TSharedPtr<const FMazeChunkData, ESPMode::ThreadSafe>> Result;
};

struct FMazeChunkSystem
{
	static TSharedPtr<const FMazeChunkData, ESPMode::ThreadSafe> Build(const FMazeGenerationFragment& Maze,
	                                                                   FIntPoint Chunk,
	                                                                   int32 ChunkCells);
};
