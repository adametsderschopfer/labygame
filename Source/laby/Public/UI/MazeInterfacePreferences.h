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

// Device-local audio levels for the menu ambience and its UI feedback sounds.
struct LABY_API FMazeMenuAudioPreferences
{
	float MusicVolume = 1.f;
	float InterfaceVolume = 1.f;

	static FMazeMenuAudioPreferences Read();
	void Save() const;
};

// Device-local language choice; the editor needs explicit game-localization preview.
struct LABY_API FMazeLanguagePreference
{
	static FString Current();
	static bool Set(const FString& Culture, bool bSave);
	static void ApplySaved();
};
