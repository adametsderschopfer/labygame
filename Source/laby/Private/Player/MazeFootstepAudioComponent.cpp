#include "Player/MazeFootstepAudioComponent.h"
#include "World/MazeLocationSubsystem.h"
#include "Player/MazeCharacter.h"
#include "ECS/MazeVitalsSystem.h"
#include "ECS/MazeECSSubsystem.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWave.h"

namespace
{
	struct FMazeFootstepAudioDefinition
	{
		float WalkVolume = 0.22f;
		float RunVolume = 0.28f;
		float SneakVolume = 0.12f;
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
}

void UMazeFootstepAudioComponent::InitializeAudio()
{
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
	bNeedsBaseline = true;

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
	const auto* LocationResources = GetWorld()->GetSubsystem<UMazeLocationSubsystem>();

	if (!Audio && Character && Character->IsLocallyControlled() &&
	    (!LocationResources || (LocationResources->AreAssetsReady() && LocationResources->GetFailure().IsEmpty())))
		InitializeAudio();

	if (!Audio || !Character || !Character->IsLocallyControlled() || !PC || PC->IsMoveInputIgnored() ||
	    UGameplayStatics::IsGamePaused(this) || !FMazeVitalsSystem::IsAlive(Character->GetVitals()) ||
	    !Movement->IsMovingOnGround() || DeltaTime <= 0.f)
	{
		ResetPlayback();

		return;
	}

	const auto* ECS = GetWorld()->GetSubsystem<UMazeECSSubsystem>();
	const auto Noise = ECS ? ECS->ReadNoise(Character->GetPlayerEntity()) : FMazeNoiseSnapshot();

	if (Noise.MazeRevision == 0)
		return;

	if (bNeedsBaseline || LastMazeRevision != Noise.MazeRevision || LastMazeSeed != Noise.MazeSeed)
	{
		LastMazeRevision = Noise.MazeRevision;
		LastMazeSeed = Noise.MazeSeed;
		LastStepSequence = Noise.StepSequence;
		bNeedsBaseline = false;

		return;
	}

	if (LastStepSequence == Noise.StepSequence)
		return;

	LastStepSequence = Noise.StepSequence;

	if (Noise.StepSequence == 0 || Noise.Footprints.IsEmpty() || Noise.Footprints.Last().RemainingSeconds <= 0.f)
		return;

	// Consume the latest accepted event once; never replay a burst after a network/hitch gap.
	const bool bSneaking = Noise.StepType == EMazeFootstepType::Crouch;
	const bool bRunning = Noise.StepType == EMazeFootstepType::Run;
	const int32 Action = bSneaking ? 2 : bRunning ? 1 : 0;
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
