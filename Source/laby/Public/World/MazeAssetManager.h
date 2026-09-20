#pragma once

#include "Engine/AssetManager.h"
#include "MazeAssetManager.generated.h"

// Engine resource/cook adapter only; no gameplay services or state.
UCLASS()
class LABY_API UMazeAssetManager : public UAssetManager
{
	GENERATED_BODY()
public:
#if WITH_EDITOR
	virtual void ModifyCook(TConstArrayView<const ITargetPlatform*> TargetPlatforms,
	                        TArray<FName>& PackagesToCook,
	                        TArray<FName>& PackagesToNeverCook) override;
#endif
};
