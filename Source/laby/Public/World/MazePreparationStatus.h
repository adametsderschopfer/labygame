#pragma once

#include "CoreMinimal.h"

// Detached observations of engine preparation, never gameplay state or a global percentage.
enum class EMazePreparationStage : uint8
{
	Map,
	Topology,
	Collision,
	Assets,
	Geometry,
	WorldStreaming,
	Shaders,
	Finalizing
};

struct FMazePreparationStatus
{
	EMazePreparationStage Stage = EMazePreparationStage::Map;
	int32 Completed = 0;
	int32 Total = 0;
	int32 PendingPSOs = 0;
};
