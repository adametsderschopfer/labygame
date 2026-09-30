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
	int8 SwingSign = 1;
	bool bRequiresCard = false;
	bool bUnlocked = false;
};

// Stateless interaction and motion rules for room doors.
struct FMazeDoorSystem
{
	static FVector HandleLocation(const FMazeDoorView& Door, float OpeningWidth);
	static FVector ReaderLocation(const FMazeDoorView& Door, float OpeningWidth, float WallThickness, float Side);
	static FVector FocusLocation(const FMazeDoorView& Door,
	                             const FVector& Eye,
	                             const FVector& Aim,
	                             float OpeningWidth,
	                             float WallThickness = 25.f);
	static bool IsCardDoor(int32 Seed, int32 Index);
	static bool CanUse(const FMazeDoorView& Door, bool bCardSelected);
	static bool TryToggle(FMazeDoorFragment& Door, const FVector& PlayerLocation, bool bCardSelected);
	static bool CanFocus(const FMazeDoorView& Door,
	                     const FVector& Eye,
	                     const FVector& Aim,
	                     float OpeningWidth,
	                     float WallThickness = 25.f);
	static bool CanInteract(const FMazeDoorFragment& Door,
	                        const FVector& Eye,
	                        const FVector& Aim,
	                        float OpeningWidth,
	                        float WallThickness = 25.f);
	static void Toggle(FMazeDoorFragment& Door, const FVector& PlayerLocation);
	static float NextOpenAmount(const FMazeDoorView& Door, float DeltaSeconds);
	static void Advance(FMazeDoorFragment& Door, float DeltaSeconds, float MaxSafeAmount);
};
