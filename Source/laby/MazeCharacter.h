#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MazeVitals.h"
#include "MazeCharacter.generated.h"

UCLASS()
class LABY_API AMazeCharacter : public ACharacter
{
	GENERATED_BODY()
public:
	AMazeCharacter();
	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	const FMazeVitals& GetVitals() const { return Vitals; }
	virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
private:
	UPROPERTY() FMazeVitals Vitals;
	UPROPERTY() bool bSprintRequested = false;
	void Forward(float Value);
	void Right(float Value);
	void LookUp(float Value);
	void Turn(float Value);
	void SprintStart();
	void SprintStop();
	void RestartMaze();
};
