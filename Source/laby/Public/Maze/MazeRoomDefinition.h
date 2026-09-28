#pragma once

#include "CoreMinimal.h"

enum class EMazeRoomType : uint8
{
	Empty,
	ShallowFlooded,
	Pool
};

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
	// Multiple separated rooms per sector; keep sectors large enough for 4x7 rooms.
	static constexpr int32 SectorSide = 12;
	static constexpr int32 RoomsPerSector = 3;
	// Additional compact rooms selected from corridor dead ends.
	static constexpr int32 DeadEndSectorSide = 16;
	static constexpr int32 PlacementAttemptsPerRoom = 20;
	static constexpr float ThroughFraction = 0.98f;
	static constexpr float DoorWidth = 120.f;
	static constexpr float DoorHeight = 220.f;
	static constexpr int32 EmptyWeight = 55;
	static constexpr int32 ShallowFloodedWeight = 30;
	static constexpr int32 PoolWeight = 15;
	static constexpr int32 MinPoolWidthCells = 2;
	static constexpr int32 MinPoolLengthCells = 2;
	static constexpr float ShallowDepth = 45.f;
	static constexpr float ShallowWaterDepth = 40.f;
	static constexpr float ShallowCeilingHeight = 480.f;
	static constexpr float PoolDepth = 180.f;
	static constexpr float PoolWaterSurface = -20.f;
	static constexpr float PoolCeilingHeight = 600.f;
	static constexpr float StairRise = 15.f;
	static constexpr float StairTread = 42.f;
	static constexpr float BridgeWidth = 90.f;
	static constexpr float BridgeThickness = 15.f;
	static constexpr float WaterThickness = 2.f;
	static float OpeningWidth(float Span)
	{
		return FMath::Min(DoorWidth, Span * 0.8f);
	}

	static float OpeningHeight(float Height)
	{
		return FMath::Min(DoorHeight, Height * 0.85f);
	}

	static float FloorHeight(EMazeRoomType Type)
	{
		return Type == EMazeRoomType::ShallowFlooded ? -ShallowDepth : Type == EMazeRoomType::Pool ? -PoolDepth : 0.f;
	}

	static float CeilingHeight(EMazeRoomType Type, float DefaultHeight)
	{
		return Type == EMazeRoomType::ShallowFlooded ? ShallowCeilingHeight
		       : Type == EMazeRoomType::Pool         ? PoolCeilingHeight
		                                             : DefaultHeight;
	}

	static float WaterHeight(EMazeRoomType Type)
	{
		return Type == EMazeRoomType::ShallowFlooded ? -ShallowDepth + ShallowWaterDepth
		       : Type == EMazeRoomType::Pool         ? PoolWaterSurface
		                                             : 0.f;
	}
};
