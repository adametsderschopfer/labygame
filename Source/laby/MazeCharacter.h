#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MazeVitals.h"
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
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	FMazeVitals GetVitals() const;
	int32 GetReachedExit() const;
	virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
private:
	UPROPERTY(Transient) FMassEntityHandle PlayerEntity;
	UPROPERTY(Transient) TObjectPtr<class UMazeECSSubsystem> ECSSubsystem;
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
