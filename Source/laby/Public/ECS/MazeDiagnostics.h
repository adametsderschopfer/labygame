#pragma once
#include "CoreMinimal.h"
#include "ECS/MazeVitals.h"
#include "ECS/MazeItems.h"
#include "MazeDiagnostics.generated.h"

// Detached copies for diagnostics. Entity labels are only meaningful in the reported world.
USTRUCT()
struct LABY_API FMazePlayerDiagnostics
{
	GENERATED_BODY()
	UPROPERTY()
	FString Entity;
	UPROPERTY()
	FMazeVitals Vitals;
	UPROPERTY()
	FMazeItemsSnapshot Items;
	UPROPERTY()
	FVector Location = FVector::ZeroVector;
	UPROPERTY()
	bool bRunning = false;
	UPROPERTY()
	bool bOnGround = false;
	UPROPERTY()
	int32 ReachedExit = 0;
	UPROPERTY()
	int64 MazeRevision = 0;
};

USTRUCT()
struct LABY_API FMazeDiagnosticsSnapshot
{
	GENERATED_BODY()
	UPROPERTY()
	FString Status;
	UPROPERTY()
	FString World;
	UPROPERTY()
	FString WorldType;
	UPROPERTY()
	FString NetMode;
	UPROPERTY()
	int64 Frame = 0;
	UPROPERTY()
	double WorldSeconds = 0;
	UPROPERTY()
	bool bAuthoritative = false;
	UPROPERTY()
	bool bPaused = false;
	UPROPERTY()
	bool bSessionValid = false;
	UPROPERTY()
	bool bSessionStarted = false;
	UPROPERTY()
	bool bMenuOpen = false;
	UPROPERTY()
	bool bSettingsOpen = false;
	UPROPERTY()
	bool bMapOpen = false;
	UPROPERTY()
	bool bMazeValid = false;
	UPROPERTY()
	int32 Seed = 0;
	UPROPERTY()
	int32 Size = 0;
	UPROPERTY()
	float CellSize = 0;
	UPROPERTY()
	int64 MazeRevision = 0;
	UPROPERTY()
	bool bNeedsGeneration = false;
	UPROPERTY()
	TArray<FMazePlayerDiagnostics> Players;
	UPROPERTY()
	bool bLocationReady = false;
	// Resource fields are enriched by the editor adapter; the ECS reader never queries Actors.
	UPROPERTY()
	bool bHasResourceDiagnostics = false;
	UPROPERTY()
	FString LocationFailure;
	UPROPERTY()
	int32 PendingPSOs = 0;
	UPROPERTY()
	int32 ResidentChunks = 0;
	UPROPERTY()
	int32 PendingChunks = 0;
	// Source-array estimate only, not process memory, collision memory or VRAM.
	UPROPERTY()
	int64 ResidentGeometryBytes = 0;
};
