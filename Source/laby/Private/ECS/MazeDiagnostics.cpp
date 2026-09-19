#include "ECS/MazeDiagnostics.h"
#include "ECS/MazeECSSubsystem.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"
#include "Engine/World.h"
#include "CoreGlobals.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

FMazeDiagnosticsSnapshot UMazeECSSubsystem::ReadDiagnostics() const
{
	check(IsInGameThread());
	TRACE_CPUPROFILER_EVENT_SCOPE(Maze_ReadDiagnostics);

	FMazeDiagnosticsSnapshot Result;
	const UWorld* World = GetWorld();

	Result.Frame = static_cast<int64>(GFrameCounter);

	if (!World || !MassSubsystem)
	{
		Result.Status = TEXT("World or ECS unavailable");

		return Result;
	}

	Result.World = World->GetPathName();
	Result.WorldType = World->WorldType == EWorldType::PIE ? TEXT("PIE") : TEXT("Game");

	const ENetMode Mode = World->GetNetMode();

	Result.NetMode = Mode == NM_Client            ? TEXT("Client")
	                 : Mode == NM_ListenServer    ? TEXT("ListenServer")
	                 : Mode == NM_DedicatedServer ? TEXT("DedicatedServer")
	                                              : TEXT("Standalone");
	Result.bAuthoritative = Mode != NM_Client;
	Result.bPaused = World->IsPaused();
	Result.WorldSeconds = World->GetTimeSeconds();

	auto& Manager = MassSubsystem->GetMutableEntityManager();

	Result.bSessionValid = Manager.IsEntityValid(SessionEntity);

	if (Result.bSessionValid)
	{
		const auto Session = ReadSession();

		Result.bSessionStarted = Session.bSessionStarted;
		Result.bMenuOpen = Session.bMenuOpen;
		Result.bSettingsOpen = Session.bSettingsOpen;
		Result.bMapOpen = Session.bMapOpen;
		Result.bMazeValid = Manager.IsEntityValid(Session.Maze) &&
		                    Manager.GetFragmentDataPtr<FMazeGenerationFragment>(Session.Maze) != nullptr;

		if (Result.bMazeValid)
		{
			const auto Maze = ReadMaze(Session.Maze);

			Result.Seed = Maze.Seed;
			Result.Size = Maze.Size;
			Result.CellSize = Maze.Cell;
			Result.MazeRevision = Maze.Revision;
			Result.bNeedsGeneration = Maze.bNeedsGeneration;
		}
	}

	FMassEntityQuery Query(Manager.AsShared());

	Query.AddRequirement<FMazeVitalsFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddRequirement<FMazeItemsFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddRequirement<FMazePlayerPoseFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddRequirement<FMazeLocomotionFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddRequirement<FMazeProgressFragment>(EMassFragmentAccess::ReadOnly);

	auto Context = Manager.CreateExecutionContext(0.f);

	Query.ForEachEntityChunk(Context,
	                         [&Result](FMassExecutionContext& Chunk)
	                         {
		                         const auto Vitals = Chunk.GetFragmentView<FMazeVitalsFragment>();
		                         const auto Items = Chunk.GetFragmentView<FMazeItemsFragment>();
		                         const auto Poses = Chunk.GetFragmentView<FMazePlayerPoseFragment>();
		                         const auto Locomotion = Chunk.GetFragmentView<FMazeLocomotionFragment>();
		                         const auto Progress = Chunk.GetFragmentView<FMazeProgressFragment>();

		                         for (int32 Index = 0; Index < Chunk.GetNumEntities(); ++Index)
		                         {
			                         auto& Player = Result.Players.AddDefaulted_GetRef();
			                         Player.Entity = Chunk.GetEntity(Index).DebugGetDescription();
			                         Player.Vitals = Vitals[Index].Value;
			                         Player.Items = Items[Index].Value;
			                         Player.Location = Poses[Index].Location;
			                         Player.bRunning = Locomotion[Index].bRunning;
			                         Player.bOnGround = Locomotion[Index].bOnGround;
			                         Player.ReachedExit = Progress[Index].ReachedExit;
			                         Player.MazeRevision = Progress[Index].MazeRevision;
		                         }
	                         });
	Result.Status = Result.bSessionValid ? TEXT("OK") : TEXT("Session unavailable");

	return Result;
}
