#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MazeFootstepAudioComponent.generated.h"

class UAudioComponent;
class USoundWave;

// Local audio consumes accepted ECS step sequences; owns only playback resources/caches.
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

	uint32 LastStepSequence = 0;
	uint32 LastMazeRevision = 0;
	int32 LastMazeSeed = 0;
	bool bNeedsBaseline = true;
	int32 LastVariant = INDEX_NONE;
	FRandomStream Variation{29873};
};
