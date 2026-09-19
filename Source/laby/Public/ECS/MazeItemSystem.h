#pragma once
#include "ECS/MazeItems.h"

struct LABY_API FMazeItemSystem
{
	static void InitializeLoadout(FMazeItemsFragment& Items);
	static bool HasHeadlamp(const FMazeItemsFragment& Items);
	static bool IsHeadlampEnabled(const FMazeItemsFragment& Items);
	static bool ToggleHeadlamp(FMazeItemsFragment& Items, bool bCanAct);
	static FMazeHeadlampDefinition HeadlampDefinition();
};
