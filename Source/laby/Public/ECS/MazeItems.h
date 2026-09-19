#pragma once
#include "CoreMinimal.h"
#include "Mass/EntityElementTypes.h"
#include "MassEntityTypes.h"
#include "MazeItems.generated.h"

UENUM()
enum class EMazeItemKind : uint8
{
	None,
	Headlamp
};

UENUM()
enum class EMazeEquipmentSlot : uint8
{
	None,
	Head
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
};

USTRUCT()
struct FMazeItemsFragment : public FMassFragment
{
	GENERATED_BODY()
	UPROPERTY()
	FMazeItemsSnapshot Value;
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
