#include "MazeVitals.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMazeVitalsTest, "Laby.Character.Vitals", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMazeVitalsTest::RunTest(const FString& Parameters)
{
	FMazeVitals V;
	V.Update(2.f, true);
	TestEqual(TEXT("Two seconds of sprint"), V.Stamina, 60.f);
	V.Update(1.f, false);
	TestEqual(TEXT("Recovery waits after running"), V.Stamina, 60.f);
	V.Update(1.f, false);
	TestEqual(TEXT("Only time after delay regenerates"), V.Stamina, 67.5f);
	V.Update(10.f, true);
	TestEqual(TEXT("Stamina cannot go negative"), V.Stamina, 0.f);
	TestFalse(TEXT("Exhaustion blocks sprint"), V.CanSprint());
	V.Update(2.5f, false);
	TestEqual(TEXT("Recovery after exhaustion"), V.Stamina, 15.f);
	TestFalse(TEXT("Low stamina does not cause sprint flicker"), V.CanSprint());
	V.Update(1.f, false);
	TestTrue(TEXT("Sprint returns above threshold"), V.CanSprint());
	V.Update(100.f, false);
	TestEqual(TEXT("Recovery caps at maximum"), V.Stamina, 100.f);
	V.Update(-1.f, true);
	TestEqual(TEXT("Negative time ignored"), V.Stamina, 100.f);
	TestEqual(TEXT("Damage applied"), V.Damage(35.f), 35.f);
	TestEqual(TEXT("Health decreases"), V.Health, 65.f);
	TestEqual(TEXT("Negative damage ignored"), V.Damage(-20.f), 0.f);
	TestEqual(TEXT("Overkill returns remaining health"), V.Damage(1000.f), 65.f);
	TestFalse(TEXT("Zero health is dead"), V.IsAlive());
	TestFalse(TEXT("Dead player cannot sprint"), V.CanSprint());
	TestEqual(TEXT("Dead player takes no further damage"), V.Damage(10.f), 0.f);
	V.Update(5.f, true);
	TestEqual(TEXT("Dead player does not consume stamina"), V.Stamina, 100.f);
	FMazeVitals FreshLife;
	TestEqual(TEXT("New life health"), FreshLife.Health, 100.f);
	TestEqual(TEXT("New life stamina"), FreshLife.Stamina, 100.f);
	return true;
}
#endif
