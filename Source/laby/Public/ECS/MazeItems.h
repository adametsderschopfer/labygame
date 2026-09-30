#pragma once
#include "CoreMinimal.h"
#include "Mass/EntityElementTypes.h"
#include "Mass/EntityHandle.h"
#include "MassEntityTypes.h"
#include "MazeItems.generated.h"

UENUM()
enum class EMazeItemKind : uint8
{
	None,
	Headlamp,
	AccessCard
};

UENUM()
enum class EMazeEquipmentSlot : uint8
{
	None,
	Head,
	Hand
};

// Instance identifiers are scoped to the owning player, never world entity handles.
USTRUCT()
struct FMazeItemInstance
{
	GENERATED_BODY()
	UPROPERTY()
	int32 Id = INDEX_NONE;
	UPROPERTY()
	EMazeItemKind Kind = EMazeItemKind::None;
	UPROPERTY()
	EMazeEquipmentSlot Slot = EMazeEquipmentSlot::None;
	UPROPERTY()
	bool bEnabled = true;
};

// Value snapshot for transport and read-only consumers. ECS owns the authoritative copy.
USTRUCT()
struct FMazeItemsSnapshot
{
	GENERATED_BODY()
	UPROPERTY()
	TArray<FMazeItemInstance> Items;
	UPROPERTY()
	int32 NextInstanceId = 1;
	UPROPERTY()
	uint8 SelectedSlot = 0;
};

USTRUCT()
struct FMazeItemsFragment : public FMassFragment
{
	GENERATED_BODY()
	UPROPERTY()
	FMazeItemsSnapshot Value;
};

// Stable pickups per generated maze. World components only present this state.
USTRUCT()
struct FMazeWorldItemFragment : public FMassFragment
{
	GENERATED_BODY()
	UPROPERTY()
	FMassEntityHandle Maze;
	UPROPERTY()
	uint32 MazeRevision = 0;
	UPROPERTY()
	int32 Id = 1;
	UPROPERTY()
	EMazeItemKind Kind = EMazeItemKind::Headlamp;
	UPROPERTY()
	FVector Location = FVector::ZeroVector;
	UPROPERTY()
	bool bAvailable = true;
};

struct FMazeWorldItemView
{
	int32 Id = INDEX_NONE;
	EMazeItemKind Kind = EMazeItemKind::None;
	FVector Location = FVector::ZeroVector;
	bool bAvailable = false;
};

struct FMazeItemDefinition
{
	static constexpr float FocusRange = 250.f;
	static constexpr float FocusRadiusCm = 18.f;
	static constexpr float MinimumAimDot = 0.88f;
	static constexpr float EyePoseTolerance = 130.f;
	static constexpr float DropForwardCm = 95.f;
	static constexpr float DropSweepRadiusCm = 12.f;
	static constexpr float DropWallClearanceCm = 22.f;
	static constexpr float DropHeightCm = 15.f;
	static constexpr float DropRangeCm = 200.f;
	static constexpr int32 InventoryCapacity = 4;
	static constexpr int32 HeadlampWorldId = 1;
	static constexpr int32 CardWorldId = 2;
	static FVector StartOffset()
	{
		return FVector(-95.f, -20.f, -78.f);
	}
};

template <> struct TMassFragmentTraits<FMazeItemsFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

// One definition of the lamp's presentation parameters, consumed by the engine adapter.
struct FMazeHeadlampDefinition
{
	FVector HeadOffset = FVector(8.f, 0.f, 96.f);
	FLinearColor Color = FLinearColor(1.f, 0.94f, 0.82f);
	float IntensityLumens = 120.f;
	float Range = 2400.f;
	float InnerConeDegrees = 8.f;
	float OuterConeDegrees = 42.f;
};
