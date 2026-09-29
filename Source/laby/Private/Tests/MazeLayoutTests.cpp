#include "Maze/MazeLayout.h"
#include "Maze/MazeNarrowPassageDefinition.h"
#include "Maze/MazeRoomDefinition.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMazeLayoutTest,
                                 "Laby.Maze.ConnectivityAndExits",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMazeLayoutTest::RunTest(const FString& Parameters)
{
	bool FoundTinyRoom = false, FoundLargestRoom = false;
	bool FoundDeadEndRoom = false;
	bool FoundNarrowRoomDoor = false;
	bool FoundShallowRoom = false, FoundPoolRoom = false;

	for (int32 Seed = 1; Seed <= 100; ++Seed)
	{
		FMazeLayout Maze, Copy;

		Maze.Generate(Seed);
		TestEqual(TEXT("Default maze is 80 by 80"), Maze.Size, 80);
		TestEqual(TEXT("Default maze has 6400 cells"), Maze.Walls.Num(), 6400);

		const int32 BaseSectors = FMath::DivideAndRoundUp(Maze.Size, FMazeRoomDefinition::SectorSide);
		const int32 DeadEndSectors = FMath::DivideAndRoundUp(Maze.Size, FMazeRoomDefinition::DeadEndSectorSide);
		const int32 MaxBaseRooms = BaseSectors * BaseSectors * FMazeRoomDefinition::RoomsPerSector;
		const int32 MaxDeadEndRooms = DeadEndSectors * DeadEndSectors * FMazeRoomDefinition::RoomsPerSector;

		TestTrue(TEXT("Default maze retains sector coverage and respects room quotas"),
		         Maze.Rooms.Num() >= BaseSectors * BaseSectors && Maze.Rooms.Num() <= MaxBaseRooms + MaxDeadEndRooms);
		TestEqual(TEXT("Every room has one deterministic type"), Maze.RoomTypes.Num(), Maze.Rooms.Num());
		TestEqual(TEXT("Spawn is a corridor dead end"), uint8(~Maze.Walls[Maze.Start()] & 15), uint8(2));
		TestTrue(TEXT("Spawn floor is intact"), Maze.HasFloor(Maze.Start()));

		for (int32 X = 1; X < FMazeLayout::EntranceLengthCells; ++X)
		{
			const int32 Cell = Maze.Start() + X;

			TestEqual(TEXT("Entrance stays a straight corridor"), uint8(~Maze.Walls[Cell] & 15), uint8(10));
			TestTrue(TEXT("Entrance floor is intact"), Maze.HasFloor(Cell));
		}

		// Base slots may be skipped when crowded; entries beyond their maximum are always dead-end rooms.
		for (int32 I = MaxBaseRooms; I < Maze.Rooms.Num(); ++I)
		{
			const FIntRect& Room = Maze.Rooms[I];

			int32 DoorCount = 0;

			for (const FMazeRoomDoorway& Doorway : Maze.RoomDoorways())
				DoorCount += Doorway.RoomIndex == I;

			TestTrue(TEXT("Additional rooms follow a straight dead-end branch"),
			         (Room.Width() == 1 || Room.Height() == 1) &&
			             FMath::Max(Room.Width(), Room.Height()) <= FMazeRoomDefinition::MaxDeadEndRoomLength &&
			             DoorCount == 1);
		}

		for (int32 I = 0; I < Maze.Rooms.Num(); ++I)
		{
			const FIntRect& Room = Maze.Rooms[I];

			TestTrue(TEXT("Random room fits the configured size limits"),
			         Room.Width() >= FMazeRoomDefinition::MinWidth && Room.Width() <= FMazeRoomDefinition::MaxWidth &&
			             Room.Height() >= FMazeRoomDefinition::MinLength &&
			             Room.Height() <= FMazeRoomDefinition::MaxLength);
			FoundTinyRoom |= Room.Width() == 1 && Room.Height() == 1;

			if (Room.Width() == 1 && Room.Height() == 1)
			{
				const uint8 Open = ~Maze.Walls[Room.Min.Y * Maze.Size + Room.Min.X] & 15;

				TestTrue(TEXT("One-cell rooms have exactly one doorway"), Open != 0 && (Open & (Open - 1)) == 0);
				FoundDeadEndRoom |= Open != 0 && (Open & (Open - 1)) == 0;
			}

			FoundLargestRoom |=
			    Room.Width() == FMazeRoomDefinition::MaxWidth && Room.Height() == FMazeRoomDefinition::MaxLength;
			FoundShallowRoom |= Maze.RoomType(I) == EMazeRoomType::ShallowFlooded;
			FoundPoolRoom |= Maze.RoomType(I) == EMazeRoomType::Pool;

			if (Maze.RoomType(I) == EMazeRoomType::Pool)
			{
				int32 DoorCount = 0;

				for (const FMazeRoomDoorway& Doorway : Maze.RoomDoorways())
					DoorCount += Doorway.RoomIndex == I;

				TestTrue(TEXT("Pool rooms are large enough for water and a bridge"),
				         Room.Width() >= FMazeRoomDefinition::MinPoolWidthCells &&
				             Room.Height() >= FMazeRoomDefinition::MinPoolLengthCells);
				TestTrue(TEXT("Pool rooms are always through-rooms"), DoorCount >= 2);
			}

			for (int32 J = 0; J < I; ++J)
			{
				const FIntRect& Other = Maze.Rooms[J];

				TestTrue(TEXT("A corridor cell separates every pair of rooms"),
				         Room.Min.X > Other.Max.X || Room.Max.X < Other.Min.X || Room.Min.Y > Other.Max.Y ||
				             Room.Max.Y < Other.Min.Y);
			}
		}

		for (int32 SectorY = 0; SectorY < BaseSectors; ++SectorY)
			for (int32 SectorX = 0; SectorX < BaseSectors; ++SectorX)
			{
				const FIntRect Sector(SectorX * Maze.Size / BaseSectors,
				                      SectorY * Maze.Size / BaseSectors,
				                      (SectorX + 1) * Maze.Size / BaseSectors,
				                      (SectorY + 1) * Maze.Size / BaseSectors);
				int32 RoomCount = 0;

				for (int32 I = 0; I < Maze.Rooms.Num(); ++I)
				{
					const FIntRect& Room = Maze.Rooms[I];

					if (Sector.Contains(Room.Min) && Sector.Contains(Room.Max - FIntPoint(1, 1)))
						++RoomCount;
				}

				TestTrue(TEXT("Every default-map sector retains a non-entrance room"), RoomCount >= 1);
			}

		Copy.Generate(Seed);
		TestTrue(TEXT("Same seed reproduces topology, room types, narrow passages and holes"),
		         Maze.Walls == Copy.Walls && Maze.Exits == Copy.Exits && Maze.Rooms == Copy.Rooms &&
		             Maze.RoomTypes == Copy.RoomTypes && Maze.NarrowPassages == Copy.NarrowPassages &&
		             Maze.Holes == Copy.Holes);

		int32 NarrowCells = 0;
		const TArray<uint8> RoomDoorSides = Maze.RoomDoorApproachSides();

		for (int32 Cell = 0; Cell < Maze.NarrowPassages.Num(); ++Cell)
		{
			const uint8 Axis = Maze.NarrowPassages[Cell];

			if (Axis == 0)
				continue;

			++NarrowCells;
			TestTrue(TEXT("Narrow passages use a known axis"), Axis == 1 || Axis == 2);

			const uint8 OpenSides = ~Maze.Walls[Cell] & 15;
			const uint8 NonRoomOpenSides = OpenSides & ~RoomDoorSides[Cell];

			TestEqual(TEXT("Narrow passages retain a straight non-room corridor"),
			          NonRoomOpenSides,
			          uint8(Axis == 1 ? 10 : 5));
			TestEqual(TEXT("Room doorway side masks always describe open topology"),
			          uint8(RoomDoorSides[Cell] & ~OpenSides),
			          uint8(0));
			FoundNarrowRoomDoor |= RoomDoorSides[Cell] != 0;
			TestTrue(TEXT("Narrow passage floor cannot become a hole"), Maze.HasFloor(Cell));

			const FIntPoint Point(Cell % Maze.Size, Cell / Maze.Size);

			TestFalse(TEXT("Narrow passages never occupy rooms"),
			          Maze.Rooms.ContainsByPredicate(
			              [Point](const FIntRect& Room)
			              {
				              return Room.Contains(Point);
			              }));

			for (int32 OffsetY = -1; OffsetY <= 1; ++OffsetY)
				for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
				{
					const int32 X = Point.X + OffsetX, Y = Point.Y + OffsetY;

					if (X >= 0 && Y >= 0 && X < Maze.Size && Y < Maze.Size)
						TestTrue(TEXT("Narrow passage approach remains solid"), Maze.HasFloor(Y * Maze.Size + X));
				}

			const int32 Previous = Axis == 1 ? Cell - 1 : Cell - Maze.Size;
			const bool bHasPrevious = Axis == 1 ? Point.X > 0 : Point.Y > 0;

			if (!bHasPrevious || Maze.NarrowPassages[Previous] != Axis)
			{
				int32 SegmentCells = 1;

				while (Axis == 1 && Point.X + SegmentCells < Maze.Size &&
				       Maze.NarrowPassages[Cell + SegmentCells] == Axis)
					++SegmentCells;

				while (Axis == 2 && Point.Y + SegmentCells < Maze.Size &&
				       Maze.NarrowPassages[Cell + SegmentCells * Maze.Size] == Axis)
					++SegmentCells;

				TestTrue(TEXT("Narrow passage length stays within its definition"),
				         SegmentCells >= FMazeNarrowPassageDefinition::MinCells &&
				             SegmentCells <= FMazeNarrowPassageDefinition::MaxCells);
			}
		}

		TestTrue(TEXT("Default maze contains narrow passages"), NarrowCells > 0);

		TSet<int32> Seen;
		TArray<int32> Queue;

		Queue.Add(Maze.Start());
		Seen.Add(Maze.Start());

		int32 Openings = 0;
		const int32 DX[] = {0, 1, 0, -1}, DY[] = {-1, 0, 1, 0};

		for (int32 Head = 0; Head < Queue.Num(); ++Head)
		{
			int32 C = Queue[Head];

			for (int32 D = 0; D < 4; ++D)
			{
				if (Maze.Walls[C] & (1 << D))
					continue;

				int32 X = C % Maze.Size + DX[D], Y = C / Maze.Size + DY[D];

				if (X < 0 || Y < 0 || X >= Maze.Size || Y >= Maze.Size)
				{
					++Openings;
					continue;
				}

				int32 N = Y * Maze.Size + X;

				if (Maze.Walls[N] & (1 << ((D + 2) % 4)))
				{
					AddError(TEXT("Asymmetric wall"));

					return false;
				}

				if (!Seen.Contains(N))
				{
					Seen.Add(N);
					Queue.Add(N);
				}
			}
		}

		TestEqual(TEXT("All cells reachable from A"), Seen.Num(), Maze.Size * Maze.Size);
		TestEqual(TEXT("Exactly one boundary opening"), Openings, 1);

		for (int32 Exit : Maze.Exits)
			TestTrue(TEXT("Exit reachable"), Seen.Contains(Exit));

		Copy.Generate(Seed + 1);
		TestTrue(TEXT("Different seed changes maze"), Maze.Walls != Copy.Walls);
	}

	TestTrue(TEXT("Seed corpus includes compact one-cell rooms"), FoundTinyRoom);
	TestTrue(TEXT("Seed corpus retains the previous largest rooms"), FoundLargestRoom);
	TestTrue(TEXT("Seed corpus includes rooms at dead ends"), FoundDeadEndRoom);
	TestTrue(TEXT("Seed corpus includes room doors opening inside narrow passages"), FoundNarrowRoomDoor);
	TestTrue(TEXT("Seed corpus includes shallow flooded rooms"), FoundShallowRoom);
	TestTrue(TEXT("Seed corpus includes bridged pool rooms"), FoundPoolRoom);

	return true;
}

#endif
