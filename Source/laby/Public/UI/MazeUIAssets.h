#pragma once
#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"

class UMazeMenuWidget;
class UMazeHUDWidget;
class UMazeExplorationMapWidget;

// Fixed presentation catalog. These packages are also registered in the location manifest.
// No gameplay state or UObject ownership is stored here.
namespace MazeUIAssets
{
	LABY_API TSoftClassPtr<UMazeMenuWidget> Menu(bool Settings, bool Pause);
	LABY_API TSoftClassPtr<UMazeHUDWidget> HUD();
	LABY_API TSoftClassPtr<UMazeExplorationMapWidget> Map();
}
