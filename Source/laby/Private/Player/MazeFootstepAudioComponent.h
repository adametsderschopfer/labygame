#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MazeFootstepAudioComponent.generated.h"

class UAudioComponent;
class USoundWave;

// Per-pawn local presentation only; never produces gameplay noise or AI events.
UCLASS(Transient)
class UMazeFootstepAudioComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UMazeFootstepAudioComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime,
	                           ELevelTick TickType,
	                           FActorComponentTickFunction* TickFunction) override;
	void ResetPlayback();

private:
	void InitializeAudio();

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Audio;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundWave>> Sounds;

	FVector PreviousLocation = FVector::ZeroVector;
	float DistanceSinceStep = 0.f;
	bool bHasPreviousLocation = false;
	bool bFirstStep = true;
	int32 LastVariant = INDEX_NONE;
	FRandomStream Variation{29873};
};
