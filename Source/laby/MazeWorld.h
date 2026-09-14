#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MazeLayout.h"
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
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(ReplicatedUsing=OnRep_Seed, VisibleAnywhere, Category="Maze") int32 Seed = 0;
	FVector StartLocation() const;
	int32 ExitAt(const FVector& Location) const;
	const FMazeLayout& GetLayout() const { return Layout; }
	float GetCellSize() const { return Cell; }
private:
	UFUNCTION() void OnRep_Seed();
	void Build();
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Walls;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Floor;
	FMazeLayout Layout;
	static constexpr float Cell = 875.f;
};
