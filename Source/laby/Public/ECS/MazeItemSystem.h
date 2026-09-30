#pragma once
#include "ECS/MazeItems.h"

struct LABY_API FMazeItemSystem
{
	static void InitializeLoadout(FMazeItemsFragment& Items);
	static bool HasHeadlamp(const FMazeItemsFragment& Items);
	static bool IsCardSelected(const FMazeItemsSnapshot& Items);
	static bool IsHeadlampEnabled(const FMazeItemsFragment& Items);
	static bool ToggleHeadlamp(FMazeItemsFragment& Items, bool bCanAct);
	static bool SelectSlot(FMazeItemsFragment& Items, int32 Slot, bool bCanAct);
	static int32 NextSlot(int32 Slot, int32 Step);
	static bool CanFocus(const FMazeWorldItemFragment& WorldItem,
	                     const FVector& EyeLocation,
	                     const FVector& AimDirection);
	static bool CanFocus(const FMazeWorldItemView& WorldItem, const FVector& EyeLocation, const FVector& AimDirection);
	static bool CanFocusNearby(const FMazeWorldItemView& WorldItem,
	                           const FVector& EyeLocation,
	                           const FVector& AimDirection);
	static bool Pickup(FMazeWorldItemFragment& WorldItem,
	                   FMazeItemsFragment& Items,
	                   const FVector& EyeLocation,
	                   const FVector& AimDirection,
	                   bool bCanAct);
	static bool Drop(FMazeItemsFragment& Items,
	                 FMazeWorldItemFragment& WorldItem,
	                 const FVector& Location,
	                 bool bCanAct);
	static FMazeHeadlampDefinition HeadlampDefinition();
};
