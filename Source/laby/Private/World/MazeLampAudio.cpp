#include "World/MazeLampAudio.h"
#include "Maze/MazeInterior.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWave.h"
#include "TimerManager.h"

namespace
{
	const FName LampTag(TEXT("MazeLampAudio"));
	constexpr float LampVolume = 0.025f;
	constexpr float AudibleRadius = 360.f;
	constexpr int32 PoolSize = 8;

	struct FLampAudioPool
	{
		TArray<FVector> Locations;
		TArray<TWeakObjectPtr<UAudioComponent>> Components;
		TArray<int32> Assigned;
	};
}

void MazeLampAudio::Stop(AActor& Owner)
{
	TInlineComponentArray<UAudioComponent*> Components;

	Owner.GetComponents(Components);

	for (auto* Audio : Components)
		if (Audio->ComponentHasTag(LampTag))
		{
			Owner.GetWorld()->GetTimerManager().ClearAllTimersForObject(Audio);
			Audio->Stop();
			Audio->DestroyComponent();
		}
}

void MazeLampAudio::Rebuild(AActor& Owner, const FMazeInterior& Interior)
{
	Stop(Owner);

	if (Owner.GetNetMode() == NM_DedicatedServer || Interior.LampLocations.IsEmpty())
		return;

	auto* Sound = LoadObject<USoundWave>(nullptr, TEXT("/Game/Audio/Ambience/S_LampHum.S_LampHum"));

	if (!Sound)
	{
		UE_LOG(LogTemp, Warning, TEXT("Missing S_LampHum; lamp audio disabled."));

		return;
	}

	const auto Pool = MakeShared<FLampAudioPool>();

	Pool->Locations = Interior.LampLocations;

	FSoundAttenuationSettings Attenuation;

	Attenuation.bAttenuate = true;
	Attenuation.bSpatialize = true;
	Attenuation.AttenuationShapeExtents = FVector(80.f, 0.f, 0.f);
	Attenuation.FalloffDistance = AudibleRadius - 80.f;
	Attenuation.bEnableOcclusion = true;
	Attenuation.OcclusionVolumeAttenuation = 0.1f;
	Attenuation.OcclusionLowPassFilterFrequency = 1000.f;

	for (int32 Index = 0; Index < PoolSize; ++Index)
	{
		auto* Audio = NewObject<UAudioComponent>(&Owner);

		Audio->ComponentTags.Add(LampTag);
		Audio->SetupAttachment(Owner.GetRootComponent());
		Audio->bAutoActivate = false;
		Audio->bAutoDestroy = false;
		Audio->bStopWhenOwnerDestroyed = true;
		Audio->bIsUISound = false;
		Audio->bAllowSpatialization = true;
		Audio->SetVolumeMultiplier(LampVolume);
		Audio->SetSound(Sound);
		Audio->AdjustAttenuation(Attenuation);
		Audio->RegisterComponent();
		Pool->Components.Add(Audio);
		Pool->Assigned.Add(INDEX_NONE);
	}

	// Engine timer only selects local presentation sources; no gameplay state.
	const TWeakObjectPtr<AActor> WeakOwner(&Owner);
	const auto Update = [Pool, WeakOwner]()
	{
		auto* Actor = WeakOwner.Get();

		if (!Actor)
			return;

		TArray<FVector> Listeners;

		for (auto It = Actor->GetWorld()->GetPlayerControllerIterator(); It; ++It)
			if (auto* Controller = It->Get(); Controller && Controller->IsLocalController())
			{
				FVector Position, Front, Right;
				Controller->GetAudioListenerPosition(Position, Front, Right);
				Listeners.Add(Position);
			}

		TArray<TPair<int32, double>> Nearby;

		for (int32 Index = 0; Index < Pool->Locations.Num(); ++Index)
		{
			const FVector Position = Actor->GetActorTransform().TransformPosition(Pool->Locations[Index]);
			double Distance = FMath::Square(double(AudibleRadius));

			for (const FVector& Listener : Listeners)
				Distance = FMath::Min(Distance, FVector::DistSquared(Position, Listener));

			if (Distance < FMath::Square(double(AudibleRadius)))
				Nearby.Emplace(Index, Distance);
		}

		Nearby.Sort(
		    [](const auto& A, const auto& B)
		    {
			    return A.Value < B.Value;
		    });
		TArray<int32> Wanted;

		for (int32 Index = 0; Index < FMath::Min(PoolSize, Nearby.Num()); ++Index)
			Wanted.Add(Nearby[Index].Key);

		for (int32 Slot = 0; Slot < Pool->Components.Num(); ++Slot)
			if (!Wanted.Contains(Pool->Assigned[Slot]))
			{
				if (auto* Audio = Pool->Components[Slot].Get())
					Audio->Stop();

				Pool->Assigned[Slot] = INDEX_NONE;
			}

		for (int32 Lamp : Wanted)
		{
			if (Pool->Assigned.Contains(Lamp))
				continue;

			const int32 Slot = Pool->Assigned.IndexOfByKey(INDEX_NONE);

			if (Slot == INDEX_NONE)
				break;

			if (auto* Audio = Pool->Components[Slot].Get())
			{
				Audio->SetRelativeLocation(Pool->Locations[Lamp]);
				Audio->FadeIn(0.3f);
				Pool->Assigned[Slot] = Lamp;
			}
		}
	};
	FTimerHandle Handle;

	Owner.GetWorld()->GetTimerManager().SetTimer(
	    Handle, FTimerDelegate::CreateWeakLambda(Pool->Components[0].Get(), Update), 0.2f, true);
}
