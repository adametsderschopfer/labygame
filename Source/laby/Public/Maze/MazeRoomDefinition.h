#pragma once

#include "CoreMinimal.h"

// Shared deterministic room topology and physical opening dimensions, in centimeters.
struct FMazeRoomDefinition
{
	static constexpr int32 MinWidth = 1;
	static constexpr int32 MaxWidth = 4;
	static constexpr int32 MinLength = 1;
	static constexpr int32 MaxLength = 7;
	struct FSizeRange
	{
		int32 MinWidth, MaxWidth, MinLength, MaxLength, Weight;
	};
	// Compact utility rooms are common; the previous largest rooms remain possible.
	inline static constexpr FSizeRange SizeRanges[] = {
	    {MinWidth, 2, MinLength, 2, 50}, {2, 3, 3, 5, 35}, {3, MaxWidth, 5, MaxLength, 15}};
	// One room per spatial sector instead of unconstrained scatter across the map.
	static constexpr int32 SectorSide = 12;
	// Additional compact rooms selected from actual bends and nearby dead-end branches.
	static constexpr int32 BendSectorSide = 16;
	static constexpr float BendThroughFraction = 0.5f;
	static constexpr int32 PlacementAttemptsPerRoom = 20;
	static constexpr float ThroughFraction = 0.45f;
	static constexpr float DoorWidth = 120.f;
	static constexpr float DoorHeight = 220.f;
	static float OpeningWidth(float Span)
	{
		return FMath::Min(DoorWidth, Span * 0.8f);
	}

	static float OpeningHeight(float Height)
	{
		return FMath::Min(DoorHeight, Height * 0.85f);
	}
};
