#include "ECS/MazeItemSystem.h"

void FMazeItemSystem::InitializeLoadout(FMazeItemsFragment& Items)
{
	Items.Value = FMazeItemsSnapshot();

	FMazeItemInstance Lamp;

	Lamp.Id = Items.Value.NextInstanceId++;
	Lamp.Kind = EMazeItemKind::Headlamp;
	Lamp.Slot = EMazeEquipmentSlot::Head;
	Items.Value.Items.Add(Lamp);
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
