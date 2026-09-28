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
	FTransform DoorLeafTransform(const FMazeDoorView& Door, float Side) const;
	FTransform DoorCollisionTransform(const FMazeDoorView& Door, float Side) const;
	void ClearChunks();
	void UpdateChunks();

	UPROPERTY(Transient)
	TMap<FIntPoint, TObjectPtr<AMazeChunkView>> ChunkViews;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UMaterialInterface>> VisualMaterials;
	TSharedPtr<FMazeChunkJob> PendingChunk;
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
	TObjectPtr<UInstancedStaticMeshComponent> DoorLeavesLeft;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> DoorLeavesRight;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> DoorCollisionLeft;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> DoorCollisionRight;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> DoorStatusLights;

	UPROPERTY(ReplicatedUsing = OnRep_DoorTargets)
	TArray<uint8> ReplicatedDoorTargets;

	UPROPERTY(Transient)
	FMassEntityHandle MazeEntity;

	UPROPERTY(Transient)
	TObjectPtr<class UMazeECSSubsystem> ECSSubsystem;
};
