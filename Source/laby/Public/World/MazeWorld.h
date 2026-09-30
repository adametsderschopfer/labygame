#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ECS/MazeECSFragments.h"
#include "ECS/MazeItems.h"
#include "MazeWorld.generated.h"

class UInstancedStaticMeshComponent;
class UProceduralMeshComponent;
class UStaticMeshComponent;
class AMazeChunkView;

struct FMazeChunkJob;
struct FMazeDoorView;

USTRUCT()
struct FMazeDoorNetworkSnapshot
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 MazeRevision = 0;

	UPROPERTY()
	TArray<uint8> States;
};

USTRUCT()
struct FMazeWorldItemNetworkSnapshot
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 MazeRevision = 0;

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	bool bAvailable = true;
};

UCLASS()
class LABY_API AMazeWorld : public AActor
{
	GENERATED_BODY()
public:
	AMazeWorld();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	int32 GetResidentChunkCount() const
	{
		return ChunkViews.Num();
	}

	int64 GetResidentGeometryBytes() const;
	bool HasPendingChunk() const
	{
		return PendingChunk.IsValid();
	}

	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	void InitializeMaze();
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	void InvalidateDevelopmentTeleportReadiness();
#endif
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(ReplicatedUsing = OnRep_Seed, VisibleAnywhere, Category = "Maze")
	int32 Seed = 0;

	FVector StartLocation() const;
	TSharedPtr<const FMazeGeneratedData, ESPMode::ThreadSafe> GetGeneratedData() const;
	float GetCellSize() const;
	int32 PickupId(const UPrimitiveComponent* Component) const;
	bool IsPickupComponent(const UPrimitiveComponent* Component) const;
	bool GetPickupOutline(TArray<FVector>& OutPoints, int32 ItemId = FMazeItemDefinition::HeadlampWorldId) const;
	bool IsDoorCollisionComponent(const UPrimitiveComponent* Component, int32 Instance, int32 DoorIndex) const;
	FMassEntityHandle GetMazeEntity() const
	{
		return MazeEntity;
	}

private:
	UFUNCTION()
	void OnRep_Seed();
	UFUNCTION()
	void OnRep_DoorStates();
	UFUNCTION()
	void OnRep_WorldItem();
	void Build();
	void RefreshWorldItem();
	void PrepareMaterials();
	void PrepareDoorAssets();
	void RebuildDoorInstances();
	void UpdateDoors(float DeltaSeconds);
	void ClearChunks();
	void UpdateChunks();

	UPROPERTY(Transient)
	TMap<FIntPoint, TObjectPtr<AMazeChunkView>> ChunkViews;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UMaterialInterface>> VisualMaterials;
	TSharedPtr<FMazeChunkJob> PendingChunk;
	TArray<float> AppliedDoorOpenAmounts;
	TArray<int8> AppliedDoorSwingSigns;
	TArray<int32> ReaderDoorIndices;
	TArray<bool> AppliedDoorUnlocks;
	bool bInitialChunksReady = false;
	bool bPresentationStarted = false;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> Walls;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> Floor;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Ceiling;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> HeadlampBody;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> HeadlampLens;

	UPROPERTY(ReplicatedUsing = OnRep_WorldItem)
	FMazeWorldItemNetworkSnapshot ReplicatedWorldItem;

	UPROPERTY(ReplicatedUsing = OnRep_WorldItem)
	FMazeWorldItemNetworkSnapshot ReplicatedCard;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> CardBody;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> CardStripe;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> DoorReaders;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> DoorScreens;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> DoorFrames;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> DoorLeaves;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> DoorHandles;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> DoorCollision;

	UPROPERTY(ReplicatedUsing = OnRep_DoorStates)
	FMazeDoorNetworkSnapshot ReplicatedDoorStates;

	UPROPERTY(Transient)
	FMassEntityHandle MazeEntity;

	UPROPERTY(Transient)
	TObjectPtr<class UMazeECSSubsystem> ECSSubsystem;
};
