#pragma once
#include "CoreMinimal.h"

// Stable source identities shared by generated assets and existing runtime widgets.
namespace MazeText
{
	inline const TMap<FName, FText>& WidgetLabels()
	{
		static const TMap<FName, FText> Labels = {
		    {TEXT("Title"), NSLOCTEXT("Maze.Widgets", "Title", "L A B Y")},
		    {TEXT("SensitivityLabel"), NSLOCTEXT("Maze.Widgets", "SensitivityLabel", "Mouse sensitivity")},
		    {TEXT("SensitivityText"), NSLOCTEXT("Maze.Widgets", "SensitivityText", "1.00 x")},
		    {TEXT("SettingsHint"), NSLOCTEXT("Maze.Widgets", "SettingsHint", "0.10 - 3.00 - Saved automatically")},
		    {TEXT("ResetButtonLabel"), NSLOCTEXT("Maze.Widgets", "ResetButtonLabel", "Reset to defaults")},
		    {TEXT("BackButtonLabel"), NSLOCTEXT("Maze.Widgets", "BackButtonLabel", "Back")},
		    {TEXT("ResumeButtonLabel"), NSLOCTEXT("Maze.Widgets", "ResumeButtonLabel", "Resume")},
		    {TEXT("NewGameButtonLabel"), NSLOCTEXT("Maze.Widgets", "NewGameButtonLabel", "Create room")},
		    {TEXT("SettingsButtonLabel"), NSLOCTEXT("Maze.Widgets", "SettingsButtonLabel", "Settings")},
		    {TEXT("QuitButtonLabel"), NSLOCTEXT("Maze.Widgets", "QuitButtonLabel", "Quit to desktop")},
		    {TEXT("VersionText"), NSLOCTEXT("Maze.Widgets", "VersionText", "ALPHA 0.0.0.1")},
		    {TEXT("HealthText"), NSLOCTEXT("Maze.Widgets", "HealthText", "HEALTH  100 / 100")},
		    {TEXT("StaminaText"), NSLOCTEXT("Maze.Widgets", "StaminaText", "STAMINA  100 / 100")},
		    {TEXT("DeveloperHint"), NSLOCTEXT("Maze.Widgets", "DeveloperHint", "DEV: F7  Toggle development map")},
		    {TEXT("SessionText"), NSLOCTEXT("Maze.Widgets", "SessionText", "SESSION 0 - 80 x 80 - START A")},
		    {TEXT("MapTitle"), NSLOCTEXT("Maze.Widgets", "MapTitle", "DEVELOPMENT MAP")},
		    {TEXT("MapLegend"), NSLOCTEXT("Maze.Widgets", "MapLegend", "YOU        A        EXIT")},
		    {TEXT("DeathText"), NSLOCTEXT("Maze.Widgets", "DeathText", "YOU DIED")},
		    {TEXT("ExitText"), NSLOCTEXT("Maze.Widgets", "ExitText", "EXIT REACHED")},
		    {TEXT("DeathHint"), NSLOCTEXT("Maze.Widgets", "DeathHint", "Press Escape to open the game menu")},
		    {TEXT("ExitHint"), NSLOCTEXT("Maze.Widgets", "ExitHint", "Press Escape to open the game menu")},
		};

		return Labels;
	}

	inline FText Widget(FName Name)
	{
		const FText* Label = WidgetLabels().Find(Name);

		return Label ? *Label : FText::GetEmpty();
	}

	inline FText Subtitle(bool bSettings, bool bPause)
	{
		return bSettings ? NSLOCTEXT("Maze.Widgets", "SettingsSubtitle", "Settings / Controls")
		       : bPause  ? NSLOCTEXT("Maze.Widgets", "PauseSubtitle", "Game menu")
		                 : NSLOCTEXT("Maze.Widgets", "MainSubtitle", "One maze. One exit.");
	}
}
