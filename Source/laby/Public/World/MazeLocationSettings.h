#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MazeLocationSettings.generated.h"

UENUM()
enum class EMazeLocationMode : uint8
{
	Whole,
	Procedural,
	WorldPartition
};

// Resource policy only. Gameplay definitions and persistent facts belong to ECS.
USTRUCT()
struct FMazeLocationDefinition
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere)
	FSoftObjectPath Map;
	UPROPERTY(EditAnywhere)
	EMazeLocationMode Mode = EMazeLocationMode::Whole;
	UPROPERTY(EditAnywhere)
	TArray<FSoftObjectPath> Assets;
};

UCLASS(Config = Game, DefaultConfig)
class LABY_API UMazeLocationSettings : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(Config, EditAnywhere, Category = Streaming)
	int32 ChunkCells = 8;

	UPROPERTY(Config, EditAnywhere, Category = Streaming)
	int32 LoadRadius = 2;

	UPROPERTY(Config, EditAnywhere, Category = Streaming)
	int32 UnloadRadius = 3;

	// Soft scheduling budget: a single engine resource operation cannot be preempted.
	UPROPERTY(Config, EditAnywhere, Category = Streaming)
	float CommitBudgetMs = 3.f;

	UPROPERTY(Config, EditAnywhere, Category = Streaming)
	int32 MaxRetainedChunks = 64;

	UPROPERTY(Config, EditAnywhere, Category = Streaming)
	TArray<FMazeLocationDefinition> Locations;

	bool Validate(FString& Error) const;
};
