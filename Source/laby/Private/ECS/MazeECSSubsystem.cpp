#include "ECS/MazeECSSubsystem.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ECS/MazeVitalsSystem.h"
#include "ECS/MazeGameplaySystems.h"
#include "ECS/MazeExplorationSystem.h"
#include "ECS/MazeItemSystem.h"
#include "ECS/MazeSignal.h"
#include "ECS/MazeDoorSystem.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"
#include "Engine/World.h"
#include "Maze/MazeInterior.h"

template <typename T> T* UMazeECSSubsystem::FindFragment(FMassEntityHandle Entity) const
{
	check(IsInGameThread());

	if (!MassSubsystem || !MassSubsystem->GetEntityManager().IsEntityValid(Entity))
		return nullptr;

	return MassSubsystem->GetEntityManager().GetFragmentDataPtr<T>(Entity);
}

void UMazeECSSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UMassEntitySubsystem>();
	Super::Initialize(Collection);
	MassSubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	check(MassSubsystem);

	auto& Manager = MassSubsystem->GetMutableEntityManager();
	const UScriptStruct* Fragments[] = {FMazeVitalsFragment::StaticStruct(),
	                                    FMazeLocomotionFragment::StaticStruct(),
	                                    FMazePlayerInputFragment::StaticStruct(),
	                                    FMazePlayerPoseFragment::StaticStruct(),
	                                    FMazePlayerCommandFragment::StaticStruct(),
	                                    FMazeSignalFragment::StaticStruct(),
	                                    FMazeProgressFragment::StaticStruct(),
	                                    FMazeExplorationFragment::StaticStruct(),
	                                    FMazeItemsFragment::StaticStruct()};

	PlayerArchetype = Manager.CreateArchetype(MakeArrayView(Fragments));
	VitalsQuery = MakeUnique<FMassEntityQuery>(Manager.AsShared());
	VitalsQuery->AddRequirement<FMazeVitalsFragment>(EMassFragmentAccess::ReadWrite);
	VitalsQuery->AddRequirement<FMazeLocomotionFragment>(EMassFragmentAccess::ReadOnly);

	const UScriptStruct* MazeFragments[] = {FMazeGenerationFragment::StaticStruct()};

	MazeArchetype = Manager.CreateArchetype(MakeArrayView(MazeFragments));
	GenerationQuery = MakeUnique<FMassEntityQuery>(Manager.AsShared());
	GenerationQuery->AddRequirement<FMazeGenerationFragment>(EMassFragmentAccess::ReadWrite);
	InputQuery = MakeUnique<FMassEntityQuery>(Manager.AsShared());
	InputQuery->AddRequirement<FMazePlayerInputFragment>(EMassFragmentAccess::ReadWrite);
	InputQuery->AddRequirement<FMazeLocomotionFragment>(EMassFragmentAccess::ReadWrite);
	SignalQuery = MakeUnique<FMassEntityQuery>(Manager.AsShared());
	SignalQuery->AddRequirement<FMazeSignalFragment>(EMassFragmentAccess::ReadWrite);
	DoorPlayerQuery = MakeUnique<FMassEntityQuery>(Manager.AsShared());
	DoorPlayerQuery->AddRequirement<FMazePlayerPoseFragment>(EMassFragmentAccess::ReadOnly);
	DoorPlayerQuery->AddRequirement<FMazeVitalsFragment>(EMassFragmentAccess::ReadOnly);

	const UScriptStruct* DoorFragments[] = {FMazeDoorFragment::StaticStruct()};

	DoorArchetype = Manager.CreateArchetype(MakeArrayView(DoorFragments));
	DoorQuery = MakeUnique<FMassEntityQuery>(Manager.AsShared());
	DoorQuery->AddRequirement<FMazeDoorFragment>(EMassFragmentAccess::ReadWrite);

	const UScriptStruct* SessionFragments[] = {FMazeSessionFragment::StaticStruct(), FMazeRoomFragment::StaticStruct()};

	SessionEntity = Manager.CreateEntity(Manager.CreateArchetype(MakeArrayView(SessionFragments)));
}

void UMazeECSSubsystem::Deinitialize()
{
	if (MassSubsystem)
		for (const FMassEntityHandle Entity : DoorEntities)
			if (MassSubsystem->GetEntityManager().IsEntityValid(Entity))
				MassSubsystem->GetMutableEntityManager().DestroyEntity(Entity);

	DoorEntities.Reset();
	VitalsQuery.Reset();
	GenerationQuery.Reset();
	InputQuery.Reset();
	SignalQuery.Reset();
	DoorQuery.Reset();
	DoorPlayerQuery.Reset();

	if (MassSubsystem && MassSubsystem->GetEntityManager().IsEntityValid(SessionEntity))
		MassSubsystem->GetMutableEntityManager().DestroyEntity(SessionEntity);

	SessionEntity = FMassEntityHandle();
	MazeArchetype = FMassArchetypeHandle();
	DoorArchetype = FMassArchetypeHandle();
	PlayerArchetype = FMassArchetypeHandle();
	MassSubsystem = nullptr;
	Super::Deinitialize();
}

bool UMazeECSSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UMazeECSSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMazeECSSubsystem, STATGROUP_Tickables);
}

void UMazeECSSubsystem::Tick(float DeltaSeconds)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Maze_ECS_Tick);
	Super::Tick(DeltaSeconds);

	if (!MassSubsystem || !VitalsQuery || !SignalQuery || !DoorQuery || !DoorPlayerQuery ||
	    !GetWorld()->HasBegunPlay() || GetWorld()->IsPaused())
		return;

	auto Context = MassSubsystem->GetMutableEntityManager().CreateExecutionContext(DeltaSeconds);
	const bool bAuthority = GetWorld()->GetNetMode() != NM_Client;

	SignalQuery->ForEachEntityChunk(Context,
	                                [DeltaSeconds, bAuthority](FMassExecutionContext& Chunk)
	                                {
		                                auto Signals = Chunk.GetMutableFragmentView<FMazeSignalFragment>();

		                                for (auto& Signal : Signals)
			                                FMazeSignalSystem::Update(Signal, DeltaSeconds, bAuthority);
	                                });

	if (!FMazeDoorDefinition::bEnabled)
	{
		// Drop pre-patch entities once when Live Coding is applied to an existing world.
		auto& Manager = MassSubsystem->GetMutableEntityManager();

		for (const FMassEntityHandle Entity : DoorEntities)
			if (Manager.IsEntityValid(Entity))
				Manager.DestroyEntity(Entity);

		DoorEntities.Reset();
	}
	else
	{
		TArray<FVector> PlayerLocations;

		if (bAuthority)
			DoorPlayerQuery->ForEachEntityChunk(Context,
			                                    [&PlayerLocations](FMassExecutionContext& Chunk)
			                                    {
				                                    const auto Poses = Chunk.GetFragmentView<FMazePlayerPoseFragment>();
				                                    const auto Vitals = Chunk.GetFragmentView<FMazeVitalsFragment>();

				                                    for (int32 Index = 0; Index < Chunk.GetNumEntities(); ++Index)
					                                    if (FMazeVitalsSystem::IsAlive(Vitals[Index].Value))
						                                    PlayerLocations.Add(Poses[Index].Location);
			                                    });

		DoorQuery->ForEachEntityChunk(Context,
		                              [DeltaSeconds, bAuthority, &PlayerLocations](FMassExecutionContext& Chunk)
		                              {
			                              auto Doors = Chunk.GetMutableFragmentView<FMazeDoorFragment>();

			                              for (auto& Door : Doors)
				                              FMazeDoorSystem::Update(Door, DeltaSeconds, PlayerLocations, bAuthority);
		                              });
	}

	if (!bAuthority)
		return;

	VitalsQuery->ForEachEntityChunk(Context,
	                                [DeltaSeconds](FMassExecutionContext& Chunk)
	                                {
		                                auto Vitals = Chunk.GetMutableFragmentView<FMazeVitalsFragment>();
		                                const auto Locomotion = Chunk.GetFragmentView<FMazeLocomotionFragment>();

		                                for (int32 Index = 0; Index < Chunk.GetNumEntities(); ++Index)
			                                FMazeVitalsSystem::Update(Vitals[Index].Value,
			                                                          DeltaSeconds,
			                                                          Locomotion[Index].bRunning,
			                                                          Locomotion[Index].bOnGround);
	                                });
}

FMassEntityHandle UMazeECSSubsystem::CreatePlayer()
{
	check(IsInGameThread());

	if (!MassSubsystem)
		return FMassEntityHandle();

	const auto Entity = MassSubsystem->GetMutableEntityManager().CreateEntity(PlayerArchetype);

	FindFragment<FMazeProgressFragment>(Entity)->Maze = ReadSession().Maze;

	if (GetWorld()->GetNetMode() != NM_Client)
		FMazeItemSystem::InitializeLoadout(*FindFragment<FMazeItemsFragment>(Entity));

	return Entity;
}

FMazeItemsSnapshot UMazeECSSubsystem::ReadItems(FMassEntityHandle Entity) const
{
	const auto* Items = FindFragment<FMazeItemsFragment>(Entity);

	return Items ? Items->Value : FMazeItemsSnapshot();
}

bool UMazeECSSubsystem::ReadHeadlampEnabled(FMassEntityHandle Entity) const
{
	const auto* Items = FindFragment<FMazeItemsFragment>(Entity);

	return Items && FMazeItemSystem::IsHeadlampEnabled(*Items);
}

bool UMazeECSSubsystem::ToggleHeadlamp(FMassEntityHandle Entity)
{
	if (GetWorld()->GetNetMode() == NM_Client || GetWorld()->IsPaused())
		return false;

	auto* Items = FindFragment<FMazeItemsFragment>(Entity);
	const auto* Pose = FindFragment<FMazePlayerPoseFragment>(Entity);
	const auto Room = ReadRoom();

	return Items && FMazeItemSystem::ToggleHeadlamp(*Items,
	                                                Pose && Pose->bInputEnabled &&
	                                                    FMazeVitalsSystem::IsAlive(ReadVitals(Entity)) &&
	                                                    (!Room.bActive || Room.bStarted));
}

void UMazeECSSubsystem::ReceiveItems(FMassEntityHandle Entity, const FMazeItemsSnapshot& Snapshot)
{
	if (GetWorld()->GetNetMode() != NM_Client)
		return;

	if (auto* Items = FindFragment<FMazeItemsFragment>(Entity))
		Items->Value = Snapshot;
}

void UMazeECSSubsystem::DestroyPlayer(FMassEntityHandle Entity)
{
	check(IsInGameThread());

	if (MassSubsystem && MassSubsystem->GetEntityManager().IsEntityValid(Entity))
		MassSubsystem->GetMutableEntityManager().DestroyEntity(Entity);
}

FMazeVitals* UMazeECSSubsystem::FindVitals(FMassEntityHandle Entity) const
{
	auto* Fragment = FindFragment<FMazeVitalsFragment>(Entity);

	return Fragment ? &Fragment->Value : nullptr;
}

FMazeVitals UMazeECSSubsystem::ReadVitals(FMassEntityHandle Entity) const
{
	if (const auto* Vitals = FindVitals(Entity))
		return *Vitals;

	FMazeVitals Missing;

	Missing.Health = Missing.Stamina = 0.f;

	return Missing;
}

void UMazeECSSubsystem::SetLocomotion(FMassEntityHandle Entity, bool bRunning, bool bOnGround)
{
	check(IsInGameThread());

	if (!MassSubsystem || !MassSubsystem->GetEntityManager().IsEntityValid(Entity))
		return;

	if (auto* Fragment = MassSubsystem->GetEntityManager().GetFragmentDataPtr<FMazeLocomotionFragment>(Entity))
	{
		Fragment->bRunning = bRunning;
		Fragment->bOnGround = bOnGround;
	}
}

bool UMazeECSSubsystem::SpendJumpStamina(FMassEntityHandle Entity)
{
	auto* Vitals = FindVitals(Entity);

	return Vitals && FMazeVitalsSystem::SpendJumpStamina(*Vitals);
}

float UMazeECSSubsystem::ApplyDamage(FMassEntityHandle Entity, float Amount)
{
	auto* Vitals = FindVitals(Entity);

	return Vitals ? FMazeVitalsSystem::Damage(*Vitals, Amount) : 0.f;
}

void UMazeECSSubsystem::SetInputAxis(FMassEntityHandle Entity, EMazeInputAxis Axis, float Value)
{
	if (!FMath::IsFinite(Value))
		return;

	if (auto* Input = FindFragment<FMazePlayerInputFragment>(Entity))
	{
		switch (Axis)
		{
		case EMazeInputAxis::Forward:
			Input->Forward = FMath::Clamp(Value, -1.f, 1.f);
			break;
		case EMazeInputAxis::Right:
			Input->Right = FMath::Clamp(Value, -1.f, 1.f);
			break;
		case EMazeInputAxis::Yaw:
			Input->Yaw += Value;
			break;
		case EMazeInputAxis::Pitch:
			Input->Pitch += Value;
			break;
		}
	}
}

void UMazeECSSubsystem::SetInputAction(FMassEntityHandle Entity, EMazeInputAction Action, bool bPressed)
{
	if (auto* Input = FindFragment<FMazePlayerInputFragment>(Entity))
	{
		if (Action == EMazeInputAction::Sprint)
			Input->bSprintHeld = bPressed;
		else if (Action == EMazeInputAction::Crouch)
			Input->bCrouchHeld = bPressed;
		else if (Action == EMazeInputAction::Jump)
		{
			Input->bJumpPressed = bPressed && !Input->bJumpHeld;
			Input->bJumpHeld = bPressed;
		}
	}
}

void UMazeECSSubsystem::ClearPlayerInput()
{
	if (!MassSubsystem || !InputQuery)
		return;

	auto Context = MassSubsystem->GetMutableEntityManager().CreateExecutionContext(0.f);

	InputQuery->ForEachEntityChunk(Context,
	                               [](FMassExecutionContext& Chunk)
	                               {
		                               auto Inputs = Chunk.GetMutableFragmentView<FMazePlayerInputFragment>();
		                               auto Locomotion = Chunk.GetMutableFragmentView<FMazeLocomotionFragment>();

		                               for (int32 I = 0; I < Chunk.GetNumEntities(); ++I)
		                               {
			                               Inputs[I] = FMazePlayerInputFragment();
			                               Locomotion[I].bRunning = false;
		                               }
	                               });
}

FMazePlayerCommandFragment UMazeECSSubsystem::ResolvePlayer(FMassEntityHandle Entity,
                                                            const FMazePlayerPoseFragment& Pose)
{
	auto* Input = FindFragment<FMazePlayerInputFragment>(Entity);
	auto* StoredPose = FindFragment<FMazePlayerPoseFragment>(Entity);
	auto* Command = FindFragment<FMazePlayerCommandFragment>(Entity);
	auto* Locomotion = FindFragment<FMazeLocomotionFragment>(Entity);
	auto* Vitals = FindVitals(Entity);

	if (!Input || !StoredPose || !Command || !Locomotion || !Vitals)
	{
		FMazePlayerCommandFragment Missing;

		Missing.bDead = true;

		return Missing;
	}

	*StoredPose = Pose;
	StoredPose->bInputEnabled &= !ReadRoom().bActive || ReadRoom().bStarted;

	const FMassEntityHandle MazeEntity = ReadSession().Maze;
	const auto* Maze = FindFragment<FMazeGenerationFragment>(MazeEntity);

	if (auto* Exploration = FindFragment<FMazeExplorationFragment>(Entity); Exploration && Maze)
		FMazeExplorationSystem::Update(
		    *Exploration, MazeEntity, *Maze, *StoredPose, FMazeVitalsSystem::IsAlive(*Vitals));

	if (GetWorld()->GetNetMode() != NM_Client && Maze)
		FMazeHazardSystem::Apply(*Maze, *StoredPose, *Vitals);

	*Command = FMazePlayerControlSystem::Resolve(*Input, *StoredPose, *Vitals, *Locomotion);

	if (GetWorld()->GetNetMode() != NM_Client)
		UpdateProgress(Entity);

	return *Command;
}

bool UMazeECSSubsystem::RequestSignal(FMassEntityHandle Entity, FMazeSignalSnapshot& OutSignal)
{
	if (GetWorld()->GetNetMode() == NM_Client || GetWorld()->IsPaused())
		return false;

	auto* Signal = FindFragment<FMazeSignalFragment>(Entity);
	const auto* Pose = FindFragment<FMazePlayerPoseFragment>(Entity);
	const auto* Vitals = FindVitals(Entity);
	const auto Session = ReadSession();
	const auto Room = ReadRoom();
	const bool bAllowed = Pose && Vitals && Pose->bInputEnabled && FMazeVitalsSystem::IsAlive(*Vitals) &&
	                      Session.bSessionStarted && (!Room.bActive || Room.bStarted);

	if (!Signal || !Pose || !FMazeSignalSystem::Request(*Signal, Pose->Location, Pose->Forward, bAllowed))
		return false;

	OutSignal = Signal->Value;

	return true;
}

bool UMazeECSSubsystem::ReceiveSignal(FMassEntityHandle Entity, const FMazeSignalSnapshot& Signal)
{
	if (GetWorld()->GetNetMode() != NM_Client)
		return false;

	auto* Stored = FindFragment<FMazeSignalFragment>(Entity);

	return Stored && FMazeSignalSystem::Receive(*Stored, Signal);
}

FMazeSignalView UMazeECSSubsystem::ReadSignal(FMassEntityHandle Entity) const
{
	FMazeSignalView Result;

	if (const auto* Signal = FindFragment<FMazeSignalFragment>(Entity))
	{
		Result.Location = Signal->Value.Location;
		Result.RemainingSeconds = Signal->DisplayRemaining;
		Result.Sequence = Signal->Value.Sequence;
	}

	return Result;
}

void UMazeECSSubsystem::UpdateProgress(FMassEntityHandle Entity)
{
	auto* Progress = FindFragment<FMazeProgressFragment>(Entity);
	const auto* Pose = FindFragment<FMazePlayerPoseFragment>(Entity);
	const auto* Vitals = FindVitals(Entity);

	if (!Progress || !Pose || !Vitals || !FMazeVitalsSystem::IsAlive(*Vitals))
		return;

	if (Progress->Maze != ReadSession().Maze)
	{
		Progress->Maze = ReadSession().Maze;
		Progress->ReachedExit = 0;
	}

	if (const auto* Maze = FindFragment<FMazeGenerationFragment>(Progress->Maze))
	{
		if (Progress->MazeRevision != Maze->Revision)
		{
			Progress->ReachedExit = 0;
			Progress->MazeRevision = Maze->Revision;
		}

		if (!Progress->ReachedExit)
			Progress->ReachedExit = FMazeGenerationSystem::ExitAt(*Maze, Pose->Location);
	}
}

int32 UMazeECSSubsystem::ReadReachedExit(FMassEntityHandle Entity) const
{
	const auto* Progress = FindFragment<FMazeProgressFragment>(Entity);

	return Progress ? Progress->ReachedExit : 0;
}

void UMazeECSSubsystem::GeneratePending()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Maze_GeneratePending);

	if (!MassSubsystem || !GenerationQuery)
		return;

	auto Context = MassSubsystem->GetMutableEntityManager().CreateExecutionContext(0.f);

	GenerationQuery->ForEachEntityChunk(Context,
	                                    [](FMassExecutionContext& Chunk)
	                                    {
		                                    auto Mazes = Chunk.GetMutableFragmentView<FMazeGenerationFragment>();

		                                    for (auto& Maze : Mazes)
			                                    FMazeGenerationSystem::Generate(Maze);
	                                    });
}

FMassEntityHandle UMazeECSSubsystem::CreateMaze(int32 Seed, FVector Origin)
{
	check(IsInGameThread());

	if (!MassSubsystem)
		return FMassEntityHandle();

	const auto Entity = MassSubsystem->GetMutableEntityManager().CreateEntity(MazeArchetype);

	if (Seed == 0)
	{
		Seed = static_cast<int32>(FPlatformTime::Cycles64() & 0x7fffffff);

		if (Seed == 0)
			Seed = 1;
	}

	RegenerateMaze(Entity, Seed, Origin);

	if (auto* Session = FindFragment<FMazeSessionFragment>(SessionEntity))
		Session->Maze = Entity;

	return Entity;
}

void UMazeECSSubsystem::RegenerateMaze(FMassEntityHandle Entity, int32 Seed, FVector Origin)
{
	if (auto* Maze = FindFragment<FMazeGenerationFragment>(Entity))
	{
		if (Maze->Seed == Seed && Maze->Origin == Origin && Maze->Data)
			return;

		Maze->Seed = Seed;
		Maze->Origin = Origin;
		Maze->bNeedsGeneration = true;
		GeneratePending();
		RebuildDoors(Entity);
	}
}

void UMazeECSSubsystem::DestroyMaze(FMassEntityHandle Entity)
{
	DestroyDoors(Entity);

	if (auto* Session = FindFragment<FMazeSessionFragment>(SessionEntity); Session && Session->Maze == Entity)
		Session->Maze = FMassEntityHandle();

	DestroyPlayer(Entity);
}

FMazeGenerationFragment UMazeECSSubsystem::ReadMaze(FMassEntityHandle Entity) const
{
	const auto* Maze = FindFragment<FMazeGenerationFragment>(Entity);

	return Maze ? *Maze : FMazeGenerationFragment();
}

void UMazeECSSubsystem::DestroyDoors(FMassEntityHandle MazeEntity)
{
	if (!MassSubsystem)
		return;

	auto& Manager = MassSubsystem->GetMutableEntityManager();

	for (int32 Index = DoorEntities.Num() - 1; Index >= 0; --Index)
	{
		const FMassEntityHandle Entity = DoorEntities[Index];
		const auto* Door =
		    Manager.IsEntityValid(Entity) ? Manager.GetFragmentDataPtr<FMazeDoorFragment>(Entity) : nullptr;

		if (!Door || Door->Maze == MazeEntity)
		{
			if (Manager.IsEntityValid(Entity))
				Manager.DestroyEntity(Entity);

			DoorEntities.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		}
	}
}

void UMazeECSSubsystem::RebuildDoors(FMassEntityHandle MazeEntity)
{
	DestroyDoors(MazeEntity);

	if (!FMazeDoorDefinition::bEnabled)
		return;

	const auto* Maze = FindFragment<FMazeGenerationFragment>(MazeEntity);

	if (!MassSubsystem || !Maze || !Maze->Data)
		return;

	static const FVector Directions[] = {
	    FVector(0.f, -1.f, 0.f), FVector(1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), FVector(-1.f, 0.f, 0.f)};
	const TArray<FMazeRoomDoorway> Doorways = Maze->Data->Layout.RoomDoorways();
	auto& Manager = MassSubsystem->GetMutableEntityManager();

	DoorEntities.Reserve(DoorEntities.Num() + Doorways.Num());

	for (int32 Index = 0; Index < Doorways.Num(); ++Index)
	{
		const FMazeRoomDoorway& Doorway = Doorways[Index];
		const FVector Normal = Directions[Doorway.Direction];
		const FVector SlideAxis(-Normal.Y, Normal.X, 0.f);
		const FVector LocalCenter =
		    FVector((Doorway.Cell.X + 0.5f) * Maze->Cell, (Doorway.Cell.Y + 0.5f) * Maze->Cell, 0.f) +
		    Normal * (Maze->Cell * 0.5f);
		const FMassEntityHandle Entity = Manager.CreateEntity(DoorArchetype);
		auto& Door = Manager.GetFragmentDataChecked<FMazeDoorFragment>(Entity);

		Door.Maze = MazeEntity;
		Door.MazeRevision = Maze->Revision;
		Door.Index = Index;
		Door.Center = Maze->Origin + LocalCenter;
		Door.SlideAxis = SlideAxis;
		Door.Normal = Normal;
		DoorEntities.Add(Entity);
	}
}

TArray<FMazeDoorView> UMazeECSSubsystem::ReadDoors(FMassEntityHandle MazeEntity) const
{
	TArray<FMazeDoorView> Result;

	if (!FMazeDoorDefinition::bEnabled)
		return Result;

	const auto* Maze = FindFragment<FMazeGenerationFragment>(MazeEntity);

	if (!Maze)
		return Result;

	Result.Reserve(DoorEntities.Num());

	for (const FMassEntityHandle Entity : DoorEntities)
		if (const auto* Door = FindFragment<FMazeDoorFragment>(Entity);
		    Door && Door->Maze == MazeEntity && Door->MazeRevision == Maze->Revision)
			Result.Add({Door->Index, Door->Center, Door->SlideAxis, Door->Normal, Door->OpenAmount, Door->bWantsOpen});

	Result.Sort(
	    [](const FMazeDoorView& A, const FMazeDoorView& B)
	    {
		    return A.Index < B.Index;
	    });

	return Result;
}

TArray<uint8> UMazeECSSubsystem::ReadDoorTargets(FMassEntityHandle MazeEntity) const
{
	const TArray<FMazeDoorView> Doors = ReadDoors(MazeEntity);
	TArray<uint8> Result;

	Result.SetNumZeroed(Doors.Num());

	for (const FMazeDoorView& Door : Doors)
		if (Result.IsValidIndex(Door.Index))
			Result[Door.Index] = Door.bWantsOpen ? 1 : 0;

	return Result;
}

void UMazeECSSubsystem::ReceiveDoorTargets(FMassEntityHandle MazeEntity, TConstArrayView<uint8> Targets)
{
	if (!FMazeDoorDefinition::bEnabled || GetWorld()->GetNetMode() != NM_Client)
		return;

	for (const FMassEntityHandle Entity : DoorEntities)
		if (auto* Door = FindFragment<FMazeDoorFragment>(Entity);
		    Door && Door->Maze == MazeEntity && Targets.IsValidIndex(Door->Index))
		{
			Door->bWantsOpen = Targets[Door->Index] != 0;

			if (Door->bWantsOpen)
				Door->CloseDelayRemaining = FMazeDoorDefinition::CloseDelaySeconds;
		}
}

TSharedPtr<const FMazeInterior> UMazeECSSubsystem::BuildMazeLampLocations(FMassEntityHandle Entity) const
{
	const auto* Maze = FindFragment<FMazeGenerationFragment>(Entity);

	if (!Maze || !Maze->Data)
		return nullptr;

	return MakeShared<FMazeInterior>(FMazeInterior::Build(Maze->Data->Layout,
	                                                      FMazeSurface(),
	                                                      Maze->Cell,
	                                                      Maze->WallThickness,
	                                                      Maze->WallHeight,
	                                                      Maze->Seed,
	                                                      FIntRect(),
	                                                      true));
}

FMazeSessionFragment UMazeECSSubsystem::ReadSession() const
{
	const auto* Session = FindFragment<FMazeSessionFragment>(SessionEntity);

	return Session ? *Session : FMazeSessionFragment();
}

void UMazeECSSubsystem::SetSessionStarted(bool bStarted)
{
	if (auto* Session = FindFragment<FMazeSessionFragment>(SessionEntity))
		Session->bSessionStarted = bStarted;
}

void UMazeECSSubsystem::SetMenu(bool bOpen, bool bSettings)
{
	if (auto* Session = FindFragment<FMazeSessionFragment>(SessionEntity))
	{
		Session->bMenuOpen = bOpen;
		Session->bSettingsOpen = bOpen && bSettings;
	}
}

const FMazeExplorationFragment* UMazeECSSubsystem::ReadExploration(FMassEntityHandle Entity) const
{
	const auto* Exploration = FindFragment<FMazeExplorationFragment>(Entity);
	const auto* Maze = FindFragment<FMazeGenerationFragment>(ReadSession().Maze);

	return Exploration && Maze && Exploration->Maze == ReadSession().Maze && Exploration->Revision == Maze->Revision
	           ? Exploration
	           : nullptr;
}

void UMazeECSSubsystem::SetMapOpen(bool bOpen)
{
	if (auto* Session = FindFragment<FMazeSessionFragment>(SessionEntity))
		FMazeExplorationSystem::SetOpen(*Session, bOpen);
}

FMazeRoomFragment UMazeECSSubsystem::ReadRoom() const
{
	const auto* Room = FindFragment<FMazeRoomFragment>(SessionEntity);

	return Room ? *Room : FMazeRoomFragment();
}

void UMazeECSSubsystem::ReceiveRoom(const FMazeRoomFragment& Room)
{
	if (auto* Stored = FindFragment<FMazeRoomFragment>(SessionEntity))
		*Stored = Room;

	SetSessionStarted(Room.bStarted);
}

void UMazeECSSubsystem::OpenRoom()
{
	if (auto* Room = FindFragment<FMazeRoomFragment>(SessionEntity))
		Room->bActive = true;
}

bool UMazeECSSubsystem::AddRoomMember(int32 Id, const FString& Name, bool bHost)
{
	auto* Room = FindFragment<FMazeRoomFragment>(SessionEntity);

	return Room && FMazeRoomSystem::Add(*Room, Id, Name, bHost);
}

void UMazeECSSubsystem::RemoveRoomMember(int32 Id)
{
	if (auto* Room = FindFragment<FMazeRoomFragment>(SessionEntity))
		Room->Members.RemoveAll(
		    [Id](const auto& M)
		    {
			    return M.Id == Id;
		    });
}

bool UMazeECSSubsystem::StartRoom(int32 Requester)
{
	auto* Room = FindFragment<FMazeRoomFragment>(SessionEntity);

	if (!Room || !FMazeRoomSystem::Start(*Room, Requester))
		return false;

	SetSessionStarted(true);

	return true;
}

FVector UMazeECSSubsystem::RoomSpawn(int32 Id) const
{
	const auto Room = ReadRoom();
	const auto Maze = ReadMaze(ReadSession().Maze);
	const auto* Member = Room.Members.FindByPredicate(
	    [Id](const auto& M)
	    {
		    return M.Id == Id;
	    });

	return Maze.Data && Member && Maze.Data->PlayerStarts.IsValidIndex(Member->Slot)
	           ? Maze.Origin + Maze.Data->PlayerStarts[Member->Slot]
	           : FVector::ZeroVector;
}

bool UMazeECSSubsystem::IsSprintHeld(FMassEntityHandle Entity) const
{
	const auto* Input = FindFragment<FMazePlayerInputFragment>(Entity);

	return Input && Input->bSprintHeld;
}

void UMazeECSSubsystem::ReceivePlayer(FMassEntityHandle Entity, const FMazeVitals& Vitals, int32 Exit)
{
	if (auto* Stored = FindVitals(Entity))
		*Stored = Vitals;

	if (auto* Progress = FindFragment<FMazeProgressFragment>(Entity))
		Progress->ReachedExit = Exit;
}

void UMazeECSSubsystem::ClearInput(FMassEntityHandle Entity)
{
	if (auto* Input = FindFragment<FMazePlayerInputFragment>(Entity))
		*Input = FMazePlayerInputFragment();

	if (auto* Locomotion = FindFragment<FMazeLocomotionFragment>(Entity))
		Locomotion->bRunning = false;
}

FString UMazeECSSubsystem::RoomAdmissionError() const
{
	return FMazeRoomSystem::AdmissionError(ReadRoom());
}
