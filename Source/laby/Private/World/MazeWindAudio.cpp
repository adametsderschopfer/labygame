#include "World/MazeWindAudio.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Actor.h"
#include "Sound/SoundWave.h"

namespace
{
	constexpr float WindVolume = 0.4f;
	constexpr float WindFadeInSeconds = 3.f;
	const FName WindTag(TEXT("MazeWindAmbience"));
}

void MazeWindAudio::Start(AActor& Owner)
{
	if (Owner.GetNetMode() == NM_DedicatedServer)
		return;

	Stop(Owner);

	auto* Sound = LoadObject<USoundWave>(nullptr, TEXT("/Game/Audio/Ambience/S_CalmWindOutside.S_CalmWindOutside"));

	if (!Sound)
	{
		UE_LOG(LogTemp, Warning, TEXT("Missing Calm Wind Outside audio asset; ambience disabled."));

		return;
	}

	// Local presentation only. Unreal owns playback, looping and world pause.
	auto* Audio = NewObject<UAudioComponent>(&Owner);

	Audio->ComponentTags.Add(WindTag);
	Audio->bAutoActivate = false;
	Audio->bAutoDestroy = false;
	Audio->bStopWhenOwnerDestroyed = true;
	Audio->bAllowSpatialization = false;
	Audio->bIsUISound = false;
	Audio->SetVolumeMultiplier(WindVolume);
	Audio->SetSound(Sound);
	Audio->RegisterComponent();
	Audio->FadeIn(WindFadeInSeconds);
}

void MazeWindAudio::Stop(AActor& Owner)
{
	TInlineComponentArray<UAudioComponent*> Components;

	Owner.GetComponents(Components);

	for (UAudioComponent* Audio : Components)
	{
		if (Audio->ComponentHasTag(WindTag))
		{
			Audio->Stop();
			Audio->DestroyComponent();
		}
	}
}
