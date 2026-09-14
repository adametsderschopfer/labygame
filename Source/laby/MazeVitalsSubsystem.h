#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Mass/EntityElementTypes.h"
#include "Mass/EntityHandle.h"
#include "MassEntityQuery.h"
#include "MazeVitals.h"
#include "MazeVitalsSubsystem.generated.h"

USTRUCT()
struct FMazeVitalsFragment : public FMassFragment
{
	GENERATED_BODY()
	UPROPERTY() FMazeVitals Value;
};

USTRUCT()
struct FMazeLocomotionFragment : public FMassFragment
{
	GENERATED_BODY()
	UPROPERTY() bool bRunning = false;
	UPROPERTY() bool bOnGround = false;
};

// ECS system for this world's Mass entities. The Actor bridge never owns vitals.
// Synchronous game-thread execution keeps jump/damage and HUD reads consistent.
UCLASS()
class LABY_API UMazeVitalsSubsystem : public UTickableWorldSubsystem
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
protected:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
private:
	UPROPERTY() TObjectPtr<class UMassEntitySubsystem> MassSubsystem;
	TUniquePtr<FMassEntityQuery> VitalsQuery;
	FMassArchetypeHandle PlayerArchetype;
	FMazeVitals* FindVitals(FMassEntityHandle Entity) const;
};
