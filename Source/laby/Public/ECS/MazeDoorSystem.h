#pragma once

#include "CoreMinimal.h"
#include "ECS/MazeDoorDefinition.h"
#include "ECS/MazeECSFragments.h"

struct FMazeDoorView
{
	int32 Index = INDEX_NONE;
	FVector Center = FVector::ZeroVector;
	FVector SlideAxis = FVector::RightVector;
	FVector Normal = FVector::ForwardVector;
	float OpenAmount = 0.f;
	bool bWantsOpen = false;
};

// Stateless proximity, hysteresis and motion rules for automatic room doors.
struct FMazeDoorSystem
{
	static void Update(FMazeDoorFragment& Door,
	                   float DeltaSeconds,
	                   TConstArrayView<FVector> PlayerLocations,
	                   bool bAuthority);
};
