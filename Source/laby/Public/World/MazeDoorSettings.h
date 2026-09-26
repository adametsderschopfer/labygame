#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MazeDoorSettings.generated.h"

class UMaterialInterface;
class UStaticMesh;

// Presentation resources only. Door identity and motion state remain in Mass ECS.
UCLASS(Config = Game, DefaultConfig)
class LABY_API UMazeDoorSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = Door)
	TSoftObjectPtr<UStaticMesh> FrameMesh;

	UPROPERTY(Config, EditAnywhere, Category = Door)
	TSoftObjectPtr<UStaticMesh> LeftLeafMesh;

	UPROPERTY(Config, EditAnywhere, Category = Door)
	TSoftObjectPtr<UStaticMesh> RightLeafMesh;

	UPROPERTY(Config, EditAnywhere, Category = Door)
	TSoftObjectPtr<UMaterialInterface> SurfaceMaterial;

	UPROPERTY(Config, EditAnywhere, Category = Door)
	TSoftObjectPtr<UMaterialInterface> StatusMaterial;
};
