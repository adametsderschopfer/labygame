#pragma once

#include "CoreMinimal.h"

struct FMazeLayout;

namespace MazeRoutes
{
	// Builds only unreserved corridor cells. Returns floor to protect under scenic runs.
	TArray<int32> Build(FMazeLayout& Layout, const TArray<bool>& Reserved, int32 Start, FRandomStream& Random);
}
