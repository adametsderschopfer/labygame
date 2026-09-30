#pragma once
#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "Mass/EntityElementTypes.h"
#include "Mass/EntityHandle.h"
#include "MassEntityTypes.h"
#include "MazeNoise.generated.h"

UENUM()
enum class EMazeNoiseSource : uint8
{
	None,
	Footstep,
	Whistle,
	Headlamp,
	Door
};

UENUM()
enum class EMazeFootstepType : uint8
{
	Walk,
	Run,
	Crouch
};

struct FMazeNoiseDefinition
{
	static constexpr int32 TrailCount = 5;
	static constexpr float TrailSeconds = 3.f;
	static constexpr float DecayPerSecond = 0.6f;
	static constexpr float WhistleStrength = 1.f;
	static constexpr float WalkStrength = 0.28f;
	static constexpr float RunStrength = 0.55f;
	static constexpr float CrouchStrength = 0.1f;
	static constexpr float HeadlampStrength = 0.04f;
	static constexpr float DoorStrength = 0.08f;
	static constexpr float WalkStride = 180.f;
	static constexpr float RunStride = 240.f;
	static constexpr float CrouchStride = 140.f;
	static constexpr float FirstStride = 35.f;
	static constexpr float MinimumSpeed = 15.f;
	static constexpr float MaximumStepDelta = 0.15f;
};

USTRUCT()
struct FMazeNoiseFootprint
{
	GENERATED_BODY()
	UPROPERTY()
	FVector_NetQuantize Location = FVector::ZeroVector;
	UPROPERTY()
	float RemainingSeconds = 0.f;
	UPROPERTY()
	float Strength = 0.f;
};

USTRUCT()
struct FMazeNoiseSnapshot
{
	GENERATED_BODY()
	UPROPERTY()
	float Coefficient = 0.f;
	UPROPERTY()
	EMazeNoiseSource LastSource = EMazeNoiseSource::None;
	UPROPERTY()
	uint32 EventSequence = 0;
	UPROPERTY()
	uint32 StepSequence = 0;
	UPROPERTY()
	EMazeFootstepType StepType = EMazeFootstepType::Walk;
	UPROPERTY()
	uint32 MazeRevision = 0;
	UPROPERTY()
	int32 MazeSeed = 0;
	UPROPERTY()
	TArray<FMazeNoiseFootprint> Footprints;
};

// One authoritative owner of per-player noise, stride cadence and short step history.
USTRUCT()
struct FMazeNoiseFragment : public FMassFragment
{
	GENERATED_BODY()
	UPROPERTY()
	FMazeNoiseSnapshot Value;
	FMassEntityHandle Maze;
	FVector PreviousLocation = FVector::ZeroVector;
	float StrideDistance = 0.f;
	bool bHasPreviousLocation = false;
	bool bHasObservation = false;
	bool bFirstStep = true;
	// Client-only elapsed time for drawing received marks; does not change authoritative coefficient.
	float PresentationElapsed = 0.f;
};

template <> struct TMassFragmentTraits<FMazeNoiseFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
