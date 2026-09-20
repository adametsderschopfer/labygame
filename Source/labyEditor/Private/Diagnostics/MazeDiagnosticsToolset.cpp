#include "Diagnostics/MazeDiagnosticsToolset.h"
#include "ECS/MazeECSSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "World/MazeLocationSubsystem.h"
#include "World/MazeWorld.h"
#include "EngineUtils.h"

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
			{
				auto& Snapshot = Result.Add_GetRef(ECS->ReadDiagnostics());

				Snapshot.bHasResourceDiagnostics = true;

				if (const auto* Location = World->GetSubsystem<UMazeLocationSubsystem>())
				{
					Snapshot.bLocationReady = Location->IsReady();
					Snapshot.LocationFailure = Location->GetFailure();
					Snapshot.PendingPSOs = Location->GetPendingPSOs();
				}

				for (TActorIterator<AMazeWorld> It(World); It; ++It)
				{
					Snapshot.ResidentChunks += It->GetResidentChunkCount();
					Snapshot.PendingChunks += It->HasPendingChunk() ? 1 : 0;
					Snapshot.ResidentGeometryBytes += It->GetResidentGeometryBytes();
				}
			}
		}
	}

	if (Result.IsEmpty())
	{
		auto& Missing = Result.AddDefaulted_GetRef();

		Missing.Status = TEXT("No active game/PIE ECS world. Play was not started.");
	}

	return Result;
}
