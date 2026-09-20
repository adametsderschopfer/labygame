#include "World/MazeAssetManager.h"
#include "World/MazeLocationSettings.h"

#if WITH_EDITOR
void UMazeAssetManager::ModifyCook(TConstArrayView<const ITargetPlatform*> TargetPlatforms,
                                   TArray<FName>& PackagesToCook,
                                   TArray<FName>& PackagesToNeverCook)
{
	Super::ModifyCook(TargetPlatforms, PackagesToCook, PackagesToNeverCook);

	const auto* Settings = GetDefault<UMazeLocationSettings>();
	FString Error;

	if (!Settings->Validate(Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid location manifest during cook: %s"), *Error);

		return;
	}

	const auto Add = [&](const FSoftObjectPath& Path)
	{
		const FName Package(*Path.GetLongPackageName());

		if (PackagesToNeverCook.Contains(Package))
			UE_LOG(LogTemp, Error, TEXT("Required location package is excluded from cook: %s"), *Package.ToString());
		else
			PackagesToCook.AddUnique(Package);
	};

	for (const auto& Location : Settings->Locations)
	{
		Add(Location.Map);

		for (const auto& Path : Location.Assets)
			Add(Path);
	}
}

#endif
