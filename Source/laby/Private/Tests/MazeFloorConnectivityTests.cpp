#include "ECS/MazeGameplaySystems.h"
#include "Maze/MazeRoomDefinition.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMazeFloorConnectivityTest,
                                 "Laby.Maze.RoomsAndHolesRemainWalkable",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMazeFloorConnectivityTest::RunTest(const FString& Parameters)
{
	for (int32 Size : {8, 16, 40, 80, 100})
		for (int32 Seed : {1, 2, 42, 98765, -1, MAX_int32})
		{
			FMazeGenerationFragment Generation;
			Generation.Seed = Seed;
			Generation.Size = Size;
			FMazeGenerationSystem::Generate(Generation);
			const auto& Data = *Generation.Data;
			const auto& Layout = Data.Layout;
			TSet<int32> Seen;
			TArray<int32> Queue;
			Queue.Add(Layout.Start());
			Seen.Add(Layout.Start());
			const int32 DX[] = {0, 1, 0, -1}, DY[] = {-1, 0, 1, 0};

			for (int32 Head = 0; Head < Queue.Num(); ++Head)
			{
				const int32 C = Queue[Head];

				for (int32 D = 0; D < 4; ++D)
				{
					if (Layout.Walls[C] & (1 << D))
						continue;

					const int32 X = C % Size + DX[D], Y = C / Size + DY[D];

					if (X < 0 || Y < 0 || X >= Size || Y >= Size)
						continue;

					const int32 Next = Y * Size + X;

					if (Layout.Holes[Next] || Seen.Contains(Next))
						continue;

					Seen.Add(Next);
					Queue.Add(Next);
				}
			}

			TestEqual(
			    TEXT("All surviving floor is reachable without jumping"), Seen.Num(), Size * Size - Layout.NumHoles());
			TestEqual(TEXT("Still exactly one exit"), Layout.Exits.Num(), 1);

			for (int32 Exit : Layout.Exits)
				TestTrue(TEXT("Exit has a walkable route"), Seen.Contains(Exit));

			for (const FVector& Spawn : Data.PlayerStarts)
			{
				const int32 C =
				    FMath::FloorToInt(Spawn.Y / Generation.Cell) * Size + FMath::FloorToInt(Spawn.X / Generation.Cell);
				TestTrue(TEXT("Every player spawns over connected floor"), Seen.Contains(C));
			}

			int32 TotalDoors = 0, ThroughRooms = 0;

			for (const auto& Room : Layout.Rooms)
			{
				int32 DoorCount = 0;
				uint8 DoorSides = 0;

				TestTrue(TEXT("Rooms stay inside the boundary"),
				         Room.Min.X >= 0 && Room.Min.Y >= 0 && Room.Max.X <= Size && Room.Max.Y <= Size);

				for (int32 Y = Room.Min.Y; Y < Room.Max.Y; ++Y)
					for (int32 X = Room.Min.X; X < Room.Max.X; ++X)
					{
						const uint8 Walls = Layout.Walls[Y * Size + X];
						TestTrue(TEXT("Room floor has no holes"), Layout.HasFloor(Y * Size + X));

						for (int32 D = 0; D < 4; ++D)
						{
							const FIntPoint Outside(X + DX[D], Y + DY[D]);

							if (Room.Contains(Outside) || (Walls & (1 << D)))
								continue;

							++DoorCount;
							DoorSides |= 1 << D;
							TestTrue(TEXT("Door connects to reachable corridor floor"),
							         Seen.Contains(Outside.Y * Size + Outside.X));
						}

						if (X + 1 < Room.Max.X)
							TestFalse(TEXT("No east interior room wall"), (Walls & 2) != 0);

						if (Y + 1 < Room.Max.Y)
							TestFalse(TEXT("No south interior room wall"), (Walls & 4) != 0);
					}

				if (Room == Layout.Rooms[0])
					TestEqual(TEXT("Entrance room keeps one doorway"), DoorCount, 1);
				else
					TestTrue(TEXT("Random room has one or two doorways"), DoorCount == 1 || DoorCount == 2);

				if (DoorCount == 2)
				{
					++ThroughRooms;
					TestTrue(TEXT("Through-room doors turn a corner rather than align"),
					         DoorSides != 5 && DoorSides != 10);
				}

				TotalDoors += DoorCount;
			}

			TestEqual(TEXT("Derived door list matches open perimeter edges"), Layout.RoomDoorways().Num(), TotalDoors);

			if (Size >= 40)
			{
				TestTrue(TEXT("Large maps have random rooms beyond the spawn room"), Layout.Rooms.Num() > 1);
				TestTrue(TEXT("Large maps retain through-rooms"), ThroughRooms > 0);
				TestTrue(TEXT("Large maps contain holes"), Layout.NumHoles() > 0);
			}

			// Check actual cube footprints, not just the topology mask. A surviving monolithic
			// floor would violate this even if the graph's connectivity check passed.
			for (int32 C = 0; C < Layout.Holes.Num(); ++C)
			{
				const FVector Point((C % Size + 0.5f) * Generation.Cell, (C / Size + 0.5f) * Generation.Cell, 0);
				int32 CoverCount = 0;

				for (const auto& Transform : Data.FloorTransforms)
				{
					const FVector Local = Transform.InverseTransformPosition(Point);

					if (FMath::Abs(Local.X) < 50.f && FMath::Abs(Local.Y) < 50.f)
						++CoverCount;
				}

				TestEqual(TEXT("Floor geometry matches hole mask"), CoverCount, Layout.Holes[C] ? 0 : 1);
			}

			TestEqual(TEXT("Falling below the exit is not a win"),
			          FMazeGenerationSystem::ExitAt(Generation, FVector(0, -100, -600)),
			          0);
		}

	return true;
}

#endif
