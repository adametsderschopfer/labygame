#pragma once

#include "CoreMinimal.h"
#include "MazeSignal.generated.h"

USTRUCT()
struct FMazeSignalSnapshot
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	FVector Direction = FVector::ForwardVector;

	UPROPERTY()
	uint32 Sequence = 0;
};

struct FMazeSignalView
{
	FVector Location = FVector::ZeroVector;
	float RemainingSeconds = 0.f;
	uint32 Sequence = 0;
};

struct FMazeSignalDefinition
{
	static constexpr float CooldownSeconds = 4.f;
	static constexpr float DisplaySeconds = 3.f;
	static constexpr float AudibleRadius = 8000.f;
	static constexpr float ForwardProbeDistance = 1200.f;
};

struct FMazeSignalFragment;

struct LABY_API FMazeSignalSystem
{
	static void Update(FMazeSignalFragment& Signal, float DeltaSeconds, bool bAuthority);
	static bool Request(FMazeSignalFragment& Signal, const FVector& Location, const FVector& Direction, bool bAllowed);
	static bool Receive(FMazeSignalFragment& Signal, const FMazeSignalSnapshot& Snapshot);
};
