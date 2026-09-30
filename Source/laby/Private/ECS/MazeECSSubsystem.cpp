#include "ECS/MazeECSSubsystem.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ECS/MazeVitalsSystem.h"
#include "ECS/MazeGameplaySystems.h"
#include "ECS/MazeExplorationSystem.h"
#include "ECS/MazeItemSystem.h"
#include "ECS/MazeSignal.h"
#include "ECS/MazeNoiseSystem.h"
#include "ECS/MazeDoorSystem.h"
#include "ECS/MazeWaterSystem.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"
#include "Engine/World.h"
#include "Maze/MazeInterior.h"
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
#include "ECS/MazeDevelopmentSystem.h"
#endif

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
	                                    FMazeNoiseFragment::StaticStruct(),
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
	NoiseQuery = MakeUnique<FMassEntityQuery>(Manager.AsShared());
	NoiseQuery->AddRequirement<FMazeNoiseFragment>(EMassFragmentAccess::ReadWrite);
	NoiseQuery->AddRequirement<FMazePlayerPoseFragment>(EMassFragmentAccess::ReadOnly);
	NoiseQuery->AddRequirement<FMazeVitalsFragment>(EMassFragmentAccess::ReadOnly);

	const UScriptStruct* DoorFragments[] = {FMazeDoorFragment::StaticStruct()};

	DoorArchetype = Manager.CreateArchetype(MakeArrayView(DoorFragments));

	const UScriptStruct* ItemFragments[] = {FMazeWorldItemFragment::StaticStruct()};

	WorldItemArchetype = Manager.CreateArchetype(MakeArrayView(ItemFragments));

	const UScriptStruct* SessionFragments[] = {FMazeSessionFragment::StaticStruct(), FMazeRoomFragment::StaticStruct()};

	SessionEntity = Manager.CreateEntity(Manager.CreateArchetype(MakeArrayView(SessionFragments)));
}

void UMazeECSSubsystem::Deinitialize()
{
	if (MassSubsystem)
		for (const FMassEntityHandle Entity : WorldItemEntities)
			if (MassSubsystem->GetEntityManager().IsEntityValid(Entity))
				MassSubsystem->GetMutableEntityManager().DestroyEntity(Entity);

	WorldItemEntities.Reset();

	if (MassSubsystem)
		for (const FMassEntityHandle Entity : DoorEntities)
			if (MassSubsystem->GetEntityManager().IsEntityValid(Entity))
				MassSubsystem->GetMutableEntityManager().DestroyEntity(Entity);

	DoorEntities.Reset();
	VitalsQuery.Reset();
	GenerationQuery.Reset();
	InputQuery.Reset();
	SignalQuery.Reset();
	NoiseQuery.Reset();

	if (MassSubsystem && MassSubsystem->GetEntityManager().IsEntityValid(SessionEntity))
		MassSubsystem->GetMutableEntityManager().DestroyEntity(SessionEntity);

	SessionEntity = FMassEntityHandle();
	MazeArchetype = FMassArchetypeHandle();
	DoorArchetype = FMassArchetypeHandle();
	WorldItemArchetype = FMassArchetypeHandle();
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

	if (!MassSubsystem || !VitalsQuery || !SignalQuery || !NoiseQuery || !GetWorld()->HasBegunPlay() ||
	    GetWorld()->IsPaused())
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

	const auto Session = ReadSession();
	const auto Maze = ReadMaze(Session.Maze);

	NoiseQuery->ForEachEntityChunk(
	    Context,
	    [DeltaSeconds, bAuthority, Session, Maze](FMassExecutionContext& Chunk)
	    {
		    auto Noises = Chunk.GetMutableFragmentView<FMazeNoiseFragment>();
		    const auto Poses = Chunk.GetFragmentView<FMazePlayerPoseFragment>();
		    const auto Vitals = Chunk.GetFragmentView<FMazeVitalsFragment>();

		    for (int32 Index = 0; Index < Chunk.GetNumEntities(); ++Index)
		    {
			    FMazeNoiseSystem::Synchronize(Noises[Index], Session.Maze, Maze);

			    if (bAuthority)
				    FMazeNoiseSystem::Update(Noises[Index],
				                             Poses[Index],
				                             DeltaSeconds,
				                             Session.bSessionStarted && Maze.Data && !Maze.bNeedsGeneration &&
				                                 FMazeVitalsSystem::IsAlive(Vitals[Index].Value));
			    else
				    Noises[Index].PresentationElapsed = FMath::Min(FMazeNoiseDefinition::TrailSeconds,
				                                                   Noises[Index].PresentationElapsed + DeltaSeconds);
		    }
	    });

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

	const bool bAccepted = Items && FMazeItemSystem::ToggleHeadlamp(
	                                    *Items,
	                                    Pose && Pose->bInputEnabled && FMazeVitalsSystem::IsAlive(ReadVitals(Entity)) &&
	                                        (!Room.bActive || Room.bStarted));

	if (bAccepted)
		EmitNoise(Entity, EMazeNoiseSource::Headlamp);

	return bAccepted;
}

bool UMazeECSSubsystem::SelectInventorySlot(FMassEntityHandle Entity, int32 Slot)
{
	if (GetWorld()->GetNetMode() == NM_Client || GetWorld()->IsPaused())
		return false;

	auto* Items = FindFragment<FMazeItemsFragment>(Entity);
	const auto* Pose = FindFragment<FMazePlayerPoseFragment>(Entity);
	const auto Room = ReadRoom();

	return Items &&
	       FMazeItemSystem::SelectSlot(*Items,
	                                   Slot,
	                                   Pose && Pose->bInputEnabled && FMazeVitalsSystem::IsAlive(ReadVitals(Entity)) &&
	                                       (!Room.bActive || Room.bStarted));
}

bool UMazeECSSubsystem::CycleInventorySlot(FMassEntityHandle Entity, int32 Step)
{
	if (Step != -1 && Step != 1)
		return false;

	const auto* Items = FindFragment<FMazeItemsFragment>(Entity);

	return Items && SelectInventorySlot(Entity, FMazeItemSystem::NextSlot(Items->Value.SelectedSlot, Step));
}

void UMazeECSSubsystem::ReceiveItems(FMassEntityHandle Entity, const FMazeItemsSnapshot& Snapshot)
{
	if (GetWorld()->GetNetMode() != NM_Client)
		return;

	if (auto* Items = FindFragment<FMazeItemsFragment>(Entity))
		Items->Value = Snapshot;
}

FMazeWorldItemView UMazeECSSubsystem::ReadWorldItem(FMassEntityHandle Maze, int32 ItemId) const
{
	const auto* Generation = FindFragment<FMazeGenerationFragment>(Maze);

	for (const FMassEntityHandle Entity : WorldItemEntities)
		if (const auto* Item = FindFragment<FMazeWorldItemFragment>(Entity);
		    Item && Generation && Item->Maze == Maze && Item->MazeRevision == Generation->Revision &&
		    Item->Id == ItemId)
			return {Item->Id, Item->Kind, Item->Location, Item->bAvailable};

	return FMazeWorldItemView();
}

bool UMazeECSSubsystem::PickupWorldItem(FMassEntityHandle Player,
                                        FMassEntityHandle Maze,
                                        int32 ItemId,
                                        const FVector& EyeLocation,
                                        const FVector& AimDirection)
{
	if (GetWorld()->GetNetMode() == NM_Client || GetWorld()->IsPaused() || ReadSession().Maze != Maze)
		return false;

	auto* Items = FindFragment<FMazeItemsFragment>(Player);
	const auto* Pose = FindFragment<FMazePlayerPoseFragment>(Player);
	const auto* Vitals = FindVitals(Player);
	const auto Room = ReadRoom();
	const auto View = ReadWorldItem(Maze, ItemId);

	if (!Items || !Pose || !Vitals || !Pose->bInputEnabled || !FMazeVitalsSystem::IsAlive(*Vitals) ||
	    (Room.bActive && !Room.bStarted) || !View.bAvailable || View.Id != ItemId ||
	    FVector::DistSquared(EyeLocation, Pose->Location) > FMath::Square(FMazeItemDefinition::EyePoseTolerance))
		return false;

	for (const FMassEntityHandle Entity : WorldItemEntities)
		if (auto* Item = FindFragment<FMazeWorldItemFragment>(Entity); Item && Item->Maze == Maze && Item->Id == ItemId)
			return FMazeItemSystem::Pickup(*Item, *Items, EyeLocation, AimDirection, true);

	return false;
}

bool UMazeECSSubsystem::DropSelectedItem(FMassEntityHandle Player,
                                         FMassEntityHandle MazeEntity,
                                         const FVector& Location)
{
	if (GetWorld()->GetNetMode() == NM_Client || GetWorld()->IsPaused() || ReadSession().Maze != MazeEntity ||
	    Location.ContainsNaN())
		return false;

	auto* Items = FindFragment<FMazeItemsFragment>(Player);
	const auto* Pose = FindFragment<FMazePlayerPoseFragment>(Player);
	const auto* Progress = FindFragment<FMazeProgressFragment>(Player);
	const auto* Vitals = FindVitals(Player);
	const auto* Maze = FindFragment<FMazeGenerationFragment>(MazeEntity);
	const auto Room = ReadRoom();

	if (!Items || !Pose || !Progress || !Vitals || !Maze || !Maze->Data || !Pose->bInputEnabled ||
	    !FMazeVitalsSystem::IsAlive(*Vitals) || Progress->Maze != MazeEntity ||
	    Progress->MazeRevision != Maze->Revision || (Room.bActive && !Room.bStarted) ||
	    FVector::DistSquared(Location, Pose->Location) > FMath::Square(FMazeItemDefinition::DropRangeCm))
		return false;

	for (const FMassEntityHandle Entity : WorldItemEntities)
		if (auto* Item = FindFragment<FMazeWorldItemFragment>(Entity);
		    Item && Item->Maze == MazeEntity && Item->MazeRevision == Maze->Revision &&
		    Items->Value.Items.IsValidIndex(Items->Value.SelectedSlot) &&
		    Item->Kind == Items->Value.Items[Items->Value.SelectedSlot].Kind)
			return FMazeItemSystem::Drop(*Items, *Item, Location, true);

	return false;
}

void UMazeECSSubsystem::ReceiveWorldItemSnapshot(
    FMassEntityHandle MazeEntity, uint32 Revision, const FVector& Location, bool bAvailable, int32 ItemId)
{
	if (GetWorld()->GetNetMode() != NM_Client)
		return;

	const auto* Maze = FindFragment<FMazeGenerationFragment>(MazeEntity);

	if (!Maze || !Maze->Data || Maze->Revision != Revision || Location.ContainsNaN())
		return;

	for (const FMassEntityHandle Entity : WorldItemEntities)
		if (auto* Item = FindFragment<FMazeWorldItemFragment>(Entity);
		    Item && Item->Maze == MazeEntity && Item->MazeRevision == Revision && Item->Id == ItemId)
		{
			Item->Location = Location;
			Item->bAvailable = bAvailable;
		}
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

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
bool UMazeECSSubsystem::SetDevelopmentInfiniteStamina(FMassEntityHandle Entity, bool bEnabled)
{
	if (GetWorld()->GetNetMode() != NM_Standalone || !ReadSession().bSessionStarted)
		return false;

	if (auto* Vitals = FindVitals(Entity))
	{
		FMazeVitalsSystem::SetDevelopmentInfiniteStamina(*Vitals, bEnabled);

		return true;
	}

	return false;
}

bool UMazeECSSubsystem::SetDevelopmentImmortal(FMassEntityHandle Entity, bool bEnabled)
{
	if (GetWorld()->GetNetMode() != NM_Standalone || !ReadSession().bSessionStarted)
		return false;

	if (auto* Vitals = FindVitals(Entity))
		return FMazeVitalsSystem::SetDevelopmentImmortal(*Vitals, bEnabled);

	return false;
}

bool UMazeECSSubsystem::ShouldDevelopmentRescue(FMassEntityHandle Entity) const
{
	const auto* Vitals = FindVitals(Entity);

	return GetWorld()->GetNetMode() == NM_Standalone && ReadSession().bSessionStarted && Vitals &&
	       FMazeVitalsSystem::ShouldDevelopmentRescue(*Vitals);
}

bool UMazeECSSubsystem::RequestDevelopmentTeleport(FMassEntityHandle Entity,
                                                   const FVector& Position,
                                                   bool bExit,
                                                   FMazeDevelopmentTeleport& Out) const
{
	const auto* Vitals = FindVitals(Entity);

	if (GetWorld()->GetNetMode() != NM_Standalone || GetWorld()->IsPaused() || !ReadSession().bSessionStarted ||
	    !Vitals || !FMazeVitalsSystem::IsAlive(*Vitals))
		return false;

	const auto* Maze = FindFragment<FMazeGenerationFragment>(ReadSession().Maze);

	return Maze && FMazeDevelopmentSystem::Teleport(*Maze, Position, bExit, Out);
}

#endif

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
                                                            const FMazePlayerPoseFragment& Pose,
                                                            const FVector& FeetLocation,
                                                            const FVector& EyeLocation)
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
	const FMazeWaterExposure Water =
	    Maze ? FMazeWaterSystem::Evaluate(*Maze, FeetLocation, EyeLocation) : FMazeWaterExposure();

	if (auto* Exploration = FindFragment<FMazeExplorationFragment>(Entity); Exploration && Maze)
		FMazeExplorationSystem::Update(
		    *Exploration, MazeEntity, *Maze, *StoredPose, FMazeVitalsSystem::IsAlive(*Vitals));

	if (GetWorld()->GetNetMode() != NM_Client)
	{
		if (Maze)
			FMazeHazardSystem::Apply(*Maze, *StoredPose, *Vitals);

		FMazeWaterSystem::ApplySubmersion(Water, *Vitals);
	}

	*Command = FMazePlayerControlSystem::Resolve(*Input, *StoredPose, *Vitals, *Locomotion, Water.bWading);

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
	auto* Vitals = FindVitals(Entity);
	const auto Session = ReadSession();
	const auto Room = ReadRoom();
	const bool bAllowed = Pose && Pose->bInputEnabled && Session.bSessionStarted && (!Room.bActive || Room.bStarted);

	if (!Signal || !Pose || !Vitals ||
	    !FMazeSignalSystem::Request(*Signal, *Vitals, Pose->Location, Pose->Forward, bAllowed))
		return false;

	OutSignal = Signal->Value;
	EmitNoise(Entity, EMazeNoiseSource::Whistle);

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

void UMazeECSSubsystem::EmitNoise(FMassEntityHandle Entity, EMazeNoiseSource Source)
{
	if (GetWorld()->GetNetMode() == NM_Client || GetWorld()->IsPaused())
		return;

	auto* Noise = FindFragment<FMazeNoiseFragment>(Entity);
	const auto Session = ReadSession();
	const auto* Maze = FindFragment<FMazeGenerationFragment>(Session.Maze);

	if (!Noise || !Maze || !Maze->Data || Maze->bNeedsGeneration || !Session.bSessionStarted)
		return;

	FMazeNoiseSystem::Synchronize(*Noise, Session.Maze, *Maze);
	FMazeNoiseSystem::Emit(*Noise, Source);
}

FMazeNoiseSnapshot UMazeECSSubsystem::ReadNoise(FMassEntityHandle Entity) const
{
	const auto* Noise = FindFragment<FMazeNoiseFragment>(Entity);
	const auto Session = ReadSession();
	const auto* Maze = FindFragment<FMazeGenerationFragment>(Session.Maze);

	if (!Noise || !Maze || !Maze->Data || Maze->bNeedsGeneration || Noise->Maze != Session.Maze ||
	    Noise->Value.MazeRevision != Maze->Revision || Noise->Value.MazeSeed != Maze->Seed)
		return {};

	return FMazeNoiseSystem::Snapshot(*Noise, false);
}

FMazeNoiseSnapshot UMazeECSSubsystem::ReadNoiseForReplication(FMassEntityHandle Entity) const
{
	const auto* Noise = FindFragment<FMazeNoiseFragment>(Entity);

	if (!Noise || ReadNoise(Entity).MazeRevision == 0)
		return {};

	return FMazeNoiseSystem::Snapshot(*Noise, true);
}

bool UMazeECSSubsystem::ReceiveNoise(FMassEntityHandle Entity, const FMazeNoiseSnapshot& Snapshot)
{
	if (GetWorld()->GetNetMode() != NM_Client)
		return false;

	auto* Noise = FindFragment<FMazeNoiseFragment>(Entity);
	const auto Session = ReadSession();
	const auto* Maze = FindFragment<FMazeGenerationFragment>(Session.Maze);

	if (!Noise || !Maze || !Maze->Data || Maze->bNeedsGeneration || Snapshot.MazeRevision != Maze->Revision ||
	    Snapshot.MazeSeed != Maze->Seed)
		return false;

	if (!FMazeNoiseSystem::Receive(*Noise, Snapshot))
		return false;

	Noise->Maze = Session.Maze;

	return true;
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
		RebuildWorldItems(Entity);
	}
}

void UMazeECSSubsystem::DestroyMaze(FMassEntityHandle Entity)
{
	DestroyDoors(Entity);
	DestroyWorldItems(Entity);

	if (auto* Session = FindFragment<FMazeSessionFragment>(SessionEntity); Session && Session->Maze == Entity)
		Session->Maze = FMassEntityHandle();

	DestroyPlayer(Entity);
}

void UMazeECSSubsystem::DestroyWorldItems(FMassEntityHandle MazeEntity)
{
	if (!MassSubsystem)
		return;

	auto& Manager = MassSubsystem->GetMutableEntityManager();

	for (int32 Index = WorldItemEntities.Num() - 1; Index >= 0; --Index)
	{
		const FMassEntityHandle Entity = WorldItemEntities[Index];
		const auto* Item = FindFragment<FMazeWorldItemFragment>(Entity);

		if (!Item || Item->Maze == MazeEntity)
		{
			if (Manager.IsEntityValid(Entity))
				Manager.DestroyEntity(Entity);

			WorldItemEntities.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		}
	}
}

void UMazeECSSubsystem::RebuildWorldItems(FMassEntityHandle MazeEntity)
{
	DestroyWorldItems(MazeEntity);

	const auto* Maze = FindFragment<FMazeGenerationFragment>(MazeEntity);

	if (!MassSubsystem || !Maze || !Maze->Data)
		return;

	// Copy values before structural entity creation; fragment storage may move.
	const uint32 Revision = Maze->Revision;
	const FVector Start = Maze->Origin + Maze->Data->Start + FMazeItemDefinition::StartOffset();

	for (int32 Id : {FMazeItemDefinition::HeadlampWorldId, FMazeItemDefinition::CardWorldId})
	{
		const FMassEntityHandle Entity = MassSubsystem->GetMutableEntityManager().CreateEntity(WorldItemArchetype);
		auto* Item = FindFragment<FMazeWorldItemFragment>(Entity);

		Item->Maze = MazeEntity;
		Item->MazeRevision = Revision;
		Item->Id = Id;
		Item->Kind = Id == FMazeItemDefinition::CardWorldId ? EMazeItemKind::AccessCard : EMazeItemKind::Headlamp;
		Item->Location =
		    Start + (Id == FMazeItemDefinition::CardWorldId ? FVector(0.f, 48.f, -8.f) : FVector::ZeroVector);
		WorldItemEntities.Add(Entity);
	}
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

	const auto* Maze = FindFragment<FMazeGenerationFragment>(MazeEntity);

	if (!MassSubsystem || !Maze || !Maze->Data)
		return;

	static const FVector Directions[] = {
	    FVector(0.f, -1.f, 0.f), FVector(1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), FVector(-1.f, 0.f, 0.f)};
	const TArray<FMazeDoorway> Doorways = Maze->Data->Layout.Doorways();
	const int32 Seed = Maze->Seed;
	const uint32 Revision = Maze->Revision;
	const FVector Origin = Maze->Origin;
	const float Cell = Maze->Cell;
	auto& Manager = MassSubsystem->GetMutableEntityManager();

	DoorEntities.Reserve(DoorEntities.Num() + Doorways.Num());

	for (int32 Index = 0; Index < Doorways.Num(); ++Index)
	{
		const FMazeDoorway& Doorway = Doorways[Index];
		const FVector Normal = Directions[Doorway.Direction];
		const FVector SlideAxis(-Normal.Y, Normal.X, 0.f);
		const FVector LocalCenter =
		    FVector((Doorway.Cell.X + 0.5f) * Cell, (Doorway.Cell.Y + 0.5f) * Cell, 0.f) + Normal * (Cell * 0.5f);
		const FMassEntityHandle Entity = Manager.CreateEntity(DoorArchetype);
		auto& Door = Manager.GetFragmentDataChecked<FMazeDoorFragment>(Entity);

		Door.Maze = MazeEntity;
		Door.MazeRevision = Revision;
		Door.Index = Index;
		Door.Center = Origin + LocalCenter;
		Door.SlideAxis = SlideAxis;
		Door.Normal = Normal;
		Door.bRequiresCard = FMazeDoorSystem::IsCardDoor(Seed, Index);
		DoorEntities.Add(Entity);
	}
}

TArray<FMazeDoorView> UMazeECSSubsystem::ReadDoors(FMassEntityHandle MazeEntity) const
{
	TArray<FMazeDoorView> Result;

	const auto* Maze = FindFragment<FMazeGenerationFragment>(MazeEntity);

	if (!Maze)
		return Result;

	Result.Reserve(DoorEntities.Num());

	for (const FMassEntityHandle Entity : DoorEntities)
		if (const auto* Door = FindFragment<FMazeDoorFragment>(Entity);
		    Door && Door->Maze == MazeEntity && Door->MazeRevision == Maze->Revision)
			Result.Add({Door->Index,
			            Door->Center,
			            Door->SlideAxis,
			            Door->Normal,
			            Door->OpenAmount,
			            Door->bWantsOpen,
			            Door->SwingSign,
			            Door->bRequiresCard,
			            Door->bUnlocked});

	Result.Sort(
	    [](const FMazeDoorView& A, const FMazeDoorView& B)
	    {
		    return A.Index < B.Index;
	    });

	return Result;
}

TArray<uint8> UMazeECSSubsystem::ReadDoorStates(FMassEntityHandle MazeEntity) const
{
	const TArray<FMazeDoorView> Doors = ReadDoors(MazeEntity);
	TArray<uint8> Result;

	Result.SetNumZeroed(Doors.Num() * 2);

	for (const FMazeDoorView& Door : Doors)
		if (Result.IsValidIndex(Door.Index * 2 + 1))
		{
			Result[Door.Index * 2 + 1] = Door.bUnlocked ? 1 : 0;
			Result[Door.Index * 2] =
			    static_cast<uint8>(FMath::RoundToInt(FMath::Clamp(Door.OpenAmount, 0.f, 1.f) * 63.f)) |
			    (Door.SwingSign < 0 ? 0x40 : 0) | (Door.bWantsOpen ? 0x80 : 0);
		}

	return Result;
}

bool UMazeECSSubsystem::AdvanceDoor(FMassEntityHandle MazeEntity,
                                    int32 DoorIndex,
                                    float DeltaSeconds,
                                    float MaxSafeAmount)
{
	if (GetWorld()->GetNetMode() == NM_Client || GetWorld()->IsPaused() || !DoorEntities.IsValidIndex(DoorIndex))
		return false;

	auto* Door = FindFragment<FMazeDoorFragment>(DoorEntities[DoorIndex]);
	const auto* Maze = FindFragment<FMazeGenerationFragment>(MazeEntity);

	if (!Door || !Maze || !Maze->Data || Door->Maze != MazeEntity || Door->MazeRevision != Maze->Revision ||
	    Door->Index != DoorIndex)
		return false;

	FMazeDoorSystem::Advance(*Door, DeltaSeconds, MaxSafeAmount);

	return true;
}

bool UMazeECSSubsystem::ToggleDoor(FMassEntityHandle Player, int32 DoorIndex, const FVector& Eye, const FVector& Aim)
{
	if (GetWorld()->GetNetMode() == NM_Client || GetWorld()->IsPaused() || !DoorEntities.IsValidIndex(DoorIndex))
		return false;

	const auto* Vitals = FindVitals(Player);
	const auto* Progress = FindFragment<FMazeProgressFragment>(Player);
	const auto* Pose = FindFragment<FMazePlayerPoseFragment>(Player);
	auto* Door = FindFragment<FMazeDoorFragment>(DoorEntities[DoorIndex]);
	const auto* Maze = Door ? FindFragment<FMazeGenerationFragment>(Door->Maze) : nullptr;

	if (!Vitals || !FMazeVitalsSystem::IsAlive(*Vitals) || !Progress || !Pose || !Pose->bInputEnabled || !Door ||
	    Door->Index != DoorIndex || !Maze || !Maze->Data || Door->MazeRevision != Maze->Revision ||
	    Progress->Maze != Door->Maze || (ReadRoom().bActive && !ReadRoom().bStarted) ||
	    !FMazeDoorSystem::CanInteract(
	        *Door, Eye, Aim, FMazeRoomDefinition::OpeningWidth(Maze->Cell - Maze->WallThickness), Maze->WallThickness))
		return false;

	const auto* Items = FindFragment<FMazeItemsFragment>(Player);

	if (!FMazeDoorSystem::TryToggle(*Door, Pose->Location, Items && FMazeItemSystem::IsCardSelected(Items->Value)))
		return false;

	EmitNoise(Player, EMazeNoiseSource::Door);

	return true;
}

void UMazeECSSubsystem::ReceiveDoorStates(FMassEntityHandle MazeEntity, uint32 Revision, TConstArrayView<uint8> States)
{
	if (GetWorld()->GetNetMode() != NM_Client)
		return;

	const auto* Maze = FindFragment<FMazeGenerationFragment>(MazeEntity);

	if (!Maze || !Maze->Data || Maze->Revision != Revision || States.Num() != ReadDoors(MazeEntity).Num() * 2)
		return;

	for (const FMassEntityHandle Entity : DoorEntities)
		if (auto* Door = FindFragment<FMazeDoorFragment>(Entity); Door && Door->Maze == MazeEntity &&
		                                                          Door->MazeRevision == Revision &&
		                                                          States.IsValidIndex(Door->Index * 2 + 1))
		{
			Door->bWantsOpen = (States[Door->Index * 2] & 0x80) != 0;
			Door->SwingSign = (States[Door->Index * 2] & 0x40) != 0 ? -1 : 1;
			Door->OpenAmount = (States[Door->Index * 2] & 0x3f) / 63.f;
			Door->PendingSwingSign = 0;
			Door->bUnlocked = States[Door->Index * 2 + 1] != 0;
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
