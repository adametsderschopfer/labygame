#include "World/MazeLocationSettings.h"

TSoftObjectPtr<UMaterialInterface> UMazeLocationSettings::PoolTileMaterial() const
{
	return TSoftObjectPtr<UMaterialInterface>(
	    FSoftObjectPath(TEXT("/Game/Materials/Laboratory/MI_PoolTile.MI_PoolTile")));
}

TSoftObjectPtr<UMaterialInterface> UMazeLocationSettings::WaterMaterial() const
{
	return TSoftObjectPtr<UMaterialInterface>(
	    FSoftObjectPath(TEXT("/Game/Materials/Laboratory/MI_RoomWater.MI_RoomWater")));
}

bool UMazeLocationSettings::Validate(FString& Error) const
{
	if (ChunkCells < 2 || ChunkCells > 32 || LoadRadius < 1 || LoadRadius > 8 || UnloadRadius <= LoadRadius ||
	    UnloadRadius > 12 || !FMath::IsFinite(CommitBudgetMs) || CommitBudgetMs <= 0 ||
	    MaxRetainedChunks < (2 * LoadRadius + 1) * (2 * LoadRadius + 1))
	{
		Error = TEXT("Invalid location streaming budget or radii");

		return false;
	}

	TSet<FString> Maps;

	if (PoolTileMaterial().IsNull() || WaterMaterial().IsNull())
	{
		Error = TEXT("Room presentation materials must be configured");

		return false;
	}

	for (const auto& Location : Locations)
	{
		const FString Map = Location.Map.GetLongPackageName();

		if (!Location.Map.IsValid() || Maps.Contains(Map))
		{
			Error = TEXT("Location maps must be valid and unique");

			return false;
		}

		Maps.Add(Map);

		for (const auto& Asset : Location.Assets)
			if (!Asset.IsValid())
			{
				Error = FString::Printf(TEXT("Invalid dependency for %s"), *Map);

				return false;
			}
	}

	return true;
}
