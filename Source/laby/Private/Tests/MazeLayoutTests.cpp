#include "Maze/MazeLayout.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMazeLayoutTest,
                                 "Laby.Maze.ConnectivityAndExits",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMazeLayoutTest::RunTest(const FString& Parameters)
{
	for (int32 Seed = 1; Seed <= 100; ++Seed)
	{
		FMazeLayout Maze, Copy;

		Maze.Generate(Seed);
		TestEqual(TEXT("Default maze is 80 by 80"), Maze.Size, 80);
		TestEqual(TEXT("Default maze has 6400 cells"), Maze.Walls.Num(), 6400);
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

	return true;
}

#endif
