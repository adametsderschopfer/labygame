#include "Maze/MazeLayout.h"
#include "Maze/MazeNarrowPassageDefinition.h"
#include "Maze/MazeRoomDefinition.h"
#include "Maze/MazeRoutes.h"

bool FMazeLayout::IsEntranceCell(int32 X, int32 Y) const
{
	return X >= 0 && X < EntranceLengthCells && Y == Start() / Size;
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
	const int32 Y = Start() / Size;

	// Open the short, single-width dead end directly into the generated corridors.
	for (int32 X = 0; X < EntranceLengthCells; ++X)
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
	CrawlwaySides.Init(0, Walls.Num());
	ReserveRooms(Random);

	TArray<bool> Reserved;

	Reserved.Init(false, Walls.Num());

	for (const FIntRect& Room : Rooms)
		for (int32 Y = Room.Min.Y; Y < Room.Max.Y; ++Y)
			for (int32 X = Room.Min.X; X < Room.Max.X; ++X)
				Reserved[Y * Size + X] = true;

	for (int32 C = 0; C < Walls.Num(); ++C)
		Reserved[C] = Reserved[C] || IsEntranceCell(C % Size, C / Size);

	const int32 PassageEnd = Start() + EntranceLengthCells;
	const TArray<int32> ScenicFloor = MazeRoutes::Build(*this, Reserved, PassageEnd, Random);
	const int32 DX[] = {0, 1, 0, -1}, DY[] = {-1, 0, 1, 0};

	// One opening on the north boundary, away from corners.
	Exits.Add(Random.RandRange(2, Size - 3));
	Walls[Exits[0]] &= ~1;
	CarveEntrance();

	TArray<int32> RoomOrder;

	for (int32 I = 0; I < Rooms.Num(); ++I)
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

		const bool bThroughRoom = Room.Width() * Room.Height() > 1 && I < ThroughCount;

		for (int32 DoorIndex = 0; DoorIndex < (bThroughRoom ? 2 : 1); ++DoorIndex)
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

	AddRoomsAtDeadEnds(Random, ScenicFloor);
	GenerateRoomTypes(Seed);
	GenerateRoomPurposes(Seed);
	GenerateCrawlways(Seed);
	// Temporarily disabled; retain the generator for restoration.
	// GenerateNarrowPassages(Seed, ScenicFloor);
	NarrowPassages.Init(0, Walls.Num());
	GenerateHoles(Random, ScenicFloor);
	GenerateCorridorDoorways(Seed);
}

void FMazeLayout::GenerateRoomPurposes(int32 Seed)
{
	RoomPurposes.Init(EMazeRoomPurpose::Ward, Rooms.Num());
	WardNumbers.Init(0, Rooms.Num());

	FRandomStream Random(static_cast<int32>(uint32(Seed) ^ 0x71D41B27u));
	TArray<int32> GeneratorCandidates;
	bool bHasGenerator = false;

	for (int32 Index = 0; Index < Rooms.Num(); ++Index)
	{
		const FIntRect& Room = Rooms[Index];
		const int32 Area = Room.Width() * Room.Height();
		const EMazeRoomType PhysicalType = RoomType(Index);
		const bool bWardEligible = Room.Width() >= FMazeRoomDefinition::MinWardWidthCells &&
		                           Room.Height() >= FMazeRoomDefinition::MinWardLengthCells;

		if (PhysicalType == EMazeRoomType::Pool)
		{
			RoomPurposes[Index] = EMazeRoomPurpose::Pool;
			continue;
		}

		if (PhysicalType == EMazeRoomType::ShallowFlooded)
		{
			const int32 WaterPurposePick = Random.RandRange(0, 99);

			if (WaterPurposePick < 15)
			{
				RoomPurposes[Index] = EMazeRoomPurpose::Hydrotherapy;
				continue;
			}

			if (WaterPurposePick < 30)
			{
				RoomPurposes[Index] = EMazeRoomPurpose::Washroom;
				continue;
			}
		}

		if (PhysicalType == EMazeRoomType::Empty && Area >= 4)
			GeneratorCandidates.Add(Index);

		const int32 Pick = Random.RandRange(0, 99);
		EMazeRoomPurpose Purpose = EMazeRoomPurpose::Ward;

		if (Pick < 52)
			Purpose = bWardEligible ? EMazeRoomPurpose::Ward
			          : Pick < 26   ? EMazeRoomPurpose::Examination
			                        : EMazeRoomPurpose::Treatment;
		else if (Pick < 65)
			Purpose = EMazeRoomPurpose::Treatment;
		else if (Pick < 75)
			Purpose = EMazeRoomPurpose::Examination;
		else if (Pick < 81)
			Purpose = EMazeRoomPurpose::Isolation;
		else if (Pick < 86)
			Purpose = EMazeRoomPurpose::Staff;
		else if (Pick < 90)
			Purpose = EMazeRoomPurpose::Records;
		else if (Pick < 94)
			Purpose = EMazeRoomPurpose::Storage;
		else
			Purpose = EMazeRoomPurpose::Laundry;

		if (PhysicalType == EMazeRoomType::Empty && Area >= 4 && Pick >= 95 && Pick < 97 && !bHasGenerator)
		{
			Purpose = EMazeRoomPurpose::Generator;
			bHasGenerator = true;
		}
		else if (PhysicalType == EMazeRoomType::Empty && Area >= 8 && Pick >= 97)
			Purpose = Pick == 99 ? EMazeRoomPurpose::Dining : EMazeRoomPurpose::Recreation;

		RoomPurposes[Index] = Purpose;
	}

	if (!bHasGenerator && !GeneratorCandidates.IsEmpty())
		RoomPurposes[GeneratorCandidates[Random.RandRange(0, GeneratorCandidates.Num() - 1)]] =
		    EMazeRoomPurpose::Generator;

	TArray<int32> Wards;

	for (int32 Index = 0; Index < Rooms.Num(); ++Index)
		if (RoomPurposes[Index] == EMazeRoomPurpose::Ward)
			Wards.Add(Index);

	Wards.Sort(
	    [this](int32 A, int32 B)
	    {
		    return Rooms[A].Min.Y == Rooms[B].Min.Y ? Rooms[A].Min.X < Rooms[B].Min.X : Rooms[A].Min.Y < Rooms[B].Min.Y;
	    });

	for (int32 Index = 0; Index < Wards.Num(); ++Index)
		WardNumbers[Wards[Index]] = Index + 1;
}

void FMazeLayout::GenerateRoomTypes(int32 Seed)
{
	RoomTypes.Init(EMazeRoomType::Empty, Rooms.Num());

	if (Rooms.IsEmpty())
		return;

	FRandomStream Random(static_cast<int32>(uint32(Seed) ^ 0xA341316Cu));
	TArray<int32> PoolCandidates;
	TArray<int32> ShallowCandidates;
	const TArray<FMazeDoorway> Doorways = RoomDoorways();
	bool bHasPool = false;

	for (int32 RoomIndex = 0; RoomIndex < Rooms.Num(); ++RoomIndex)
	{
		const FIntRect& Room = Rooms[RoomIndex];
		int32 DoorCount = 0;

		for (const FMazeDoorway& Doorway : Doorways)
			DoorCount += Doorway.RoomIndex == RoomIndex;

		const bool bShallowEligible = Room.Width() >= FMazeRoomDefinition::MinShallowWidthCells &&
		                              Room.Height() >= FMazeRoomDefinition::MinShallowLengthCells;
		const bool bPoolEligible = Room.Width() >= FMazeRoomDefinition::MinPoolWidthCells &&
		                           Room.Height() >= FMazeRoomDefinition::MinPoolLengthCells && DoorCount >= 2;
		const int32 TotalWeight = FMazeRoomDefinition::EmptyWeight +
		                          (bShallowEligible ? FMazeRoomDefinition::ShallowFloodedWeight : 0) +
		                          (bPoolEligible ? FMazeRoomDefinition::PoolWeight : 0);
		const int32 Pick = Random.RandRange(1, TotalWeight);

		if (Pick <= FMazeRoomDefinition::EmptyWeight)
			RoomTypes[RoomIndex] = EMazeRoomType::Empty;
		else if (Pick <= FMazeRoomDefinition::EmptyWeight + FMazeRoomDefinition::ShallowFloodedWeight)
			RoomTypes[RoomIndex] = EMazeRoomType::ShallowFlooded;
		else
			RoomTypes[RoomIndex] = EMazeRoomType::Pool;

		if (bShallowEligible)
			ShallowCandidates.Add(RoomIndex);

		if (bPoolEligible)
			PoolCandidates.Add(RoomIndex);

		bHasPool |= RoomTypes[RoomIndex] == EMazeRoomType::Pool;
	}

	// Large generated maps should always expose the new room families. These fallbacks
	// remain deterministic and never turn a dead-end or undersized room into a pool.
	if (!bHasPool && !PoolCandidates.IsEmpty())
		RoomTypes[PoolCandidates[Random.RandRange(0, PoolCandidates.Num() - 1)]] = EMazeRoomType::Pool;

	if (!RoomTypes.Contains(EMazeRoomType::ShallowFlooded) && !ShallowCandidates.IsEmpty())
	{
		TArray<int32> Choices = ShallowCandidates.FilterByPredicate(
		    [this](int32 RoomIndex)
		    {
			    return RoomType(RoomIndex) != EMazeRoomType::Pool;
		    });

		if (!Choices.IsEmpty())
			RoomTypes[Choices[Random.RandRange(0, Choices.Num() - 1)]] = EMazeRoomType::ShallowFlooded;
	}
}

void FMazeLayout::GenerateNarrowPassages(int32 Seed, const TArray<int32>& ScenicFloor)
{
	NarrowPassages.Init(0, Walls.Num());

	TArray<bool> Blocked;

	Blocked.Init(false, Walls.Num());

	for (const int32 Cell : ScenicFloor)
		if (Blocked.IsValidIndex(Cell))
			Blocked[Cell] = true;

	for (const FIntRect& Room : Rooms)
		for (int32 Y = Room.Min.Y; Y < Room.Max.Y; ++Y)
			for (int32 X = Room.Min.X; X < Room.Max.X; ++X)
				Blocked[Y * Size + X] = true;

	const TArray<uint8> RoomDoorSides = RoomDoorApproachSides();

	for (int32 Y = 0; Y < Size; ++Y)
		for (int32 X = 0; X < Size; ++X)
		{
			bool bNearEntrance = false;

			for (int32 OffsetY = -1; OffsetY <= 1 && !bNearEntrance; ++OffsetY)
				for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
					bNearEntrance |= IsEntranceCell(X + OffsetX, Y + OffsetY);

			Blocked[Y * Size + X] = Blocked[Y * Size + X] || bNearEntrance;
		}

	for (const int32 Exit : Exits)
	{
		const int32 ExitX = Exit % Size, ExitY = Exit / Size;

		for (int32 OffsetY = -1; OffsetY <= 1; ++OffsetY)
			for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
			{
				const int32 X = ExitX + OffsetX, Y = ExitY + OffsetY;

				if (X >= 0 && Y >= 0 && X < Size && Y < Size)
					Blocked[Y * Size + X] = true;
			}
	}

	TArray<uint8> EligibleAxis;

	EligibleAxis.Init(0, Walls.Num());

	int32 EligibleCount = 0;

	for (int32 Cell = 0; Cell < Walls.Num(); ++Cell)
	{
		if (Blocked[Cell])
			continue;

		const uint8 OpenSides = ~Walls[Cell] & 15;
		const uint8 NonRoomOpenSides = OpenSides & ~RoomDoorSides[Cell];

		if (NonRoomOpenSides == 10)
			EligibleAxis[Cell] = 1;
		else if (NonRoomOpenSides == 5)
			EligibleAxis[Cell] = 2;

		EligibleCount += EligibleAxis[Cell] != 0;
	}

	struct FCandidate
	{
		int32 Cell = 0;
		int32 Length = 0;
		uint8 Axis = 0;
	};
	TArray<FCandidate> Candidates;
	FRandomStream Random(static_cast<int32>(uint32(Seed) ^ 0x6D2B79F5u));

	const auto AddRun = [&](int32 Start, int32 Length, int32 Step, uint8 Axis)
	{
		for (int32 Offset = 0; Offset + FMazeNarrowPassageDefinition::MinCells <= Length; ++Offset)
		{
			const int32 Remaining = Length - Offset;
			const int32 SegmentLength = Random.RandRange(FMazeNarrowPassageDefinition::MinCells,
			                                             FMath::Min(FMazeNarrowPassageDefinition::MaxCells, Remaining));

			Candidates.Add({Start + Offset * Step, SegmentLength, Axis});
		}
	};

	for (int32 Y = 0; Y < Size; ++Y)
		for (int32 X = 0; X < Size;)
		{
			const int32 StartX = X;

			while (X < Size && EligibleAxis[Y * Size + X] == 1)
				++X;

			if (X - StartX >= FMazeNarrowPassageDefinition::MinCells)
				AddRun(Y * Size + StartX, X - StartX, 1, 1);

			if (X == StartX)
				++X;
		}

	for (int32 X = 0; X < Size; ++X)
		for (int32 Y = 0; Y < Size;)
		{
			const int32 StartY = Y;

			while (Y < Size && EligibleAxis[Y * Size + X] == 2)
				++Y;

			if (Y - StartY >= FMazeNarrowPassageDefinition::MinCells)
				AddRun(StartY * Size + X, Y - StartY, Size, 2);

			if (Y == StartY)
				++Y;
		}

	for (int32 I = Candidates.Num() - 1; I > 0; --I)
		Candidates.Swap(I, Random.RandRange(0, I));

	const int32 TargetCells = FMath::RoundToInt(EligibleCount * FMazeNarrowPassageDefinition::CoverageFraction);
	int32 PlacedCells = 0;

	for (const FCandidate& Candidate : Candidates)
	{
		if (PlacedCells >= TargetCells)
			break;

		const int32 Step = Candidate.Axis == 1 ? 1 : Size;
		bool bSeparated = true;

		for (int32 I = 0; I < Candidate.Length && bSeparated; ++I)
		{
			const int32 Cell = Candidate.Cell + I * Step;
			const int32 CellX = Cell % Size, CellY = Cell / Size;

			for (int32 OffsetY = -FMazeNarrowPassageDefinition::MinSpacingCells;
			     OffsetY <= FMazeNarrowPassageDefinition::MinSpacingCells && bSeparated;
			     ++OffsetY)
				for (int32 OffsetX = -FMazeNarrowPassageDefinition::MinSpacingCells;
				     OffsetX <= FMazeNarrowPassageDefinition::MinSpacingCells;
				     ++OffsetX)
				{
					const int32 X = CellX + OffsetX, Y = CellY + OffsetY;

					if (X >= 0 && Y >= 0 && X < Size && Y < Size && NarrowPassages[Y * Size + X] != 0)
					{
						bSeparated = false;
						break;
					}
				}
		}

		if (!bSeparated)
			continue;

		for (int32 I = 0; I < Candidate.Length; ++I)
			NarrowPassages[Candidate.Cell + I * Step] = Candidate.Axis;

		PlacedCells += Candidate.Length;
	}
}

void FMazeLayout::ReserveRooms(FRandomStream& Random)
{
	Rooms.Reset();

	const int32 SectorCount = FMath::DivideAndRoundUp(Size, FMazeRoomDefinition::SectorSide);
	int32 SizeWeight = 0;

	for (const auto& Range : FMazeRoomDefinition::SizeRanges)
		SizeWeight += Range.Weight;

	const auto TryReserve = [&](const FIntRect& Room)
	{
		// Retain a corridor cell between rooms within a sector as well as at its edges.
		if (OverlapsEntrance(Room) || Rooms.ContainsByPredicate(
		                                  [&](const FIntRect& Existing)
		                                  {
			                                  return Room.Min.X <= Existing.Max.X && Room.Max.X >= Existing.Min.X &&
			                                         Room.Min.Y <= Existing.Max.Y && Room.Max.Y >= Existing.Min.Y;
		                                  }))
			return false;

		Rooms.Add(Room);

		return true;
	};

	// Cover every sector before filling its next slot, preserving the first size draw.
	for (int32 Slot = 0; Slot < FMazeRoomDefinition::RoomsPerSector; ++Slot)
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

				if (MaxWidth < FMazeRoomDefinition::MinBaseWidth || MaxLength < FMazeRoomDefinition::MinBaseLength)
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

				// Exhaust the smallest footprint before skipping a crowded or entrance-constrained slot.
				// Thus unlucky random attempts cannot leave an otherwise usable region empty.
				for (int32 Y = Bounds.Min.Y; Y + FMazeRoomDefinition::MinBaseLength <= Bounds.Max.Y && !bPlaced; ++Y)
					for (int32 X = Bounds.Min.X; X + FMazeRoomDefinition::MinBaseWidth <= Bounds.Max.X && !bPlaced; ++X)
						bPlaced = TryReserve(FIntRect(
						    X, Y, X + FMazeRoomDefinition::MinBaseWidth, Y + FMazeRoomDefinition::MinBaseLength));
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

	for (int32 Cell = 0; Cell < NarrowPassages.Num(); ++Cell)
		if (NarrowPassages[Cell] != 0)
		{
			const int32 CellX = Cell % Size, CellY = Cell / Size;

			for (int32 OffsetY = -1; OffsetY <= 1; ++OffsetY)
				for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
				{
					const int32 X = CellX + OffsetX, Y = CellY + OffsetY;

					if (X >= 0 && Y >= 0 && X < Size && Y < Size)
						Protected[Y * Size + X] = true;
				}
		}

	for (int32 Cell = 0; Cell < CrawlwaySides.Num(); ++Cell)
		if (CrawlwaySides[Cell] != 0)
		{
			const int32 CellX = Cell % Size, CellY = Cell / Size;

			for (int32 OffsetY = -1; OffsetY <= 1; ++OffsetY)
				for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
				{
					const int32 X = CellX + OffsetX, Y = CellY + OffsetY;

					if (X >= 0 && Y >= 0 && X < Size && Y < Size)
						Protected[Y * Size + X] = true;
				}
		}

	for (const FIntRect& Room : Rooms)
		for (int32 Y = Room.Min.Y; Y < Room.Max.Y; ++Y)
			for (int32 X = Room.Min.X; X < Room.Max.X; ++X)
				Protected[Y * Size + X] = true;

	const int32 DX[] = {0, 1, 0, -1}, DY[] = {-1, 0, 1, 0};

	for (const FMazeDoorway& Door : RoomDoorways())
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

TArray<FMazeDoorway> FMazeLayout::RoomDoorways() const
{
	TArray<FMazeDoorway> Result;
	const auto Add = [&](int32 X, int32 Y, int32 Direction, int32 RoomIndex)
	{
		if (!(Walls[Y * Size + X] & (1 << Direction)) && !IsCrawlway(Y * Size + X, Direction))
			Result.Add({FIntPoint(X, Y), Direction, RoomIndex});
	};

	for (int32 RoomIndex = 0; RoomIndex < Rooms.Num(); ++RoomIndex)
	{
		const FIntRect& Room = Rooms[RoomIndex];

		for (int32 X = Room.Min.X; X < Room.Max.X; ++X)
		{
			Add(X, Room.Min.Y, 0, RoomIndex);
			Add(X, Room.Max.Y - 1, 2, RoomIndex);
		}

		for (int32 Y = Room.Min.Y; Y < Room.Max.Y; ++Y)
		{
			Add(Room.Min.X, Y, 3, RoomIndex);
			Add(Room.Max.X - 1, Y, 1, RoomIndex);
		}
	}

	return Result;
}

TArray<FMazeDoorway> FMazeLayout::Doorways() const
{
	TArray<FMazeDoorway> Result = RoomDoorways();

	Result.Append(CorridorDoorways);

	return Result;
}

TArray<uint8> FMazeLayout::RoomDoorApproachSides() const
{
	TArray<uint8> Result;

	Result.Init(0, Walls.Num());

	const int32 DX[] = {0, 1, 0, -1}, DY[] = {-1, 0, 1, 0};

	for (const FMazeDoorway& Door : RoomDoorways())
	{
		const FIntPoint Outside = Door.Cell + FIntPoint(DX[Door.Direction], DY[Door.Direction]);

		if (Outside.X >= 0 && Outside.Y >= 0 && Outside.X < Size && Outside.Y < Size)
			Result[Outside.Y * Size + Outside.X] |= 1 << ((Door.Direction + 2) % 4);
	}

	return Result;
}
