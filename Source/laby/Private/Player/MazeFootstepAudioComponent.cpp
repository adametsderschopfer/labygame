#include "Player/MazeFootstepAudioComponent.h"
#include "Player/MazeCharacter.h"
#include "ECS/MazeVitalsSystem.h"
#include "ECS/MazePlayerControlDefinition.h"
#include "Components/AudioComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWave.h"

namespace
{
	struct FMazeFootstepAudioDefinition
	{
		float WalkDistance = 180.f;
		float RunDistance = 240.f;
		float SneakDistance = 140.f;
		float FirstStepDistance = 35.f;
		float WalkVolume = 0.22f;
		float RunVolume = 0.28f;
		float SneakVolume = 0.12f;
		float MinimumSpeed = 15.f;
		float MaximumDelta = 0.15f;
	};
	constexpr FMazeFootstepAudioDefinition FootstepDefinition;
	constexpr int32 FootstepVariants = 6;
}

UMazeFootstepAudioComponent::UMazeFootstepAudioComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bTickEvenWhenPaused = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UMazeFootstepAudioComponent::BeginPlay()
{
	Super::BeginPlay();

	const auto* Character = Cast<AMazeCharacter>(GetOwner());

	if (!Character || Character->GetNetMode() == NM_DedicatedServer)
	{
		SetComponentTickEnabled(false);

		return;
	}

	AddTickPrerequisiteComponent(Character->GetCharacterMovement());

	for (const TCHAR* Action : {TEXT("Walk"), TEXT("Run"), TEXT("Sneak")})
	{
		for (int32 Variant = 1; Variant <= FootstepVariants; ++Variant)
		{
			const FString Name = FString::Printf(TEXT("S_Stone_%s_%02d"), Action, Variant);
			const FString Path = FString::Printf(TEXT("/Game/Audio/Footsteps/%s.%s"), *Name, *Name);
			auto* Sound = LoadObject<USoundWave>(nullptr, *Path);

			if (!Sound)
			{
				UE_LOG(LogTemp, Warning, TEXT("Missing footstep %s; run Scripts/import_footsteps.py."), *Path);
				Sounds.Reset();
				SetComponentTickEnabled(false);

				return;
			}

			Sounds.Add(Sound);
		}
	}

	Audio = NewObject<UAudioComponent>(GetOwner());
	Audio->bAutoActivate = false;
	Audio->bAutoDestroy = false;
	Audio->bStopWhenOwnerDestroyed = true;
	Audio->bAllowSpatialization = false;
	Audio->bIsUISound = false;
	Audio->RegisterComponent();
}

void UMazeFootstepAudioComponent::ResetPlayback()
{
	DistanceSinceStep = 0.f;
	bHasPreviousLocation = false;
	bFirstStep = true;

	if (Audio && Audio->IsPlaying())
		Audio->Stop();
}

void UMazeFootstepAudioComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	ResetPlayback();

	if (Audio)
		Audio->DestroyComponent();

	Audio = nullptr;
	Sounds.Reset();
	Super::EndPlay(Reason);
}

void UMazeFootstepAudioComponent::TickComponent(float DeltaTime,
                                                ELevelTick TickType,
                                                FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);

	const auto* Character = Cast<AMazeCharacter>(GetOwner());
	const auto* PC = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
	const auto* Movement = Character ? Character->GetCharacterMovement() : nullptr;

	if (!Audio || !Character || !Character->IsLocallyControlled() || !PC || PC->IsMoveInputIgnored() ||
	    UGameplayStatics::IsGamePaused(this) || !FMazeVitalsSystem::IsAlive(Character->GetVitals()) ||
	    !Movement->IsMovingOnGround() || DeltaTime <= 0.f || DeltaTime > FootstepDefinition.MaximumDelta ||
	    Movement->Velocity.Size2D() < FootstepDefinition.MinimumSpeed)
	{
		ResetPlayback();

		return;
	}

	const FVector Location = Character->GetActorLocation();

	if (!bHasPreviousLocation)
	{
		PreviousLocation = Location;
		bHasPreviousLocation = true;

		return;
	}

	const float Distance = FVector::Dist2D(Location, PreviousLocation);

	PreviousLocation = Location;

	const float Speed = Movement->Velocity.Size2D();

	// Ignore teleports/corrections and never emit a burst to catch up after a hitch.
	if (Distance > Speed * DeltaTime * 2.f + 10.f)
	{
		ResetPlayback();

		return;
	}

	const bool bSneaking = Movement->IsCrouching();
	const bool bRunning =
	    !bSneaking &&
	    Speed > (FMazePlayerControlDefinition::WalkSpeed + FMazePlayerControlDefinition::SprintSpeed) * 0.5f;
	const int32 Action = bSneaking ? 2 : (bRunning ? 1 : 0);
	const float StepDistance = bSneaking
	                               ? FootstepDefinition.SneakDistance
	                               : (bRunning ? FootstepDefinition.RunDistance : FootstepDefinition.WalkDistance);

	DistanceSinceStep += Distance;

	if (DistanceSinceStep < (bFirstStep ? FootstepDefinition.FirstStepDistance : StepDistance))
		return;

	DistanceSinceStep = 0.f;
	bFirstStep = false;

	const int32 Variant = LastVariant == INDEX_NONE
	                          ? Variation.RandRange(0, FootstepVariants - 1)
	                          : (LastVariant + Variation.RandRange(1, FootstepVariants - 1)) % FootstepVariants;

	LastVariant = Variant;
	Audio->Stop();
	Audio->SetSound(Sounds[Action * FootstepVariants + Variant]);
	Audio->SetVolumeMultiplier(bSneaking ? FootstepDefinition.SneakVolume
	                                     : (bRunning ? FootstepDefinition.RunVolume : FootstepDefinition.WalkVolume));
	Audio->SetPitchMultiplier(Variation.FRandRange(0.97f, 1.03f));
	Audio->Play();
}
