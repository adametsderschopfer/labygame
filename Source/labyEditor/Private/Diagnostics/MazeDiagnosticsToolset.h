#pragma once
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "ECS/MazeDiagnostics.h"
#include "MazeDiagnosticsToolset.generated.h"

UCLASS(BlueprintType, Hidden)
class UMazeDiagnosticsToolset : public UToolsetDefinition
{
	GENERATED_BODY()
public:
	/** Reads detached ECS snapshots for every active game/PIE world. Never starts Play or changes gameplay. Client snapshots are local mirrors, not server authority. */
	UFUNCTION(meta = (AICallable), Category = "MazeDiagnostics")
	static TArray<FMazeDiagnosticsSnapshot> ReadSnapshots();
};
