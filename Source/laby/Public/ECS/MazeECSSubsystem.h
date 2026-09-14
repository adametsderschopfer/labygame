#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Mass/EntityElementTypes.h"
#include "Mass/EntityHandle.h"
#include "MassEntityQuery.h"
#include "ECS/MazeECSFragments.h"
#include "MazeECSSubsystem.generated.h"

enum class EMazeInputAxis
{
	Forward,
	Right,
	Yaw,
	Pitch
};
enum class EMazeInputAction
{
	Sprint,
	Jump
};

// Game-thread ECS scheduling, lifecycle and engine bridge API for this world.
UCLASS()
class LABY_API UMazeECSSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual TStatId GetStatId() const override;
	FMassEntityHandle CreatePlayer();
	void DestroyPlayer(FMassEntityHandle Entity);
	FMazeVitals ReadVitals(FMassEntityHandle Entity) const;
	void SetLocomotion(FMassEntityHandle Entity, bool bRunning, bool bOnGround);
	bool SpendJumpStamina(FMassEntityHandle Entity);
	float ApplyDamage(FMassEntityHandle Entity, float Amount);
	void SetInputAxis(FMassEntityHandle Entity, EMazeInputAxis Axis, float Value);
	void SetInputAction(FMassEntityHandle Entity, EMazeInputAction Action, bool bPressed);
	void ClearPlayerInput();
	FMazePlayerCommandFragment ResolvePlayer(FMassEntityHandle Entity, const FMazePlayerPoseFragment& Pose);
	int32 ReadReachedExit(FMassEntityHandle Entity) const;
	FMassEntityHandle CreateMaze(int32 Seed, FVector Origin);
	void DestroyMaze(FMassEntityHandle Entity);
	void RegenerateMaze(FMassEntityHandle Entity, int32 Seed, FVector Origin);
	FMazeGenerationFragment ReadMaze(FMassEntityHandle Entity) const;
	FMazeSessionFragment ReadSession() const;
	void SetSessionStarted(bool bStarted);
	void SetMenu(bool bOpen, bool bSettings = false);
	void ToggleMinimap();

protected:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
	UPROPERTY()
	TObjectPtr<class UMassEntitySubsystem> MassSubsystem;
	TUniquePtr<FMassEntityQuery> VitalsQuery;
	TUniquePtr<FMassEntityQuery> GenerationQuery;
	TUniquePtr<FMassEntityQuery> InputQuery;
	FMassArchetypeHandle PlayerArchetype;
	FMassArchetypeHandle MazeArchetype;
	FMassEntityHandle SessionEntity;

	FMazeVitals* FindVitals(FMassEntityHandle Entity) const;
	void GeneratePending();
	void UpdateProgress(FMassEntityHandle Entity);
	template <typename T> T* FindFragment(FMassEntityHandle Entity) const;
};
