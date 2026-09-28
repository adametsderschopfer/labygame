#include "ECS/MazeItemSystem.h"

void FMazeItemSystem::InitializeLoadout(FMazeItemsFragment& Items)
{
	Items.Value = FMazeItemsSnapshot();
}

bool FMazeItemSystem::HasHeadlamp(const FMazeItemsFragment& Items)
{
	return Items.Value.Items.ContainsByPredicate(
	    [](const FMazeItemInstance& Item)
	    {
		    return Item.Id != INDEX_NONE && Item.Kind == EMazeItemKind::Headlamp &&
		           Item.Slot == EMazeEquipmentSlot::Head;
	    });
}

FMazeHeadlampDefinition FMazeItemSystem::HeadlampDefinition()
{
	// Do not retain pre-patch tuning in a function-local static during Live Coding.
	return FMazeHeadlampDefinition();
}

bool FMazeItemSystem::IsHeadlampEnabled(const FMazeItemsFragment& Items)
{
	return Items.Value.Items.ContainsByPredicate(
	    [](const FMazeItemInstance& Item)
	    {
		    return Item.Id != INDEX_NONE && Item.Kind == EMazeItemKind::Headlamp &&
		           Item.Slot == EMazeEquipmentSlot::Head && Item.bEnabled;
	    });
}

bool FMazeItemSystem::ToggleHeadlamp(FMazeItemsFragment& Items, bool bCanAct)
{
	if (!bCanAct)
		return false;

	const int32 Slot = Items.Value.SelectedSlot;

	if (Items.Value.Items.IsValidIndex(Slot))
	{
		auto& Item = Items.Value.Items[Slot];

		if (Item.Id != INDEX_NONE && Item.Kind == EMazeItemKind::Headlamp && Item.Slot == EMazeEquipmentSlot::Head)
		{
			Item.bEnabled = !Item.bEnabled;

			return true;
		}
	}

	return false;
}

bool FMazeItemSystem::SelectSlot(FMazeItemsFragment& Items, int32 Slot, bool bCanAct)
{
	if (!bCanAct || Slot < 0 || Slot >= FMazeItemDefinition::InventoryCapacity || Items.Value.SelectedSlot == Slot)
		return false;

	Items.Value.SelectedSlot = static_cast<uint8>(Slot);

	return true;
}

bool FMazeItemSystem::CanFocus(const FMazeWorldItemFragment& WorldItem,
                               const FVector& EyeLocation,
                               const FVector& AimDirection)
{
	const FVector ToItem = WorldItem.Location - EyeLocation;

	return WorldItem.bAvailable && ToItem.SizeSquared() <= FMath::Square(FMazeItemDefinition::FocusRange) &&
	       FVector::DotProduct(ToItem.GetSafeNormal(), AimDirection.GetSafeNormal()) >=
	           FMazeItemDefinition::MinimumAimDot;
}

bool FMazeItemSystem::Pickup(FMazeWorldItemFragment& WorldItem,
                             FMazeItemsFragment& Items,
                             const FVector& EyeLocation,
                             const FVector& AimDirection,
                             bool bCanAct)
{
	if (!bCanAct || !CanFocus(WorldItem, EyeLocation, AimDirection) || WorldItem.Kind != EMazeItemKind::Headlamp ||
	    HasHeadlamp(Items) || Items.Value.Items.Num() >= FMazeItemDefinition::InventoryCapacity)
		return false;

	FMazeItemInstance Lamp;

	Lamp.Id = Items.Value.NextInstanceId++;
	Lamp.Kind = WorldItem.Kind;
	Lamp.Slot = EMazeEquipmentSlot::Head;
	Lamp.bEnabled = false;
	Items.Value.Items.Add(Lamp);
	WorldItem.bAvailable = false;

	return true;
}
