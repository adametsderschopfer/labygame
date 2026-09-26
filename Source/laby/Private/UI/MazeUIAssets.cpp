#include "UI/MazeUIAssets.h"
#include "UI/MazeWidgets.h"
#include "UI/MazeExplorationMapWidget.h"

TSoftClassPtr<UMazeMenuWidget> MazeUIAssets::Menu(bool Settings, bool Pause)
{
	return TSoftClassPtr<UMazeMenuWidget>(
	    FSoftObjectPath(Settings ? TEXT("/Game/UI/Ward/WBP_WardSettings.WBP_WardSettings_C")
	                    : Pause  ? TEXT("/Game/UI/Ward/WBP_WardPauseMenu.WBP_WardPauseMenu_C")
	                             : TEXT("/Game/UI/Ward/WBP_WardMainMenu.WBP_WardMainMenu_C")));
}

TSoftClassPtr<UMazeHUDWidget> MazeUIAssets::HUD()
{
	return TSoftClassPtr<UMazeHUDWidget>(FSoftObjectPath(TEXT("/Game/UI/Ward/WBP_WardHUD.WBP_WardHUD_C")));
}

TSoftClassPtr<UMazeExplorationMapWidget> MazeUIAssets::Map()
{
	return TSoftClassPtr<UMazeExplorationMapWidget>(FSoftObjectPath(TEXT("/Game/UI/Ward/WBP_WardMap.WBP_WardMap_C")));
}
