#include "ECS/MazeDevelopmentSystem.h"
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
#include "ECS/MazeECSFragments.h"
#include "Maze/MazeDevelopmentRoute.h"

FMazeDevelopmentLocation FMazeDevelopmentSystem::Locate(const FMazeGenerationFragment& Maze, const FVector& Position)
{
	FMazeDevelopmentLocation Result;

	Result.Position = Position;

	if (!Maze.Data || Maze.Cell <= 0 || Position.ContainsNaN())
		return Result;

	const FVector Local = (Position - Maze.Origin) / Maze.Cell;

	Result.Cell = FIntPoint(FMath::FloorToInt(Local.X), FMath::FloorToInt(Local.Y));

	const auto& Layout = Maze.Data->Layout;

	Result.bInside =
	    Result.Cell.X >= 0 && Result.Cell.Y >= 0 && Result.Cell.X < Layout.Size && Result.Cell.Y < Layout.Size;

	if (Result.bInside)
		for (int32 Index = 0; Index < Layout.Rooms.Num(); ++Index)
			if (Layout.Rooms[Index].Contains(Result.Cell))
			{
				Result.RoomIndex = Index;
				break;
			}

	return Result;
}

bool FMazeDevelopmentSystem::Teleport(const FMazeGenerationFragment& Maze,
                                      const FVector& Position,
                                      bool bExit,
                                      FMazeDevelopmentTeleport& Out)
{
	if (!Maze.Data || Maze.bNeedsGeneration || Maze.Cell <= 0 || Position.ContainsNaN())
		return false;

	FVector Target = Maze.Data->Start;

	if (bExit)
	{
		const auto& Layout = Maze.Data->Layout;
		const auto Place = Locate(Maze, Position);
		const auto Route =
		    BuildMazeDevelopmentRoute(Layout, Place.bInside ? Place.Cell.Y * Layout.Size + Place.Cell.X : INDEX_NONE);
		int32 ExitIndex = Route.IsEmpty() ? INDEX_NONE : Layout.Exits.Find(Route.Last());

		if (ExitIndex == INDEX_NONE)
		{
			double Distance = TNumericLimits<double>::Max();

			for (int32 Index = 0; Index < Maze.Data->ExitPositions.Num(); ++Index)
			{
				const double Next = FVector::DistSquared2D(Position, Maze.Origin + Maze.Data->ExitPositions[Index]);

				if (Next < Distance)
				{
					Distance = Next;
					ExitIndex = Index;
				}
			}
		}

		if (!Maze.Data->ExitPositions.IsValidIndex(ExitIndex))
			return false;

		const FVector Directions[] = {FVector(0, -1, 0), FVector(1, 0, 0), FVector(0, 1, 0)};

		if (ExitIndex >= UE_ARRAY_COUNT(Directions))
			return false;

		// Cross the canonical exit threshold, staying within the resident floor apron.
		Target = Maze.Data->ExitPositions[ExitIndex] + Directions[ExitIndex] * 25.f;
	}

	Target.Z = 0;
	Out.FloorLocation = Maze.Origin + Target;
	Out.Revision = Maze.Revision;

	return true;
}

#endif
