#include "ECS/MazeExplorationSystem.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMazeExplorationMovementTest,
                                 "Laby.Exploration.MovementAndWalls",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMazeExplorationMovementTest::RunTest(const FString& Parameters)
{
	FMazeGenerationFragment Maze;
	auto Data = MakeShared<FMazeGeneratedData, ESPMode::ThreadSafe>();

	Data->Layout.Size = 8;
	Data->Layout.Walls.Init(15, 64);
	Data->Layout.Holes.Init(0, 64);

	for (int32 X = 1; X < 6; ++X)
	{
		Data->Layout.Walls[3 * 8 + X] &= ~2;
		Data->Layout.Walls[3 * 8 + X + 1] &= ~8;
	}

	Maze.Data = Data;
	Maze.Revision = 1;

	FMazePlayerPoseFragment Pose;

	Pose.bInputEnabled = true;
	Pose.Location = Maze.Origin + FVector(2.5 * Maze.Cell, 3.5 * Maze.Cell, 100);
	Pose.Forward = FVector(1, 0, 0);

	FMazeExplorationFragment East, West;
	const FMassEntityHandle Entity;

	FMazeExplorationSystem::Update(East, Entity, Maze, Pose, true);
	Pose.Forward = FVector(-1, 0, 0);
	FMazeExplorationSystem::Update(West, Entity, Maze, Pose, true);
	TestTrue(TEXT("Initial exploration does not depend on camera direction"), East.Seen == West.Seen);
	TestTrue(TEXT("Nearby floor behind the player is revealed"), East.Seen[3 * 8 + 1] != 0);
	TestTrue(TEXT("Floor at the radius is revealed"), East.Seen[3 * 8 + 4] != 0);
	TestFalse(TEXT("Floor beyond the radius stays unknown"), East.Seen[3 * 8 + 5] != 0);
	TestFalse(TEXT("Adjacent floor behind a wall stays unknown"), East.Seen[2 * 8 + 2] != 0);

	const TArray<uint8> BeforeTurn = East.Seen;

	FMazeExplorationSystem::Update(East, Entity, Maze, Pose, true);
	TestTrue(TEXT("Turning in place does not reveal more cells"), East.Seen == BeforeTurn);
	Pose.Location.X += Maze.Cell;
	FMazeExplorationSystem::Update(East, Entity, Maze, Pose, true);
	TestTrue(TEXT("Walking extends the discovered region"), East.Seen[3 * 8 + 5] != 0);
	TestTrue(TEXT("Previously explored floor stays known"), East.Seen[3 * 8 + 1] != 0);

	const TArray<uint8> BeforeDisabled = East.Seen;

	Pose.Location.X += Maze.Cell;
	Pose.bInputEnabled = false;
	FMazeExplorationSystem::Update(East, Entity, Maze, Pose, true);
	TestTrue(TEXT("Disabled input suspends exploration"), East.Seen == BeforeDisabled);
	Pose.bInputEnabled = true;
	FMazeExplorationSystem::Update(East, Entity, Maze, Pose, false);
	TestTrue(TEXT("Death suspends exploration"), East.Seen == BeforeDisabled);
	++Maze.Revision;
	FMazeExplorationSystem::Update(East, Entity, Maze, Pose, true);
	TestFalse(TEXT("New generation clears old discovery"), East.Seen[3 * 8 + 1] != 0);
	TestTrue(TEXT("New generation reveals the current position"), East.Seen[3 * 8 + 4] != 0);

	return true;
}

#endif
