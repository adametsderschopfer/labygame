#include "ECS/MazeChunkSystem.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

TSharedPtr<const FMazeChunkData, ESPMode::ThreadSafe> FMazeChunkSystem::Build(const FMazeGenerationFragment& Maze,
                                                                              FIntPoint Chunk,
                                                                              int32 ChunkCells)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Maze_BuildChunk);

	if (!Maze.Data || ChunkCells < 2 || ChunkCells > 32 || Chunk.X < 0 || Chunk.Y < 0)
		return nullptr;

	const int32 Count = FMath::DivideAndRoundUp(Maze.Data->Layout.Size, ChunkCells);

	if (Chunk.X >= Count || Chunk.Y >= Count)
		return nullptr;

	const auto& Layout = Maze.Data->Layout;
	const FIntRect Cells(Chunk * ChunkCells,
	                     FIntPoint(FMath::Min((Chunk.X + 1) * ChunkCells, Layout.Size),
	                               FMath::Min((Chunk.Y + 1) * ChunkCells, Layout.Size)));
	auto Result = MakeShared<FMazeChunkData, ESPMode::ThreadSafe>();

	Result->Coordinate = Chunk;
	Result->Walls.Build(Layout, Maze.Cell, Maze.WallThickness, Maze.WallHeight, Cells);

	// One-cell halo supplies neighboring face normals for seamless cove/corner joins.
	const FIntRect Halo(FIntPoint(FMath::Max(0, Cells.Min.X - 1), FMath::Max(0, Cells.Min.Y - 1)),
	                    FIntPoint(FMath::Min(Layout.Size, Cells.Max.X + 1), FMath::Min(Layout.Size, Cells.Max.Y + 1)));
	FMazeSurface Context;

	Context.Build(Layout, Maze.Cell, Maze.WallThickness, Maze.WallHeight, Halo);
	Result->Interior =
	    FMazeInterior::Build(Layout, Context, Maze.Cell, Maze.WallThickness, Maze.WallHeight, Maze.Seed, Cells);

	// Clip the canonical floor rectangles, preserving holes and the exterior apron.
	const float Span = Layout.Size * Maze.Cell;
	const FVector2D Min(Cells.Min.X == 0 ? -1200.f : Cells.Min.X * Maze.Cell,
	                    Cells.Min.Y == 0 ? -1200.f : Cells.Min.Y * Maze.Cell);
	const FVector2D Max(Cells.Max.X == Layout.Size ? Span + 1200.f : Cells.Max.X * Maze.Cell,
	                    Cells.Max.Y == Layout.Size ? Span + 1200.f : Cells.Max.Y * Maze.Cell);

	for (const auto& Floor : Maze.Data->FloorTransforms)
	{
		const FVector P = Floor.GetLocation(), Half = Floor.GetScale3D() * 50;
		const float X0 = FMath::Max(Min.X, P.X - Half.X), X1 = FMath::Min(Max.X, P.X + Half.X);
		const float Y0 = FMath::Max(Min.Y, P.Y - Half.Y), Y1 = FMath::Min(Max.Y, P.Y + Half.Y);

		if (X1 > X0 && Y1 > Y0)
			Result->Floors.Emplace(FRotator::ZeroRotator,
			                       FVector((X0 + X1) / 2, (Y0 + Y1) / 2, P.Z),
			                       FVector((X1 - X0) / 100, (Y1 - Y0) / 100, Floor.GetScale3D().Z));
	}

	const float X0 = Cells.Min.X * Maze.Cell - (Cells.Min.X == 0 ? Maze.WallThickness / 2 : 0);
	const float Y0 = Cells.Min.Y * Maze.Cell - (Cells.Min.Y == 0 ? Maze.WallThickness / 2 : 0);
	const float X1 = Cells.Max.X * Maze.Cell + (Cells.Max.X == Layout.Size ? Maze.WallThickness / 2 : 0);
	const float Y1 = Cells.Max.Y * Maze.Cell + (Cells.Max.Y == Layout.Size ? Maze.WallThickness / 2 : 0);

	Result->Ceiling = FTransform(FRotator::ZeroRotator,
	                             FVector((X0 + X1) / 2, (Y0 + Y1) / 2, Maze.WallHeight + Maze.WallThickness / 2),
	                             FVector((X1 - X0) / 100, (Y1 - Y0) / 100, Maze.WallThickness / 100));

	return Result;
}
