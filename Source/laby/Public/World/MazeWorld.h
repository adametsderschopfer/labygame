#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ECS/MazeECSFragments.h"
#include "MazeWorld.generated.h"

class UInstancedStaticMeshComponent;
class UProceduralMeshComponent;
class UStaticMeshComponent;
class AMazeChunkView;

struct FMazeChunkJob;
struct FMazeDoorView;

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
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(ReplicatedUsing = OnRep_Seed, VisibleAnywhere, Category = "Maze")
	int32 Seed = 0;

	FVector StartLocation() const;
	TSharedPtr<const FMazeGeneratedData, ESPMode::ThreadSafe> GetGeneratedData() const;
	float GetCellSize() const;
	bool IsPickupComponent(const UPrimitiveComponent* Component) const;
	bool GetPickupOutline(TArray<FVector>& OutPoints) const;
	bool IsDoorCollisionComponent(const UPrimitiveComponent* Component, int32 Instance, int32 DoorIndex) const;
	FMassEntityHandle GetMazeEntity() const
	{
		return MazeEntity;
	}

private:
	UFUNCTION()
	void OnRep_Seed();
	UFUNCTION()
	void OnRep_DoorTargets();
	UFUNCTION()
	void OnRep_HeadlampAvailable();
	void Build();
	void RefreshWorldItem();
	void PrepareMaterials();
	void PrepareDoorAssets();
	void RebuildDoorInstances();
	void UpdateDoors();
	void ClearChunks();
	void UpdateChunks();

	UPROPERTY(Transient)
	TMap<FIntPoint, TObjectPtr<AMazeChunkView>> ChunkViews;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UMaterialInterface>> VisualMaterials;
	TSharedPtr<FMazeChunkJob> PendingChunk;
	TArray<float> AppliedDoorOpenAmounts;
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

	UPROPERTY(ReplicatedUsing = OnRep_HeadlampAvailable)
	bool bReplicatedHeadlampAvailable = true;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> DoorFrames;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> DoorLeaves;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> DoorHandles;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> DoorCollision;

	UPROPERTY(ReplicatedUsing = OnRep_DoorTargets)
	TArray<uint8> ReplicatedDoorTargets;

	UPROPERTY(Transient)
	FMassEntityHandle MazeEntity;

	UPROPERTY(Transient)
	TObjectPtr<class UMazeECSSubsystem> ECSSubsystem;
};
