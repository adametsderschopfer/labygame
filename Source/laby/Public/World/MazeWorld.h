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

private:
	UFUNCTION()
	void OnRep_Seed();
	void Build();
	void PrepareMaterials();
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

	UPROPERTY(Transient)
	FMassEntityHandle MazeEntity;

	UPROPERTY(Transient)
	TObjectPtr<class UMazeECSSubsystem> ECSSubsystem;
};
