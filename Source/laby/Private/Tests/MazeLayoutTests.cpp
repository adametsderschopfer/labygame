#include "Maze/MazeLayout.h"
#include "Maze/MazeRoomDefinition.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMazeLayoutTest,
                                 "Laby.Maze.ConnectivityAndExits",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMazeLayoutTest::RunTest(const FString& Parameters)
{
	bool FoundTinyRoom = false, FoundLargestRoom = false;
	bool FoundBendSideRoom = false, FoundBendThroughRoom = false;

	for (int32 Seed = 1; Seed <= 100; ++Seed)
	{
		FMazeLayout Maze, Copy;

		Maze.Generate(Seed);
		TestEqual(TEXT("Default maze is 80 by 80"), Maze.Size, 80);
		TestEqual(TEXT("Default maze has 6400 cells"), Maze.Walls.Num(), 6400);

		const int32 BaseSectors = FMath::DivideAndRoundUp(Maze.Size, FMazeRoomDefinition::SectorSide);
		const int32 BendSectors = FMath::DivideAndRoundUp(Maze.Size, FMazeRoomDefinition::BendSectorSide);
		const int32 MaxBaseRooms = 1 + BaseSectors * BaseSectors * FMazeRoomDefinition::RoomsPerSector;
		const int32 MaxBendRooms = BendSectors * BendSectors * FMazeRoomDefinition::RoomsPerSector;

		TestTrue(TEXT("Default maze retains sector coverage and respects room quotas"),
		         Maze.Rooms.Num() >= 1 + BaseSectors * BaseSectors && Maze.Rooms.Num() <= MaxBaseRooms + MaxBendRooms);

		// Base slots may be skipped when crowded; entries beyond their maximum are always bend rooms.
		for (int32 I = MaxBaseRooms; I < Maze.Rooms.Num(); ++I)
		{
			const FIntRect& Room = Maze.Rooms[I];
			const uint8 Open = ~Maze.Walls[Room.Min.Y * Maze.Size + Room.Min.X] & 15;
			const bool bSideRoom = Open != 0 && (Open & (Open - 1)) == 0;
			const bool bThroughRoom = Open == 3 || Open == 6 || Open == 9 || Open == 12;

			TestTrue(TEXT("Additional rooms have one entrance or two adjacent doors"),
			         Room.Width() == 1 && Room.Height() == 1 && (bSideRoom || bThroughRoom));
			FoundBendSideRoom |= bSideRoom;
			FoundBendThroughRoom |= bThroughRoom;
		}

		for (int32 I = 1; I < Maze.Rooms.Num(); ++I)
		{
			const FIntRect& Room = Maze.Rooms[I];

			TestTrue(TEXT("Random room fits the configured size limits"),
			         Room.Width() >= FMazeRoomDefinition::MinWidth && Room.Width() <= FMazeRoomDefinition::MaxWidth &&
			             Room.Height() >= FMazeRoomDefinition::MinLength &&
			             Room.Height() <= FMazeRoomDefinition::MaxLength);
			FoundTinyRoom |= Room.Width() == 1 && Room.Height() == 1;
			FoundLargestRoom |=
			    Room.Width() == FMazeRoomDefinition::MaxWidth && Room.Height() == FMazeRoomDefinition::MaxLength;

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

				for (int32 I = 1; I < Maze.Rooms.Num(); ++I)
				{
					const FIntRect& Room = Maze.Rooms[I];

					if (Sector.Contains(Room.Min) && Sector.Contains(Room.Max - FIntPoint(1, 1)))
						++RoomCount;
				}

				TestTrue(TEXT("Every default-map sector retains a non-entrance room"), RoomCount >= 1);
			}

		Copy.Generate(Seed);
		TestTrue(TEXT("Same seed reproduces topology, rooms and holes"),
		         Maze.Walls == Copy.Walls && Maze.Exits == Copy.Exits && Maze.Rooms == Copy.Rooms &&
		             Maze.Holes == Copy.Holes);

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
	TestTrue(TEXT("Seed corpus includes side rooms near bends"), FoundBendSideRoom);
	TestTrue(TEXT("Seed corpus includes through-rooms on bends"), FoundBendThroughRoom);

	return true;
}

#endif
