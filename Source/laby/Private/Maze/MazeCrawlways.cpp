#include "Maze/MazeLayout.h"
#include "Maze/MazeCrawlwayDefinition.h"

void FMazeLayout::GenerateCrawlways(int32 Seed)
{
	CrawlwaySides.Init(0, Walls.Num());

	TArray<int32> RoomIndexByCell;

	RoomIndexByCell.Init(INDEX_NONE, Walls.Num());

	for (int32 RoomIndex = 0; RoomIndex < Rooms.Num(); ++RoomIndex)
		for (int32 Y = Rooms[RoomIndex].Min.Y; Y < Rooms[RoomIndex].Max.Y; ++Y)
			for (int32 X = Rooms[RoomIndex].Min.X; X < Rooms[RoomIndex].Max.X; ++X)
				RoomIndexByCell[Y * Size + X] = RoomIndex;

	const FIntPoint Steps[] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
	const auto Degree = [this, &Steps](int32 Cell)
	{
		int32 Count = 0;

		for (int32 Direction = 0; Direction < 4; ++Direction)
		{
			const FIntPoint Next(Cell % Size + Steps[Direction].X, Cell / Size + Steps[Direction].Y);

			Count +=
			    Next.X >= 0 && Next.Y >= 0 && Next.X < Size && Next.Y < Size && (Walls[Cell] & (1 << Direction)) == 0;
		}

		return Count;
	};
	const auto Near = [](FIntPoint A, FIntPoint B, int32 Radius)
	{
		return FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y) < Radius;
	};
	struct FCandidate
	{
		int32 Cell;
		int32 Direction;
	};
	TArray<FCandidate> Candidates[3]; // ordinary bend, dead end, dry room

	for (int32 Y = 2; Y < Size - 2; ++Y)
		for (int32 X = 2; X < Size - 2; ++X)
		{
			const int32 Cell = Y * Size + X;
			const FIntPoint Position(X, Y);

			for (const int32 Direction : {1, 2})
			{
				const int32 Next = Cell + (Direction == 1 ? 1 : Size);
				const FIntPoint NextPosition = Position + Steps[Direction];
				const int32 Opposite = (Direction + 2) % 4;
				const FIntPoint Entrance(0, Start() / Size);

				if (Near(Position, Entrance, FMazeCrawlwayDefinition::MinEntranceDistanceCells) ||
				    Near(NextPosition, Entrance, FMazeCrawlwayDefinition::MinEntranceDistanceCells))
					continue;

				if (!(Walls[Cell] & (1 << Direction)) || !(Walls[Next] & (1 << Opposite)))
					continue;

				bool bNearExit = false;

				for (const int32 Exit : Exits)
					bNearExit |= Near(Position,
					                  FIntPoint(Exit % Size, Exit / Size),
					                  FMazeCrawlwayDefinition::MinExitDistanceCells) ||
					             Near(NextPosition,
					                  FIntPoint(Exit % Size, Exit / Size),
					                  FMazeCrawlwayDefinition::MinExitDistanceCells);

				if (bNearExit)
					continue;

				const int32 FirstRoom = RoomIndexByCell[Cell], SecondRoom = RoomIndexByCell[Next];

				if (FirstRoom != INDEX_NONE && SecondRoom != INDEX_NONE)
					continue;

				const int32 RoomIndex = FirstRoom != INDEX_NONE ? FirstRoom : SecondRoom;

				if (RoomIndex != INDEX_NONE && RoomType(RoomIndex) != EMazeRoomType::Empty)
					continue;

				const int32 Kind = RoomIndex != INDEX_NONE ? 2 : Degree(Cell) == 1 || Degree(Next) == 1 ? 1 : 0;

				Candidates[Kind].Add({Cell, Direction});
			}
		}

	FRandomStream Random(static_cast<int32>(uint32(Seed) ^ 0x57A64E29u));

	for (TArray<FCandidate>& Group : Candidates)
		for (int32 Index = Group.Num() - 1; Index > 0; --Index)
			Group.Swap(Index, Random.RandRange(0, Index));

	const auto HasUsefulDetour = [this, &Steps](int32 StartCell, int32 TargetCell)
	{
		TArray<int32> Distance;
		Distance.Init(INDEX_NONE, Walls.Num());
		TArray<int32> Queue;
		Queue.Add(StartCell);
		Distance[StartCell] = 0;

		for (int32 Head = 0; Head < Queue.Num(); ++Head)
		{
			const int32 Cell = Queue[Head];

			if (Distance[Cell] >= FMazeCrawlwayDefinition::MinExistingRouteCells)
				continue;

			for (int32 Direction = 0; Direction < 4; ++Direction)
			{
				if (Walls[Cell] & (1 << Direction))
					continue;

				const FIntPoint Position(Cell % Size + Steps[Direction].X, Cell / Size + Steps[Direction].Y);

				if (Position.X < 0 || Position.Y < 0 || Position.X >= Size || Position.Y >= Size)
					continue;

				const int32 Next = Position.Y * Size + Position.X;

				if (Next == TargetCell)
					return false;

				if (Distance[Next] != INDEX_NONE)
					continue;

				Distance[Next] = Distance[Cell] + 1;
				Queue.Add(Next);
			}
		}

		return true;
	};
	TArray<FIntPoint> Placed;
	const int32 Target = FMath::Max(1, Walls.Num() / FMazeCrawlwayDefinition::FloorCellsPerCrawlway);
	const auto TryPlace = [this, &HasUsefulDetour, &Near, &Placed](const FCandidate& Candidate)
	{
		const FIntPoint Position(Candidate.Cell % Size, Candidate.Cell / Size);

		for (const FIntPoint& Existing : Placed)
			if (Near(Position, Existing, FMazeCrawlwayDefinition::MinSpacingCells))
				return false;

		const int32 Next = Candidate.Cell + (Candidate.Direction == 1 ? 1 : Size);

		if (!HasUsefulDetour(Candidate.Cell, Next))
			return false;

		const int32 Opposite = (Candidate.Direction + 2) % 4;
		Walls[Candidate.Cell] &= ~(1 << Candidate.Direction);
		Walls[Next] &= ~(1 << Opposite);
		CrawlwaySides[Candidate.Cell] |= 1 << Candidate.Direction;
		CrawlwaySides[Next] |= 1 << Opposite;
		Placed.Add(Position);

		return true;
	};

	for (const TArray<FCandidate>& Group : Candidates)
	{
		const int32 GroupTarget = Placed.Num() + Target / 3;

		for (const FCandidate& Candidate : Group)
		{
			if (Placed.Num() >= GroupTarget)
				break;

			TryPlace(Candidate);
		}
	}

	for (const TArray<FCandidate>& Group : Candidates)
		for (const FCandidate& Candidate : Group)
		{
			if (Placed.Num() >= Target)
				return;

			if (!IsCrawlway(Candidate.Cell, Candidate.Direction))
				TryPlace(Candidate);
		}
}
