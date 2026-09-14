#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ECS/MazeECSFragments.h"
#include "MazeWorld.generated.h"

class UInstancedStaticMeshComponent;
class UProceduralMeshComponent;

UCLASS()
class LABY_API AMazeWorld : public AActor
{
	GENERATED_BODY()
public:
	AMazeWorld();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	void InitializeMaze();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(ReplicatedUsing = OnRep_Seed, VisibleAnywhere, Category = "Maze")
	int32 Seed = 0;

	FVector StartLocation() const;
	TSharedPtr<const FMazeGeneratedData> GetGeneratedData() const;
	float GetCellSize() const;

private:
	UFUNCTION()
	void OnRep_Seed();
	void Build();

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> Walls;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> Floor;

	UPROPERTY(Transient)
	FMassEntityHandle MazeEntity;

	UPROPERTY(Transient)
	TObjectPtr<class UMazeECSSubsystem> ECSSubsystem;
};
