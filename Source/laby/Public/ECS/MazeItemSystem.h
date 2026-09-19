#pragma once
#include "ECS/MazeItems.h"

struct LABY_API FMazeItemSystem
{
	static void InitializeLoadout(FMazeItemsFragment& Items);
	static bool HasHeadlamp(const FMazeItemsFragment& Items);
	static FMazeHeadlampDefinition HeadlampDefinition();
};
