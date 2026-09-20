#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "MazeChunkView.generated.h"

struct FMazeChunkData;

class UMaterialInterface;
class UStaticMesh;

UCLASS()
class LABY_API UMazeCollisionMeshComponent : public UProceduralMeshComponent
{
	GENERATED_BODY()
public:
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override
	{
		return nullptr;
	}
};

UCLASS()
class LABY_API UMazeVisualMeshComponent : public UProceduralMeshComponent
{
	GENERATED_BODY()
public:
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual void CollectPSOPrecacheData(const FPSOPrecacheParams& Params,
	                                    FMaterialInterfacePSOPrecacheParamsList& OutParams) override;
};

// Disposable representation: no authoritative gameplay state, physics or replication.
UCLASS(NotBlueprintable)
class LABY_API AMazeChunkView : public AActor
{
	GENERATED_BODY()
public:
	AMazeChunkView();
	void Apply(const FMazeChunkData& Data, UStaticMesh* Cube, const TArray<UMaterialInterface*>& Materials);

	int64 GeometryBytes = 0;

private:
	UPROPERTY()
	TObjectPtr<UMazeVisualMeshComponent> Mesh;

	UPROPERTY()
	TObjectPtr<class UInstancedStaticMeshComponent> Floor;

	UPROPERTY()
	TObjectPtr<class UStaticMeshComponent> Ceiling;
};
