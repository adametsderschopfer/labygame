#include "MazeVitalsSubsystem.h"
#include "MazeVitalsSystem.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"
#include "Engine/World.h"

void UMazeVitalsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UMassEntitySubsystem>();
	Super::Initialize(Collection);
	MassSubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	check(MassSubsystem);
	auto& Manager = MassSubsystem->GetMutableEntityManager();
	const UScriptStruct* Fragments[] = { FMazeVitalsFragment::StaticStruct(), FMazeLocomotionFragment::StaticStruct() };
	PlayerArchetype = Manager.CreateArchetype(MakeArrayView(Fragments));
	VitalsQuery = MakeUnique<FMassEntityQuery>(Manager.AsShared());
	VitalsQuery->AddRequirement<FMazeVitalsFragment>(EMassFragmentAccess::ReadWrite);
	VitalsQuery->AddRequirement<FMazeLocomotionFragment>(EMassFragmentAccess::ReadOnly);
}

void UMazeVitalsSubsystem::Deinitialize()
{
	VitalsQuery.Reset();
	PlayerArchetype = FMassArchetypeHandle();
	MassSubsystem = nullptr;
	Super::Deinitialize();
}

bool UMazeVitalsSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UMazeVitalsSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMazeVitalsSubsystem, STATGROUP_Tickables);
}

void UMazeVitalsSubsystem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!MassSubsystem || !VitalsQuery || !GetWorld()->HasBegunPlay() || GetWorld()->IsPaused()) return;
	auto Context = MassSubsystem->GetMutableEntityManager().CreateExecutionContext(DeltaSeconds);
	VitalsQuery->ForEachEntityChunk(Context, [DeltaSeconds](FMassExecutionContext& Chunk)
	{
		auto Vitals = Chunk.GetMutableFragmentView<FMazeVitalsFragment>();
		const auto Locomotion = Chunk.GetFragmentView<FMazeLocomotionFragment>();
		for (int32 Index = 0; Index < Chunk.GetNumEntities(); ++Index)
			FMazeVitalsSystem::Update(Vitals[Index].Value, DeltaSeconds, Locomotion[Index].bRunning, Locomotion[Index].bOnGround);
	});
}

FMassEntityHandle UMazeVitalsSubsystem::CreatePlayer()
{
	check(IsInGameThread());
	return MassSubsystem ? MassSubsystem->GetMutableEntityManager().CreateEntity(PlayerArchetype) : FMassEntityHandle();
}

void UMazeVitalsSubsystem::DestroyPlayer(FMassEntityHandle Entity)
{
	check(IsInGameThread());
	if (MassSubsystem && MassSubsystem->GetEntityManager().IsEntityValid(Entity))
		MassSubsystem->GetMutableEntityManager().DestroyEntity(Entity);
}

FMazeVitals* UMazeVitalsSubsystem::FindVitals(FMassEntityHandle Entity) const
{
	check(IsInGameThread());
	if (!MassSubsystem || !MassSubsystem->GetEntityManager().IsEntityValid(Entity)) return nullptr;
	auto* Fragment = MassSubsystem->GetEntityManager().GetFragmentDataPtr<FMazeVitalsFragment>(Entity);
	return Fragment ? &Fragment->Value : nullptr;
}

FMazeVitals UMazeVitalsSubsystem::ReadVitals(FMassEntityHandle Entity) const
{
	if (const auto* Vitals = FindVitals(Entity)) return *Vitals;
	FMazeVitals Missing;
	Missing.Health = Missing.Stamina = 0.f;
	return Missing;
}

void UMazeVitalsSubsystem::SetLocomotion(FMassEntityHandle Entity, bool bRunning, bool bOnGround)
{
	check(IsInGameThread());
	if (!MassSubsystem || !MassSubsystem->GetEntityManager().IsEntityValid(Entity)) return;
	if (auto* Fragment = MassSubsystem->GetEntityManager().GetFragmentDataPtr<FMazeLocomotionFragment>(Entity))
	{
		Fragment->bRunning = bRunning;
		Fragment->bOnGround = bOnGround;
	}
}

bool UMazeVitalsSubsystem::SpendJumpStamina(FMassEntityHandle Entity)
{
	auto* Vitals = FindVitals(Entity);
	return Vitals && FMazeVitalsSystem::SpendJumpStamina(*Vitals);
}

float UMazeVitalsSubsystem::ApplyDamage(FMassEntityHandle Entity, float Amount)
{
	auto* Vitals = FindVitals(Entity);
	return Vitals ? FMazeVitalsSystem::Damage(*Vitals, Amount) : 0.f;
}
