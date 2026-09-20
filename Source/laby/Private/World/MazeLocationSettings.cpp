#include "World/MazeLocationSettings.h"

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
