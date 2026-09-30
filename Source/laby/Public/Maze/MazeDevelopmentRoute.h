#pragma once
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
struct FMazeLayout;

// Pure BFS over canonical topology; includes manually openable doors and crouch links.
TArray<int32> BuildMazeDevelopmentRoute(const FMazeLayout& Layout, int32 Start);
#endif
