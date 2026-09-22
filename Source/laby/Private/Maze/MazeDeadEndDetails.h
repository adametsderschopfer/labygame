#pragma once

#include "CoreMinimal.h"

struct FMazeInterior;
struct FMazeLayout;

namespace MazeDeadEndDetails
{
	void Append(FMazeInterior& Interior,
	            const FMazeLayout& Layout,
	            float Cell,
	            float Thickness,
	            float Height,
	            int32 Seed,
	            FIntRect Cells);
}
