#include "MazePlayerController.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMazePreferencesTest,
                                 "Laby.Menu.SensitivityPersistence",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMazePreferencesTest::RunTest(const FString& Parameters)
{
	const FString Path =
	    FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/MazePreferencesTest.ini"));

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);

	auto* Written = NewObject<UMazePreferences>();

	Written->MouseSensitivity = 1.75f;
	Written->SaveConfig(CPF_Config, *Path);

	auto* Read = NewObject<UMazePreferences>();

	Read->ReloadConfig(nullptr, *Path);
	TestEqual(TEXT("Sensitivity survives config reload"), Read->GetSensitivity(), 1.75f);
	Read->MouseSensitivity = -10;
	TestEqual(TEXT("Minimum sensitivity"), Read->GetSensitivity(), 0.1f);
	Read->MouseSensitivity = 100;
	TestEqual(TEXT("Maximum sensitivity"), Read->GetSensitivity(), 3.f);
	IFileManager::Get().Delete(*Path);

	return true;
}

#endif
