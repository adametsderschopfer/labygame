#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ECS/MazeVitals.h"
#include "ECS/MazeItems.h"
#include "Mass/EntityHandle.h"
#include "MazeCharacter.generated.h"

UCLASS()
class LABY_API AMazeCharacter : public ACharacter
{
	GENERATED_BODY()
public:
	AMazeCharacter();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool CanJumpInternal_Implementation() const override;
	virtual void OnJumped_Implementation() override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode = 0) override;
	virtual float TakeDamage(float DamageAmount,
	                         const FDamageEvent& DamageEvent,
	                         AController* EventInstigator,
	                         AActor* DamageCauser) override;
	FMazeVitals GetVitals() const;
	FMassEntityHandle GetPlayerEntity() const
	{
		return PlayerEntity;
	}

	void ClearLocalInput();
	int32 GetReachedExit() const;
	virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Equipment")
	TObjectPtr<class USpotLightComponent> HeadlampLight;

	UPROPERTY(ReplicatedUsing = OnRep_Items)
	FMazeItemsSnapshot ReplicatedItems;

	UFUNCTION()
	void OnRep_Items();
	void RefreshHeadlamp();

	UPROPERTY(ReplicatedUsing = OnRep_PlayerSnapshot)
	FMazeVitals ReplicatedVitals;

	UPROPERTY(ReplicatedUsing = OnRep_PlayerSnapshot)
	int32 ReplicatedExit = 0;

	UFUNCTION()
	void OnRep_PlayerSnapshot();
	UFUNCTION(Server, Reliable)
	void ServerSetSprint(bool bHeld);

	UPROPERTY(Transient)
	FMassEntityHandle PlayerEntity;

	UPROPERTY(Transient)
	TObjectPtr<class UMazeECSSubsystem> ECSSubsystem;

	void Forward(float Value);
	void Right(float Value);
	void LookUp(float Value);
	void Turn(float Value);
	void SprintStart();
	void SprintStop();
	void JumpStart();
	void JumpStop();
	void RestartMaze();
};
