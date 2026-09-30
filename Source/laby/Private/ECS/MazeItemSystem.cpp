#include "ECS/MazeItemSystem.h"

void FMazeItemSystem::InitializeLoadout(FMazeItemsFragment& Items)
{
	Items.Value = FMazeItemsSnapshot();
	Items.Value.Items.SetNum(FMazeItemDefinition::InventoryCapacity);
}

bool FMazeItemSystem::IsCardSelected(const FMazeItemsSnapshot& Items)
{
	return Items.Items.IsValidIndex(Items.SelectedSlot) && Items.Items[Items.SelectedSlot].Id != INDEX_NONE &&
	       Items.Items[Items.SelectedSlot].Kind == EMazeItemKind::AccessCard &&
	       Items.Items[Items.SelectedSlot].Slot == EMazeEquipmentSlot::Hand;
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

int32 FMazeItemSystem::NextSlot(int32 Slot, int32 Step)
{
	return (Slot + Step + FMazeItemDefinition::InventoryCapacity) % FMazeItemDefinition::InventoryCapacity;
}

bool FMazeItemSystem::CanFocus(const FMazeWorldItemFragment& WorldItem,
                               const FVector& EyeLocation,
                               const FVector& AimDirection)
{
	return CanFocus(FMazeWorldItemView{WorldItem.Id, WorldItem.Kind, WorldItem.Location, WorldItem.bAvailable},
	                EyeLocation,
	                AimDirection);
}

bool FMazeItemSystem::CanFocus(const FMazeWorldItemView& WorldItem,
                               const FVector& EyeLocation,
                               const FVector& AimDirection)
{
	if (!WorldItem.bAvailable || WorldItem.Location.ContainsNaN() || EyeLocation.ContainsNaN() ||
	    AimDirection.ContainsNaN())
		return false;

	const FVector ToItem = WorldItem.Location - EyeLocation;
	const FVector Aim = AimDirection.GetSafeNormal();

	return ToItem.SizeSquared() <= FMath::Square(FMazeItemDefinition::FocusRange) &&
	       FVector::DotProduct(ToItem.GetSafeNormal(), Aim) >= FMazeItemDefinition::MinimumAimDot;
}

bool FMazeItemSystem::CanFocusNearby(const FMazeWorldItemView& WorldItem,
                                     const FVector& EyeLocation,
                                     const FVector& AimDirection)
{
	if (!CanFocus(WorldItem, EyeLocation, AimDirection))
		return false;

	const FVector ToItem = WorldItem.Location - EyeLocation;
	const FVector Aim = AimDirection.GetSafeNormal();
	const double AlongAim = FVector::DotProduct(ToItem, Aim);
	const FVector AimOffset = ToItem - Aim * AlongAim;

	return AlongAim > 0. && AimOffset.SizeSquared() <= FMath::Square(FMazeItemDefinition::FocusRadiusCm);
}

bool FMazeItemSystem::Pickup(FMazeWorldItemFragment& WorldItem,
                             FMazeItemsFragment& Items,
                             const FVector& EyeLocation,
                             const FVector& AimDirection,
                             bool bCanAct)
{
	if (!bCanAct || !CanFocus(WorldItem, EyeLocation, AimDirection) ||
	    (WorldItem.Kind != EMazeItemKind::Headlamp && WorldItem.Kind != EMazeItemKind::AccessCard) ||
	    (WorldItem.Kind == EMazeItemKind::Headlamp && HasHeadlamp(Items)))
		return false;

	Items.Value.Items.SetNum(FMazeItemDefinition::InventoryCapacity);

	int32 TargetSlot = Items.Value.SelectedSlot;

	if (!Items.Value.Items.IsValidIndex(TargetSlot))
		return false;

	if (Items.Value.Items[TargetSlot].Id != INDEX_NONE)
		TargetSlot = Items.Value.Items.IndexOfByPredicate(
		    [](const FMazeItemInstance& Item)
		    {
			    return Item.Id == INDEX_NONE;
		    });

	if (TargetSlot == INDEX_NONE)
		return false;

	FMazeItemInstance Lamp;

	Lamp.Id = Items.Value.NextInstanceId++;
	Lamp.Kind = WorldItem.Kind;
	Lamp.Slot = WorldItem.Kind == EMazeItemKind::Headlamp ? EMazeEquipmentSlot::Head : EMazeEquipmentSlot::Hand;
	Lamp.bEnabled = false;
	Items.Value.Items[TargetSlot] = Lamp;
	WorldItem.bAvailable = false;

	return true;
}

bool FMazeItemSystem::Drop(FMazeItemsFragment& Items,
                           FMazeWorldItemFragment& WorldItem,
                           const FVector& Location,
                           bool bCanAct)
{
	const int32 Slot = Items.Value.SelectedSlot;

	if (!bCanAct || WorldItem.bAvailable || Location.ContainsNaN() || !Items.Value.Items.IsValidIndex(Slot))
		return false;

	const FMazeItemInstance& Item = Items.Value.Items[Slot];

	if (Item.Id == INDEX_NONE || Item.Kind != WorldItem.Kind ||
	    (Item.Kind != EMazeItemKind::Headlamp && Item.Kind != EMazeItemKind::AccessCard))
		return false;

	Items.Value.Items[Slot] = FMazeItemInstance();
	WorldItem.Location = Location;
	WorldItem.bAvailable = true;

	return true;
}
