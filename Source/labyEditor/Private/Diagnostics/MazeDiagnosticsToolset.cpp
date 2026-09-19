#include "Diagnostics/MazeDiagnosticsToolset.h"
#include "ECS/MazeECSSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

TArray<FMazeDiagnosticsSnapshot> UMazeDiagnosticsToolset::ReadSnapshots()
{
	check(IsInGameThread());

	TArray<FMazeDiagnosticsSnapshot> Result;

	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();

			if (!World || !World->IsGameWorld())
				continue;

			if (const auto* ECS = World->GetSubsystem<UMazeECSSubsystem>())
				Result.Add(ECS->ReadDiagnostics());
		}
	}

	if (Result.IsEmpty())
	{
		auto& Missing = Result.AddDefaulted_GetRef();

		Missing.Status = TEXT("No active game/PIE ECS world. Play was not started.");
	}

	return Result;
}
