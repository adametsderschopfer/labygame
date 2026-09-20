#include "ECS/MazeECSSubsystem.h"
#include "ECS/MazeChunkSystem.h"
#include "Async/Async.h"

TSharedPtr<const FMazeSurface> UMazeECSSubsystem::BuildMazeCollision(FMassEntityHandle Entity) const
{
	const auto Maze = ReadMaze(Entity);

	if (!Maze.Data)
		return nullptr;

	auto Surface = MakeShared<FMazeSurface>();

	Surface->Build(Maze.Data->Layout, Maze.Cell, Maze.WallThickness, Maze.WallHeight);

	return Surface;
}

TSharedPtr<FMazeChunkJob> UMazeECSSubsystem::RequestMazeChunk(FMassEntityHandle Entity,
                                                              FIntPoint Chunk,
                                                              int32 ChunkCells) const
{
	check(IsInGameThread());

	const auto Maze = ReadMaze(Entity);

	if (!Maze.Data)
		return nullptr;

	auto Job = MakeShared<FMazeChunkJob>();

	Job->Entity = Entity;
	Job->Source = Maze.Data;
	Job->Revision = Maze.Revision;
	Job->Coordinate = Chunk;
	Job->Result = Async(EAsyncExecution::ThreadPool,
	                    [Maze, Chunk, ChunkCells]()
	                    {
		                    return FMazeChunkSystem::Build(Maze, Chunk, ChunkCells);
	                    });

	return Job;
}

TSharedPtr<const FMazeChunkData, ESPMode::ThreadSafe> UMazeECSSubsystem::TakeMazeChunk(
    const TSharedPtr<FMazeChunkJob>& Job) const
{
	check(IsInGameThread());

	if (!Job || !Job->Result.IsValid() || !Job->Result.IsReady())
		return nullptr;

	const auto Maze = ReadMaze(Job->Entity);
	const auto Result = Job->Result.Get();

	return Maze.Data && Maze.Data == Job->Source && Maze.Revision == Job->Revision ? Result : nullptr;
}
