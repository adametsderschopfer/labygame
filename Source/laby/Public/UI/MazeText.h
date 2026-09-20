#pragma once
#include "CoreMinimal.h"

// Stable source identities shared by generated assets and existing runtime widgets.
namespace MazeText
{
	inline const TMap<FName, FText>& WidgetLabels()
	{
		static const TMap<FName, FText> Labels = {
		    {TEXT("Title"), NSLOCTEXT("Maze.Widgets", "Title", "L A B Y")},
		    {TEXT("SensitivityLabel"), NSLOCTEXT("Maze.Widgets", "SensitivityLabel", "Чувствительность мыши")},
		    {TEXT("SensitivityText"), NSLOCTEXT("Maze.Widgets", "SensitivityText", "1.00 x")},
		    {TEXT("SettingsHint"),
		     NSLOCTEXT("Maze.Widgets", "SettingsHint", "0.10 — 3.00 · Сохраняется кнопкой «Применить»")},
		    {TEXT("ResetButtonLabel"), NSLOCTEXT("Maze.Widgets", "ResetButtonLabel", "ИСХОДНЫЕ")},
		    {TEXT("BackButtonLabel"), NSLOCTEXT("Maze.Widgets", "BackButtonLabel", "НАЗАД")},
		    {TEXT("ResumeButtonLabel"), NSLOCTEXT("Maze.Widgets", "ResumeButtonLabel", "ПРОДОЛЖИТЬ")},
		    {TEXT("NewGameButtonLabel"), NSLOCTEXT("Maze.Widgets", "NewGameButtonLabel", "НАЧАТЬ ПУТЬ")},
		    {TEXT("SettingsButtonLabel"), NSLOCTEXT("Maze.Widgets", "SettingsButtonLabel", "НАСТРОЙКИ")},
		    {TEXT("QuitButtonLabel"), NSLOCTEXT("Maze.Widgets", "QuitButtonLabel", "ВЫЙТИ ИЗ ИГРЫ")},
		    {TEXT("VersionText"), NSLOCTEXT("Maze.Widgets", "VersionText", "ALPHA 0.0.0.1")},
		    {TEXT("HealthText"), NSLOCTEXT("Maze.Widgets", "HealthText", "СОСТОЯНИЕ   100")},
		    {TEXT("StaminaText"), NSLOCTEXT("Maze.Widgets", "StaminaText", "ВЫНОСЛИВОСТЬ   100")},
		    {TEXT("DeveloperHint"), NSLOCTEXT("Maze.Widgets", "DeveloperHint", "DEV: F7  Toggle development map")},
		    {TEXT("SessionText"), NSLOCTEXT("Maze.Widgets", "SessionText", "SESSION 0 - 80 x 80 - START A")},
		    {TEXT("MapTitle"), NSLOCTEXT("Maze.Widgets", "MapTitle", "DEVELOPMENT MAP")},
		    {TEXT("MapLegend"), NSLOCTEXT("Maze.Widgets", "MapLegend", "YOU        A        EXIT")},
		    {TEXT("DeathText"), NSLOCTEXT("Maze.Widgets", "DeathText", "СВЯЗЬ ПОТЕРЯНА")},
		    {TEXT("ExitText"), NSLOCTEXT("Maze.Widgets", "ExitText", "ВЫХОД НАЙДЕН")},
		    {TEXT("DeathHint"), NSLOCTEXT("Maze.Widgets", "DeathHint", "ESC   /   ОТКРЫТЬ МЕНЮ")},
		    {TEXT("ExitHint"), NSLOCTEXT("Maze.Widgets", "ExitHint", "ESC   /   ОТКРЫТЬ МЕНЮ")},
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
		return bSettings ? NSLOCTEXT("Maze.Widgets", "SettingsSubtitle", "ВИДЕО / УПРАВЛЕНИЕ / ИГРА")
		       : bPause  ? NSLOCTEXT("Maze.Widgets", "PauseSubtitle", "ПАУЗА")
		                 : NSLOCTEXT("Maze.Widgets", "MainSubtitle", "ЛАБИРИНТ / ИССЛЕДОВАНИЕ");
	}
}
