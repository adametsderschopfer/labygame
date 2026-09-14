#include "MazeVitalsSystem.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMazeVitalsTest,
                                 "Laby.Character.Vitals",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMazeVitalsTest::RunTest(const FString& Parameters)
{
	FMazeVitals V;

	FMazeVitalsSystem::Update(V, 2.f, true);
	TestEqual(TEXT("Two seconds of sprint"), V.Stamina, 60.f);
	FMazeVitalsSystem::Update(V, 1.f, false);
	TestEqual(TEXT("Recovery waits after running"), V.Stamina, 60.f);
	FMazeVitalsSystem::Update(V, 1.f, false);
	TestEqual(TEXT("Only time after delay regenerates"), V.Stamina, 67.5f);
	FMazeVitalsSystem::Update(V, 10.f, true);
	TestEqual(TEXT("Stamina cannot go negative"), V.Stamina, 0.f);
	TestFalse(TEXT("Exhaustion blocks sprint"), FMazeVitalsSystem::CanSprint(V));
	FMazeVitalsSystem::Update(V, 2.5f, false);
	TestEqual(TEXT("Recovery after exhaustion"), V.Stamina, 15.f);
	TestFalse(TEXT("Low stamina does not cause sprint flicker"), FMazeVitalsSystem::CanSprint(V));
	FMazeVitalsSystem::Update(V, 1.f, false);
	TestTrue(TEXT("Sprint returns above threshold"), FMazeVitalsSystem::CanSprint(V));
	FMazeVitalsSystem::Update(V, 100.f, false);
	TestEqual(TEXT("Recovery caps at maximum"), V.Stamina, 100.f);
	FMazeVitalsSystem::Update(V, -1.f, true);
	TestEqual(TEXT("Negative time ignored"), V.Stamina, 100.f);
	TestEqual(TEXT("Damage applied"), FMazeVitalsSystem::Damage(V, 35.f), 35.f);
	TestEqual(TEXT("Health decreases"), V.Health, 65.f);
	TestEqual(TEXT("Negative damage ignored"), FMazeVitalsSystem::Damage(V, -20.f), 0.f);
	TestEqual(TEXT("Overkill returns remaining health"), FMazeVitalsSystem::Damage(V, 1000.f), 65.f);
	TestFalse(TEXT("Zero health is dead"), FMazeVitalsSystem::IsAlive(V));
	TestFalse(TEXT("Dead player cannot sprint"), FMazeVitalsSystem::CanSprint(V));
	TestEqual(TEXT("Dead player takes no further damage"), FMazeVitalsSystem::Damage(V, 10.f), 0.f);
	FMazeVitalsSystem::Update(V, 5.f, true);
	TestEqual(TEXT("Dead player does not consume stamina"), V.Stamina, 100.f);

	FMazeVitals FreshLife;

	TestEqual(TEXT("New life health"), FreshLife.Health, 100.f);
	TestEqual(TEXT("New life stamina"), FreshLife.Stamina, 100.f);
	TestTrue(TEXT("Jump spends stamina"), FMazeVitalsSystem::SpendJumpStamina(FreshLife));
	TestEqual(TEXT("Jump costs 15"), FreshLife.Stamina, 85.f);
	FMazeVitalsSystem::Update(FreshLife, 3.f, false, false);
	TestEqual(TEXT("No recovery in air"), FreshLife.Stamina, 85.f);
	FMazeVitalsSystem::Update(FreshLife, 0.5f, false, true);
	TestEqual(TEXT("Walking or standing on ground recovers"), FreshLife.Stamina, 92.5f);
	FreshLife.Stamina = 14.f;
	TestFalse(TEXT("Insufficient stamina blocks jump"), FMazeVitalsSystem::CanJump(FreshLife));
	TestFalse(TEXT("Failed jump does not spend"), FMazeVitalsSystem::SpendJumpStamina(FreshLife));
	TestEqual(TEXT("Failed jump preserves stamina"), FreshLife.Stamina, 14.f);
	FreshLife.Stamina = FMazeVitals::JumpCost;
	TestTrue(TEXT("Exact cost permits jump"), FMazeVitalsSystem::SpendJumpStamina(FreshLife));
	TestEqual(TEXT("Exact cost leaves zero"), FreshLife.Stamina, 0.f);
	TestFalse(TEXT("Jump exhaustion blocks sprint"), FMazeVitalsSystem::CanSprint(FreshLife));
	FMazeVitalsSystem::Damage(FreshLife, 100.f);
	FreshLife.Stamina = 100.f;
	TestFalse(TEXT("Dead player cannot jump"), FMazeVitalsSystem::CanJump(FreshLife));

	return true;
}

#endif
