#include "Maze/MazeDevelopmentRoute.h"
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
#include "Maze/MazeLayout.h"
#include "Algo/Reverse.h"

TArray<int32> BuildMazeDevelopmentRoute(const FMazeLayout& Layout, int32 Start)
{
	TArray<int32> Route;

	if (Layout.Size <= 0 || !Layout.Walls.IsValidIndex(Start) || !Layout.HasFloor(Start))
		return Route;

	TArray<int32> Parent, Queue;

	Parent.Init(INDEX_NONE, Layout.Walls.Num());
	Queue.Add(Start);
	Parent[Start] = Start;

	const FIntPoint Directions[] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
	int32 End = INDEX_NONE;

	for (int32 Head = 0; Head < Queue.Num(); ++Head)
	{
		const int32 Cell = Queue[Head];

		if (Layout.Exits.Contains(Cell))
		{
			End = Cell;
			break;
		}

		for (int32 D = 0; D < 4; ++D)
		{
			if (Layout.Walls[Cell] & (1 << D))
				continue;

			const FIntPoint Next = FIntPoint(Cell % Layout.Size, Cell / Layout.Size) + Directions[D];

			if (Next.X < 0 || Next.Y < 0 || Next.X >= Layout.Size || Next.Y >= Layout.Size)
				continue;

			const int32 Index = Next.Y * Layout.Size + Next.X;

			if (!Parent.IsValidIndex(Index) || Parent[Index] != INDEX_NONE || !Layout.HasFloor(Index))
				continue;

			Parent[Index] = Cell;
			Queue.Add(Index);
		}
	}

	for (int32 Cell = End; Cell != INDEX_NONE; Cell = Cell == Start ? INDEX_NONE : Parent[Cell])
		Route.Add(Cell);

	Algo::Reverse(Route);

	return Route;
}

#endif
