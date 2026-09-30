#pragma once
#include "CoreMinimal.h"
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
class SWidget;
class AMazeCharacter;
class UGameViewportClient;

FText MazeDevelopmentText(const TCHAR* En, const TCHAR* Ru, const TCHAR* Es);
bool ExecuteMazeDevelopmentTeleport(AMazeCharacter& Character, bool bExit);

// Only local widget preferences/caches and original engine flags; gameplay toggles are read from ECS.
struct FMazeDevelopmentPresentation
{
	TSharedPtr<SWidget> Overlay;
	TWeakObjectPtr<UGameViewportClient> Viewport;
	FText Position;
	FText Resources;
	FText Seed;
	FText Status;
	float RefreshWait = 0;
	bool bShowPosition = false;
	bool bShowResources = false;
	bool bSavedCollision = false;
	bool bOriginalCollision = false;
	bool bOriginalVolumes = false;
	bool bWaitingTeleport = false;
};
#endif
