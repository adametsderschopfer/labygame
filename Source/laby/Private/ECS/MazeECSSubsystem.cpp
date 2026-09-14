#include "ECS/MazeECSSubsystem.h"
#include "ECS/MazeVitalsSystem.h"
#include "ECS/MazeGameplaySystems.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"
#include "Engine/World.h"

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
	                                    FMazeProgressFragment::StaticStruct()};

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

	const UScriptStruct* SessionFragments[] = {FMazeSessionFragment::StaticStruct(), FMazeRoomFragment::StaticStruct()};

	SessionEntity = Manager.CreateEntity(Manager.CreateArchetype(MakeArrayView(SessionFragments)));
}

void UMazeECSSubsystem::Deinitialize()
{
	VitalsQuery.Reset();
	GenerationQuery.Reset();
	InputQuery.Reset();

	if (MassSubsystem && MassSubsystem->GetEntityManager().IsEntityValid(SessionEntity))
		MassSubsystem->GetMutableEntityManager().DestroyEntity(SessionEntity);

	SessionEntity = FMassEntityHandle();
	MazeArchetype = FMassArchetypeHandle();
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
	Super::Tick(DeltaSeconds);

	if (!MassSubsystem || !VitalsQuery || !GetWorld()->HasBegunPlay() || GetWorld()->IsPaused() ||
	    GetWorld()->GetNetMode() == NM_Client)
		return;

	auto Context = MassSubsystem->GetMutableEntityManager().CreateExecutionContext(DeltaSeconds);

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

	return Entity;
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
		else
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

	if (GetWorld()->GetNetMode() != NM_Client)
		if (const auto* Maze = FindFragment<FMazeGenerationFragment>(ReadSession().Maze))
			FMazeHazardSystem::Apply(*Maze, *StoredPose, *Vitals);

	*Command = FMazePlayerControlSystem::Resolve(*Input, *StoredPose, *Vitals, *Locomotion);

	if (GetWorld()->GetNetMode() != NM_Client)
		UpdateProgress(Entity);

	return *Command;
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
	}
}

void UMazeECSSubsystem::DestroyMaze(FMassEntityHandle Entity)
{
	if (auto* Session = FindFragment<FMazeSessionFragment>(SessionEntity); Session && Session->Maze == Entity)
		Session->Maze = FMassEntityHandle();

	DestroyPlayer(Entity);
}

FMazeGenerationFragment UMazeECSSubsystem::ReadMaze(FMassEntityHandle Entity) const
{
	const auto* Maze = FindFragment<FMazeGenerationFragment>(Entity);

	return Maze ? *Maze : FMazeGenerationFragment();
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

void UMazeECSSubsystem::ToggleMinimap()
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST

	if (auto* Session = FindFragment<FMazeSessionFragment>(SessionEntity); Session && !Session->bMenuOpen)
		Session->bMinimapVisible = !Session->bMinimapVisible;

#endif
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
