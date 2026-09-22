#include "Maze/MazeLayout.h"
#include "Maze/MazeRoomDefinition.h"
#include "Maze/MazeRoutes.h"

bool FMazeLayout::IsEntranceCell(int32 X, int32 Y) const
{
	const bool bRoom = X >= 0 && X < EntranceRoomSize && Y >= Size - EntranceRoomSize && Y < Size;
	const bool bPassage = X >= EntranceRoomSize && X < EntranceRoomSize + EntrancePassageLength && Y == Start() / Size;

	return bRoom || bPassage;
}

bool FMazeLayout::OverlapsEntrance(const FIntRect& Room) const
{
	// Keep a one-cell margin, including the passage's connection to the maze.
	for (int32 Y = Room.Min.Y - 1; Y <= Room.Max.Y; ++Y)
		for (int32 X = Room.Min.X - 1; X <= Room.Max.X; ++X)
			if (IsEntranceCell(X, Y))
				return true;

	return false;
}

void FMazeLayout::CarveEntrance()
{
	// These cells were excluded from the random maze so the room has exactly one door.
	CarveRoom(FIntRect(0, Size - EntranceRoomSize, EntranceRoomSize, Size));

	const int32 Y = Start() / Size;

	for (int32 X = EntranceRoomSize - 1; X < EntranceRoomSize + EntrancePassageLength; ++X)
	{
		const int32 C = Y * Size + X;

		Walls[C] &= ~2;
		Walls[C + 1] &= ~8;
	}
}

void FMazeLayout::Generate(int32 Seed, int32 InSize)
{
	Size = FMath::Clamp(InSize, 8, 100);

	FRandomStream Random(Seed);

	Walls.Init(15, Size * Size); // north, east, south, west
	Exits.Reset();
	Holes.Init(0, Walls.Num());
	ReserveRooms(Random);

	TArray<bool> Reserved;

	Reserved.Init(false, Walls.Num());

	for (const FIntRect& Room : Rooms)
		for (int32 Y = Room.Min.Y; Y < Room.Max.Y; ++Y)
			for (int32 X = Room.Min.X; X < Room.Max.X; ++X)
				Reserved[Y * Size + X] = true;

	for (int32 C = 0; C < Walls.Num(); ++C)
		Reserved[C] = Reserved[C] || IsEntranceCell(C % Size, C / Size);

	const int32 PassageEnd = (Start() / Size) * Size + EntranceRoomSize + EntrancePassageLength;
	const TArray<int32> ScenicFloor = MazeRoutes::Build(*this, Reserved, PassageEnd, Random);
	const int32 DX[] = {0, 1, 0, -1}, DY[] = {-1, 0, 1, 0};

	// One opening on the north boundary, away from corners.
	Exits.Add(Random.RandRange(2, Size - 3));
	Walls[Exits[0]] &= ~1;
	CarveEntrance();

	TArray<int32> RoomOrder;

	for (int32 I = 1; I < Rooms.Num(); ++I)
		RoomOrder.Add(I);

	for (int32 I = RoomOrder.Num() - 1; I > 0; --I)
		RoomOrder.Swap(I, Random.RandRange(0, I));

	const int32 ThroughCount = FMath::RoundToInt(RoomOrder.Num() * FMazeRoomDefinition::ThroughFraction);

	for (int32 I = 0; I < RoomOrder.Num(); ++I)
	{
		const FIntRect& Room = Rooms[RoomOrder[I]];

		CarveRoom(Room);

		const int32 FirstDirection = Random.RandRange(0, 3);
		// Adjacent walls make the room a bend, avoiding aligned door-to-door sightlines.
		const int32 SecondDirection = (FirstDirection + (Random.RandRange(0, 1) ? 1 : 3)) % 4;

		for (int32 DoorIndex = 0; DoorIndex < (I < ThroughCount ? 2 : 1); ++DoorIndex)
		{
			const int32 Direction = DoorIndex == 0 ? FirstDirection : SecondDirection;
			FIntPoint Door = (Room.Min + Room.Max - FIntPoint(1, 1)) / 2;

			if (Direction == 0)
				Door.Y = Room.Min.Y;

			if (Direction == 1)
				Door.X = Room.Max.X - 1;

			if (Direction == 2)
				Door.Y = Room.Max.Y - 1;

			if (Direction == 3)
				Door.X = Room.Min.X;

			const int32 C = Door.Y * Size + Door.X;
			const int32 Next = C + DY[Direction] * Size + DX[Direction];

			Walls[C] &= ~(1 << Direction);
			Walls[Next] &= ~(1 << ((Direction + 2) % 4));
		}
	}

	AddRoomsAtBends(Random, ScenicFloor);
	GenerateHoles(Random, ScenicFloor);
}

void FMazeLayout::ReserveRooms(FRandomStream& Random)
{
	Rooms.Reset();
	Rooms.Add(FIntRect(0, Size - EntranceRoomSize, EntranceRoomSize, Size));

	const int32 SectorCount = FMath::DivideAndRoundUp(Size, FMazeRoomDefinition::SectorSide);
	int32 SizeWeight = 0;

	for (const auto& Range : FMazeRoomDefinition::SizeRanges)
		SizeWeight += Range.Weight;

	const auto TryReserve = [&](const FIntRect& Room)
	{
		// Sector margins separate random rooms. Only the fixed entrance can overlap.
		if (OverlapsEntrance(Room))
			return false;

		Rooms.Add(Room);

		return true;
	};

	for (int32 SectorY = 0; SectorY < SectorCount; ++SectorY)
		for (int32 SectorX = 0; SectorX < SectorCount; ++SectorX)
		{
			// A corridor margin on every sector edge preserves the connected outside grid.
			const FIntRect Bounds(SectorX * Size / SectorCount + 1,
			                      SectorY * Size / SectorCount + 1,
			                      (SectorX + 1) * Size / SectorCount - 1,
			                      (SectorY + 1) * Size / SectorCount - 1);
			const int32 MaxWidth = FMath::Min(FMazeRoomDefinition::MaxWidth, Bounds.Width());
			const int32 MaxLength = FMath::Min(FMazeRoomDefinition::MaxLength, Bounds.Height());

			if (MaxWidth < FMazeRoomDefinition::MinWidth || MaxLength < FMazeRoomDefinition::MinLength)
				continue;

			bool bPlaced = false;
			int32 SizePick = Random.RandRange(1, SizeWeight);
			const FMazeRoomDefinition::FSizeRange* SizeRange = &FMazeRoomDefinition::SizeRanges[0];

			for (const auto& Range : FMazeRoomDefinition::SizeRanges)
			{
				SizePick -= Range.Weight;

				if (SizePick <= 0)
				{
					SizeRange = &Range;
					break;
				}
			}

			for (int32 Attempt = 0; Attempt < FMazeRoomDefinition::PlacementAttemptsPerRoom && !bPlaced; ++Attempt)
			{
				const int32 Width = Random.RandRange(FMath::Min(SizeRange->MinWidth, MaxWidth),
				                                     FMath::Min(SizeRange->MaxWidth, MaxWidth));
				const int32 Length = Random.RandRange(FMath::Min(SizeRange->MinLength, MaxLength),
				                                      FMath::Min(SizeRange->MaxLength, MaxLength));
				const int32 X = Random.RandRange(Bounds.Min.X, Bounds.Max.X - Width);
				const int32 Y = Random.RandRange(Bounds.Min.Y, Bounds.Max.Y - Length);
				bPlaced = TryReserve(FIntRect(X, Y, X + Width, Y + Length));
			}

			// Exhaust the smallest footprint before skipping an entrance-constrained sector.
			// Thus unlucky random attempts cannot leave an otherwise usable region empty.
			for (int32 Y = Bounds.Min.Y; Y + FMazeRoomDefinition::MinLength <= Bounds.Max.Y && !bPlaced; ++Y)
				for (int32 X = Bounds.Min.X; X + FMazeRoomDefinition::MinWidth <= Bounds.Max.X && !bPlaced; ++X)
					bPlaced = TryReserve(
					    FIntRect(X, Y, X + FMazeRoomDefinition::MinWidth, Y + FMazeRoomDefinition::MinLength));
		}
}

void FMazeLayout::CarveRoom(const FIntRect& Room)
{
	for (int32 Y = Room.Min.Y; Y < Room.Max.Y; ++Y)
		for (int32 X = Room.Min.X; X < Room.Max.X; ++X)
		{
			const int32 C = Y * Size + X;

			if (X + 1 < Room.Max.X)
			{
				Walls[C] &= ~2;
				Walls[C + 1] &= ~8;
			}

			if (Y + 1 < Room.Max.Y)
			{
				Walls[C] &= ~4;
				Walls[C + Size] &= ~1;
			}
		}
}

void FMazeLayout::GenerateHoles(FRandomStream& Random, const TArray<int32>& ScenicFloor)
{
	TArray<bool> Protected;

	Protected.Init(false, Walls.Num());

	for (const int32 Cell : ScenicFloor)
		Protected[Cell] = true;

	for (const FIntRect& Room : Rooms)
		for (int32 Y = Room.Min.Y; Y < Room.Max.Y; ++Y)
			for (int32 X = Room.Min.X; X < Room.Max.X; ++X)
				Protected[Y * Size + X] = true;

	const int32 DX[] = {0, 1, 0, -1}, DY[] = {-1, 0, 1, 0};

	for (const FMazeRoomDoorway& Door : RoomDoorways())
	{
		const FIntPoint Outside = Door.Cell + FIntPoint(DX[Door.Direction], DY[Door.Direction]);

		if (Outside.X >= 0 && Outside.Y >= 0 && Outside.X < Size && Outside.Y < Size)
			Protected[Outside.Y * Size + Outside.X] = true;
	}

	TArray<int32> Candidates;

	for (int32 Y = 1; Y < Size - 1; ++Y)
		for (int32 X = 1; X < Size - 1; ++X)
			if (!Protected[Y * Size + X] && !OverlapsEntrance(FIntRect(X, Y, X + 1, Y + 1)))
				Candidates.Add(Y * Size + X);

	for (int32 I = Candidates.Num() - 1; I > 0; --I)
		Candidates.Swap(I, Random.RandRange(0, I));

	const int32 TargetHoles = FMath::Max(1, Size * Size / 100);
	int32 HoleCount = 0;
	const int32 Attempts = FMath::Min(Candidates.Num(), TargetHoles * 12);

	for (int32 I = 0; I < Attempts && HoleCount < TargetHoles; ++I)
	{
		const int32 C = Candidates[I];
		bool bAdjacentHole = false;

		for (int32 OffsetY = -1; OffsetY <= 1; ++OffsetY)
			for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
				bAdjacentHole |= Holes[C + OffsetY * Size + OffsetX] != 0;

		if (bAdjacentHole)
			continue;

		Holes[C] = 1;

		// Reject removal of an articulation cell: every remaining floor tile,
		// including the protected boundary exit, must remain reachable on foot.
		if (ReachableFloorCount() == Walls.Num() - HoleCount - 1)
			++HoleCount;
		else
			Holes[C] = 0;
	}
}

TArray<FMazeRoomDoorway> FMazeLayout::RoomDoorways() const
{
	TArray<FMazeRoomDoorway> Result;
	const auto Add = [&](int32 X, int32 Y, int32 Direction)
	{
		if (!(Walls[Y * Size + X] & (1 << Direction)))
			Result.Add({FIntPoint(X, Y), Direction});
	};

	for (const FIntRect& Room : Rooms)
	{
		for (int32 X = Room.Min.X; X < Room.Max.X; ++X)
		{
			Add(X, Room.Min.Y, 0);
			Add(X, Room.Max.Y - 1, 2);
		}

		for (int32 Y = Room.Min.Y; Y < Room.Max.Y; ++Y)
		{
			Add(Room.Min.X, Y, 3);
			Add(Room.Max.X - 1, Y, 1);
		}
	}

	return Result;
}
