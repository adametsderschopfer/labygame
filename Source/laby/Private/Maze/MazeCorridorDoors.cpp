#include "Maze/MazeLayout.h"
#include "Maze/MazeCorridorDoorDefinition.h"

void FMazeLayout::GenerateCorridorDoorways(int32 Seed)
{
	CorridorDoorways.Reset();

	TArray<uint8> RoomCells, Excluded;

	RoomCells.Init(0, Walls.Num());
	Excluded.Init(0, Walls.Num());

	for (const FIntRect& Room : Rooms)
		for (int32 Y = Room.Min.Y; Y < Room.Max.Y; ++Y)
			for (int32 X = Room.Min.X; X < Room.Max.X; ++X)
				RoomCells[Y * Size + X] = 1;

	const auto ExcludeNear = [&](FIntPoint Center, int32 Radius)
	{
		for (int32 Y = FMath::Max(0, Center.Y - Radius); Y <= FMath::Min(Size - 1, Center.Y + Radius); ++Y)
			for (int32 X = FMath::Max(0, Center.X - Radius); X <= FMath::Min(Size - 1, Center.X + Radius); ++X)
				if (FMath::Abs(X - Center.X) + FMath::Abs(Y - Center.Y) <= Radius)
					Excluded[Y * Size + X] = 1;
	};

	const FIntPoint Steps[] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};

	for (const FMazeDoorway& Door : RoomDoorways())
		ExcludeNear(Door.Cell + Steps[Door.Direction], FMazeCorridorDoorDefinition::MinRoomDoorDistanceCells);

	ExcludeNear(FIntPoint(0, Start() / Size), FMazeCorridorDoorDefinition::MinEntranceDistanceCells);

	for (const int32 Exit : Exits)
		ExcludeNear(FIntPoint(Exit % Size, Exit / Size), FMazeCorridorDoorDefinition::MinExitDistanceCells);

	for (int32 Cell = 0; Cell < Walls.Num(); ++Cell)
		if (Holes[Cell] || NarrowPassages[Cell])
			ExcludeNear(FIntPoint(Cell % Size, Cell / Size), FMazeCorridorDoorDefinition::MinHoleDistanceCells);

	for (int32 Cell = 0; Cell < CrawlwaySides.Num(); ++Cell)
		if (CrawlwaySides[Cell] != 0)
			ExcludeNear(FIntPoint(Cell % Size, Cell / Size), FMazeCorridorDoorDefinition::MinRoomDoorDistanceCells);

	TArray<FMazeDoorway> Candidates;

	for (int32 Y = 2; Y < Size - 2; ++Y)
		for (int32 X = 2; X < Size - 2; ++X)
		{
			const int32 Cell = Y * Size + X;

			if (Excluded[Cell] || RoomCells[Cell] || !HasFloor(Cell))
				continue;

			// Both sides must be the same one-cell-wide straight corridor. A new
			// transverse wall then joins the existing side walls without a loose end.
			for (const int32 Direction : {1, 2})
			{
				const int32 Next = Cell + (Direction == 1 ? 1 : Size);
				const uint8 StraightWalls = Direction == 1 ? 5 : 10;

				if (Walls[Cell] == StraightWalls && Walls[Next] == StraightWalls && !Excluded[Next] &&
				    !RoomCells[Next] && HasFloor(Next))
					Candidates.Add({FIntPoint(X, Y), Direction, INDEX_NONE});
			}
		}

	FRandomStream Random(static_cast<int32>(uint32(Seed) ^ 0xC36D4A91u));

	for (int32 I = Candidates.Num() - 1; I > 0; --I)
		Candidates.Swap(I, Random.RandRange(0, I));

	const int32 Target = FMath::Max(1, (Walls.Num() - NumHoles()) / FMazeCorridorDoorDefinition::FloorCellsPerDoor);

	for (const FMazeDoorway& Candidate : Candidates)
	{
		bool bTooClose = false;

		for (const FMazeDoorway& Existing : CorridorDoorways)
			if (FMath::Abs(Candidate.Cell.X - Existing.Cell.X) + FMath::Abs(Candidate.Cell.Y - Existing.Cell.Y) <
			    FMazeCorridorDoorDefinition::MinDoorSpacingCells)
			{
				bTooClose = true;
				break;
			}

		if (bTooClose)
			continue;

		CorridorDoorways.Add(Candidate);

		if (CorridorDoorways.Num() >= Target)
			break;
	}

	CorridorDoorways.Sort(
	    [](const FMazeDoorway& A, const FMazeDoorway& B)
	    {
		    if (A.Cell.Y != B.Cell.Y)
			    return A.Cell.Y < B.Cell.Y;

		    return A.Cell.X != B.Cell.X ? A.Cell.X < B.Cell.X : A.Direction < B.Direction;
	    });
}
