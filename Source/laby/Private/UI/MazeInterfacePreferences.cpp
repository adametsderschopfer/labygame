#include "UI/MazeInterfacePreferences.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
	const TCHAR* Section = TEXT("Maze.InterfacePreferences");
}

FMazeInterfacePreferences FMazeInterfacePreferences::Read()
{
	FMazeInterfacePreferences Value;

	GConfig->GetFloat(Section, TEXT("FieldOfView"), Value.FieldOfView, GGameUserSettingsIni);
	Value.FieldOfView = FMath::IsFinite(Value.FieldOfView) ? FMath::Clamp(Value.FieldOfView, 70.f, 110.f) : 95.f;
	GConfig->GetBool(Section, TEXT("InvertMouseY"), Value.bInvertMouseY, GGameUserSettingsIni);
	GConfig->GetBool(Section, TEXT("ShowCompass"), Value.bShowCompass, GGameUserSettingsIni);
	GConfig->GetBool(Section, TEXT("ShowCrosshair"), Value.bShowCrosshair, GGameUserSettingsIni);
	GConfig->GetBool(Section, TEXT("CompactHUD"), Value.bCompactHUD, GGameUserSettingsIni);
	GConfig->GetBool(Section, TEXT("CameraMotion"), Value.bCameraMotion, GGameUserSettingsIni);

	return Value;
}

void FMazeInterfacePreferences::Save() const
{
	GConfig->SetFloat(Section,
	                  TEXT("FieldOfView"),
	                  FMath::IsFinite(FieldOfView) ? FMath::Clamp(FieldOfView, 70.f, 110.f) : 95.f,
	                  GGameUserSettingsIni);
	GConfig->SetBool(Section, TEXT("InvertMouseY"), bInvertMouseY, GGameUserSettingsIni);
	GConfig->SetBool(Section, TEXT("ShowCompass"), bShowCompass, GGameUserSettingsIni);
	GConfig->SetBool(Section, TEXT("ShowCrosshair"), bShowCrosshair, GGameUserSettingsIni);
	GConfig->SetBool(Section, TEXT("CompactHUD"), bCompactHUD, GGameUserSettingsIni);
	GConfig->SetBool(Section, TEXT("CameraMotion"), bCameraMotion, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}
