#include "ECS/MazeGameplaySystems.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMazePlayerControlTest,
                                 "Laby.Character.NarrowPassageControl",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMazePlayerControlTest::RunTest(const FString& Parameters)
{
	FMazePlayerInputFragment Input;
	FMazePlayerPoseFragment Pose;
	FMazeVitals Vitals;
	FMazeLocomotionFragment Locomotion;

	Input.Forward = 1.f;
	Input.bSprintHeld = true;
	Input.bJumpHeld = true;
	Input.bJumpPressed = true;
	Pose.bInputEnabled = true;
	Pose.bOnGround = true;
	Pose.Velocity = FVector(FMazePlayerControlDefinition::SprintSpeed, 0, 0);
	Pose.Acceleration = FVector::ForwardVector;

	const FMazePlayerCommandFragment Narrow = FMazePlayerControlSystem::Resolve(Input, Pose, Vitals, Locomotion, true);

	TestEqual(TEXT("Narrow passage caps movement at slow walking speed"),
	          Narrow.Speed,
	          FMazePlayerControlDefinition::NarrowPassageSpeed);
	TestFalse(TEXT("Narrow passage does not count as running"), Locomotion.bRunning);
	TestFalse(TEXT("Narrow passage rejects a new jump"), Narrow.bStartJump);
	TestFalse(TEXT("Narrow passage does not hold a jump"), Narrow.bJumpHeld);

	Input.bJumpPressed = true;

	const FMazePlayerCommandFragment Ordinary =
	    FMazePlayerControlSystem::Resolve(Input, Pose, Vitals, Locomotion, false);

	TestEqual(
	    TEXT("Ordinary corridor retains sprint speed"), Ordinary.Speed, FMazePlayerControlDefinition::SprintSpeed);
	TestTrue(TEXT("Ordinary sprint counts as running"), Locomotion.bRunning);
	TestTrue(TEXT("Ordinary corridor permits a jump"), Ordinary.bStartJump);

	return true;
}

#endif
