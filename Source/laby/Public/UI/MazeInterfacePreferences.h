#pragma once

#include "CoreMinimal.h"

// Device-local presentation preferences. No gameplay state or replication mirror.
// Mouse sensitivity retains the existing UMazePreferences owner.
struct LABY_API FMazeInterfacePreferences
{
	float FieldOfView = 95.f;
	bool bInvertMouseY = false;
	bool bShowCompass = true;
	bool bShowCrosshair = true;
	bool bCompactHUD = false;
	bool bCameraMotion = true;

	static FMazeInterfacePreferences Read();
	void Save() const;
};
