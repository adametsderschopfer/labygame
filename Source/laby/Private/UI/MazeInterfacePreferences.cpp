#include "UI/MazeInterfacePreferences.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
	const TCHAR* InterfacePreferencesSection = TEXT("Maze.InterfacePreferences");
}

FMazeInterfacePreferences FMazeInterfacePreferences::Read()
{
	FMazeInterfacePreferences Value;

	GConfig->GetFloat(InterfacePreferencesSection, TEXT("FieldOfView"), Value.FieldOfView, GGameUserSettingsIni);
	Value.FieldOfView = FMath::IsFinite(Value.FieldOfView) ? FMath::Clamp(Value.FieldOfView, 70.f, 110.f) : 95.f;
	GConfig->GetBool(InterfacePreferencesSection, TEXT("InvertMouseY"), Value.bInvertMouseY, GGameUserSettingsIni);
	GConfig->GetBool(InterfacePreferencesSection, TEXT("ShowCompass"), Value.bShowCompass, GGameUserSettingsIni);
	GConfig->GetBool(InterfacePreferencesSection, TEXT("ShowCrosshair"), Value.bShowCrosshair, GGameUserSettingsIni);
	GConfig->GetBool(InterfacePreferencesSection, TEXT("CompactHUD"), Value.bCompactHUD, GGameUserSettingsIni);
	GConfig->GetBool(InterfacePreferencesSection, TEXT("CameraMotion"), Value.bCameraMotion, GGameUserSettingsIni);

	return Value;
}

void FMazeInterfacePreferences::Save() const
{
	GConfig->SetFloat(InterfacePreferencesSection,
	                  TEXT("FieldOfView"),
	                  FMath::IsFinite(FieldOfView) ? FMath::Clamp(FieldOfView, 70.f, 110.f) : 95.f,
	                  GGameUserSettingsIni);
	GConfig->SetBool(InterfacePreferencesSection, TEXT("InvertMouseY"), bInvertMouseY, GGameUserSettingsIni);
	GConfig->SetBool(InterfacePreferencesSection, TEXT("ShowCompass"), bShowCompass, GGameUserSettingsIni);
	GConfig->SetBool(InterfacePreferencesSection, TEXT("ShowCrosshair"), bShowCrosshair, GGameUserSettingsIni);
	GConfig->SetBool(InterfacePreferencesSection, TEXT("CompactHUD"), bCompactHUD, GGameUserSettingsIni);
	GConfig->SetBool(InterfacePreferencesSection, TEXT("CameraMotion"), bCameraMotion, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}
