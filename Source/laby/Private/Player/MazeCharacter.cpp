#include "Player/MazeCharacter.h"
#include "Player/MazeFootstepAudioComponent.h"
#include "Player/MazePlayerController.h"
#include "ECS/MazeECSSubsystem.h"
#include "ECS/MazeVitalsSystem.h"
#include "ECS/MazeItemSystem.h"
#include "ECS/MazePlayerControlDefinition.h"
#include "Components/SpotLightComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"

namespace
{
	// Cosmetic only: the cosine profile starts and finishes at zero vertical speed.
	constexpr float StanceTransitionSeconds = 0.55f;

	void TraceStanceCamera(AMazeCharacter& Character, const TCHAR* Stage, float HeightAdjust = 0.f)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

		if (!Character.IsLocallyControlled())
			return;

		if (const auto* Camera = Character.FindComponentByClass<UCameraComponent>())
			UE_LOG(LogTemp,
			       Log,
			       TEXT("[CrouchTrace] %s %s time=%.4f crouched=%d ground=%d keepBase=%d adjust=%.3f capsuleHalf=%.3f "
			            "actorZ=%.3f cameraLocalZ=%.3f cameraWorldZ=%.3f"),
			       *Character.GetName(),
			       Stage,
			       Character.GetWorld()->GetTimeSeconds(),
			       Character.GetCharacterMovement()->IsCrouching(),
			       Character.GetCharacterMovement()->IsMovingOnGround(),
			       Character.GetCharacterMovement()->bCrouchMaintainsBaseLocation,
			       HeightAdjust,
			       Character.GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(),
			       Character.GetActorLocation().Z,
			       Camera->GetRelativeLocation().Z,
			       Camera->GetComponentLocation().Z);

#endif
	}

	void SmoothStanceCamera(AMazeCharacter& Character, float DeltaSeconds)
	{
		if (!Character.IsLocallyControlled())
			return;

		auto* Camera = Character.FindComponentByClass<UCameraComponent>();
		const auto* Defaults = Character.GetClass()->GetDefaultObject<AMazeCharacter>();
		const auto* DefaultCamera = Defaults->FindComponentByClass<UCameraComponent>();

		if (!Camera || !DefaultCamera)
			return;

		const float HeightAdjust = Defaults->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() -
		                           Character.GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
		FVector Target = DefaultCamera->GetRelativeLocation();

		Target.Z -= HeightAdjust;

		FVector Location = Camera->GetRelativeLocation();
		const float Error = Location.Z - Target.Z;
		const float Remaining = FMath::Abs(Error);
		const auto* Movement = Character.GetCharacterMovement();
		const float CapsuleTravel = FMath::Max(
		    0.f, Defaults->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - Movement->GetCrouchedHalfHeight());
		const float FullTravel =
		    FMath::Max(Remaining, CapsuleTravel * (Movement->bCrouchMaintainsBaseLocation ? 2.f : 1.f));

		if (DeltaSeconds > 0.f && FullTravel > UE_SMALL_NUMBER)
		{
			// Recover phase from the current component height: no timer or second stance state.
			// This preserves position when Ctrl is released partway through the transition.
			const float Phase = FMath::Acos(FMath::Clamp(2.f * Remaining / FullTravel - 1.f, -1.f, 1.f));
			const float NextPhase = FMath::Min(PI, Phase + PI * DeltaSeconds / StanceTransitionSeconds);
			const float NextRemaining = FullTravel * 0.5f * (1.f + FMath::Cos(NextPhase));

			Location.Z = Target.Z + FMath::Sign(Error) * FMath::Min(Remaining, NextRemaining);
		}

		Camera->SetRelativeLocation(Location);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

		if (DeltaSeconds > 0.f && Remaining > 0.25f)
		{
			TraceStanceCamera(Character, TEXT("blend"));
			UE_LOG(LogTemp,
			       Log,
			       TEXT("[CrouchTrace] dt=%.4f targetZ=%.3f remaining=%.3f travel=%.3f step=%.3f"),
			       DeltaSeconds,
			       Target.Z,
			       Remaining,
			       FullTravel,
			       Error - (Location.Z - Target.Z));
		}

#endif

		if (auto* Light = Character.FindComponentByClass<USpotLightComponent>())
		{
			FVector LightLocation = FMazeItemSystem::HeadlampDefinition().HeadOffset;

			LightLocation.Z += Location.Z - Target.Z - HeightAdjust;
			Light->SetRelativeLocation(LightLocation);
		}
	}

	void CompensateStanceCamera(AMazeCharacter& Character, float HalfHeightAdjust)
	{
		if (Character.IsLocallyControlled() && Character.GetCharacterMovement()->bCrouchMaintainsBaseLocation)
			if (auto* Camera = Character.FindComponentByClass<UCameraComponent>())
			{
				// CharacterMovement already moved the capsule; preserve the previous world-space eye height.
				FVector Location = Camera->GetRelativeLocation();
				Location.Z += HalfHeightAdjust;
				Camera->SetRelativeLocation(Location);
			}
	}
}

AMazeCharacter::AMazeCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	GetCapsuleComponent()->InitCapsuleSize(34, 90);
	bReplicates = true;
	SetReplicateMovement(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	auto* Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PillBody"));

	Body->SetupAttachment(GetCapsuleComponent());
	Body->SetStaticMesh(Cylinder.Object);
	Body->SetRelativeScale3D(FVector(0.68f, 0.68f, 1.12f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetOwnerNoSee(true);

	auto* Top = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PillTop"));

	Top->SetupAttachment(GetCapsuleComponent());
	Top->SetStaticMesh(Sphere.Object);
	Top->SetRelativeLocation(FVector(0, 0, 56));
	Top->SetRelativeScale3D(FVector(0.68f));
	Top->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Top->SetOwnerNoSee(true);

	auto* Bottom = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PillBottom"));

	Bottom->SetupAttachment(GetCapsuleComponent());
	Bottom->SetStaticMesh(Sphere.Object);
	Bottom->SetRelativeLocation(FVector(0, 0, -56));
	Bottom->SetRelativeScale3D(FVector(0.68f));
	Bottom->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bottom->SetOwnerNoSee(true);

	auto* Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));

	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0, 0, 70));
	Camera->bUsePawnControlRotation = true;
	Camera->FieldOfView = 95;

	const auto& Lamp = FMazeItemSystem::HeadlampDefinition();

	HeadlampLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("HeadlampLight"));
	HeadlampLight->SetupAttachment(GetCapsuleComponent());
	HeadlampLight->SetRelativeLocation(Lamp.HeadOffset);
	HeadlampLight->SetAbsolute(false, true, false);
	HeadlampLight->SetMobility(EComponentMobility::Movable);
	HeadlampLight->SetIntensityUnits(ELightUnits::Lumens);
	HeadlampLight->SetIntensity(Lamp.IntensityLumens);
	HeadlampLight->SetLightColor(Lamp.Color);
	HeadlampLight->SetAttenuationRadius(Lamp.Range);
	HeadlampLight->SetInnerConeAngle(Lamp.InnerConeDegrees);
	HeadlampLight->SetOuterConeAngle(Lamp.OuterConeDegrees);
	HeadlampLight->SetCastShadows(true);
	HeadlampLight->SetVisibility(false);
	bUseControllerRotationYaw = true;
	GetCharacterMovement()->MaxWalkSpeed = FMazePlayerControlDefinition::WalkSpeed;
	GetCharacterMovement()->MaxWalkSpeedCrouched = FMazePlayerControlDefinition::CrouchSpeed;
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
	GetCharacterMovement()->SetCrouchedHalfHeight(FMazePlayerControlDefinition::CrouchedHalfHeight);
	GetCharacterMovement()->JumpZVelocity = FMazePlayerControlDefinition::JumpVelocity;
	GetCharacterMovement()->AirControl = 0.25f;
}

void AMazeCharacter::BeginPlay()
{
	Super::BeginPlay();
	ECSSubsystem = GetWorld()->GetSubsystem<UMazeECSSubsystem>();
	check(ECSSubsystem);
	PlayerEntity = ECSSubsystem->CreatePlayer();

	if (!HasAuthority())
	{
		OnRep_PlayerSnapshot();
		OnRep_Items();
	}
	else
		ReplicatedItems = ECSSubsystem->ReadItems(PlayerEntity);

	RefreshHeadlamp();

	if (GetNetMode() != NM_DedicatedServer && !FindComponentByClass<UMazeFootstepAudioComponent>())
	{
		auto* Footsteps = NewObject<UMazeFootstepAudioComponent>(this);

		Footsteps->RegisterComponent();
	}
}

void AMazeCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearLocalInput();
	CameraMotion = FMazeCameraMotion();

	if (ECSSubsystem)
		ECSSubsystem->DestroyPlayer(PlayerEntity);

	PlayerEntity = FMassEntityHandle();
	HeadlampLight->SetVisibility(false);
	ReplicatedItems = FMazeItemsSnapshot();
	ECSSubsystem = nullptr;
	Super::EndPlay(Reason);
}

FMazeVitals AMazeCharacter::GetVitals() const
{
	if (ECSSubsystem)
		return ECSSubsystem->ReadVitals(PlayerEntity);

	FMazeVitals Missing;

	Missing.Health = Missing.Stamina = 0.f;

	return Missing;
}

int32 AMazeCharacter::GetReachedExit() const
{
	return ECSSubsystem ? ECSSubsystem->ReadReachedExit(PlayerEntity) : 0;
}

void AMazeCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	SmoothStanceCamera(*this, DeltaSeconds);
	RefreshHeadlamp();

	auto* Movement = GetCharacterMovement();

	// Live Coding does not refresh constructor defaults on existing movement components.
	Movement->GetNavAgentPropertiesRef().bCanCrouch = true;
	Movement->MaxWalkSpeedCrouched = FMazePlayerControlDefinition::CrouchSpeed;
	Movement->JumpZVelocity = FMazePlayerControlDefinition::JumpVelocity;

	if (Movement->GetCrouchedHalfHeight() != FMazePlayerControlDefinition::CrouchedHalfHeight)
		Movement->SetCrouchedHalfHeight(FMazePlayerControlDefinition::CrouchedHalfHeight);

	if (!ECSSubsystem || (!HasAuthority() && !IsLocallyControlled()))
		return;

	// Input focus and physics are observations, not gameplay state owned by the Actor.
	if (const auto* PC = Cast<APlayerController>(Controller); PC && IsLocallyControlled())
	{
		if (!PC->IsInputKeyDown(EKeys::LeftShift))
			SprintStop();

		if (!PC->IsInputKeyDown(EKeys::SpaceBar))
			JumpStop();

		// Crouch is held input: refresh both states, even when the live input mapping cache is stale.
		if (PC->IsInputKeyDown(EKeys::LeftControl) || PC->IsInputKeyDown(EKeys::RightControl))
			CrouchStart();
		else
			CrouchStop();
	}

	// Remote intent arrives in CharacterMovement saved moves, without a second crouch RPC/state.
	if (HasAuthority() && !IsLocallyControlled())
		ECSSubsystem->SetInputAction(PlayerEntity, EMazeInputAction::Crouch, Movement->bWantsToCrouch);

	FMazePlayerPoseFragment Pose;

	Pose.Location = GetActorLocation();
	Pose.Forward = GetActorForwardVector();
	Pose.Right = GetActorRightVector();
	Pose.Velocity = Movement->Velocity;
	Pose.Acceleration = Movement->GetCurrentAcceleration();
	Pose.bOnGround = Movement->IsMovingOnGround();
	Pose.bCrouched = Movement->IsCrouching();
	Pose.bInputEnabled = Controller && !Controller->IsMoveInputIgnored();
	Pose.Sensitivity = GetDefault<UMazePreferences>()->GetSensitivity();

	const auto Command = ECSSubsystem->ResolvePlayer(PlayerEntity, Pose);

	Movement->MaxWalkSpeed = Command.Speed;

	if (HasAuthority())
	{
		ReplicatedVitals = GetVitals();
		ReplicatedExit = GetReachedExit();
		ReplicatedItems = ECSSubsystem->ReadItems(PlayerEntity);
	}

	if (!IsLocallyControlled() && !Command.bDead)
		return;

	if (Command.bDead)
	{
		UnCrouch();
		StopJumping();
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}
	else
	{
		if (Command.bCrouch)
			Crouch();
		else
			UnCrouch();

		AddMovementInput(Command.Movement);

		if (Command.bStartJump)
			Jump();
		else if (!Command.bJumpHeld)
			StopJumping();
	}

	AddControllerYawInput(Command.Yaw);
	AddControllerPitchInput(Command.Pitch);
}

void AMazeCharacter::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	Super::CalcCamera(DeltaTime, OutResult);

	const auto* PC = Cast<APlayerController>(Controller);
	const bool bEnabled = IsLocallyControlled() && PC && !PC->IsMoveInputIgnored() && !PC->IsLookInputIgnored() &&
	                      !UGameplayStatics::IsGamePaused(this) && FMazeVitalsSystem::IsAlive(GetVitals());

	if (!bEnabled)
	{
		CameraMotion = FMazeCameraMotion();

		return;
	}

	const auto* Movement = GetCharacterMovement();
	const float GroundSpeed = Movement->IsMovingOnGround() ? Movement->Velocity.Size2D() : 0.f;

	CameraMotion.Update(GetWorld()->GetDeltaSeconds(), GroundSpeed, OutResult.Rotation);
	// Apply once to the freshly evaluated view, never to ControlRotation or the camera component.
	OutResult.Location += OutResult.Rotation.RotateVector(CameraMotion.Offset);
	OutResult.Rotation += CameraMotion.Rotation;
}

void AMazeCharacter::UnPossessed()
{
	ClearLocalInput();
	CameraMotion = FMazeCameraMotion();
	Super::UnPossessed();
}

bool AMazeCharacter::CanJumpInternal_Implementation() const
{
	const FMazeVitals Vitals = GetVitals();

	return FMazeVitalsSystem::IsAlive(Vitals) && (bWasJumping || FMazeVitalsSystem::CanJump(Vitals)) &&
	       Super::CanJumpInternal_Implementation();
}

void AMazeCharacter::OnJumped_Implementation()
{
	// Charge only when CharacterMovement actually starts a jump, not on key presses.
	if (ECSSubsystem)
	{
		ECSSubsystem->SpendJumpStamina(PlayerEntity);
		ECSSubsystem->SetLocomotion(PlayerEntity, false, false);
	}

	Super::OnJumped_Implementation();
}

void AMazeCharacter::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	// Publish loss of ground immediately, including walking off a ledge between ticks.
	if (ECSSubsystem && !GetCharacterMovement()->IsMovingOnGround())
		ECSSubsystem->SetLocomotion(PlayerEntity, false, false);
}

float AMazeCharacter::TakeDamage(float DamageAmount,
                                 const FDamageEvent& DamageEvent,
                                 AController* EventInstigator,
                                 AActor* DamageCauser)
{
	if (!HasAuthority() || !ECSSubsystem || !FMazeVitalsSystem::IsAlive(GetVitals()) ||
	    !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.f || !CanBeDamaged())
		return 0.f;

	const float Accepted = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	const float Applied = ECSSubsystem ? ECSSubsystem->ApplyDamage(PlayerEntity, Accepted) : 0.f;

	if (!FMazeVitalsSystem::IsAlive(GetVitals()))
	{
		if (ECSSubsystem)
			ECSSubsystem->SetInputAction(PlayerEntity, EMazeInputAction::Sprint, false);

		StopJumping();
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
	}

	return Applied;
}

void AMazeCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
	Super::SetupPlayerInputComponent(Input);
	Input->BindAxis(TEXT("MoveForward"), this, &AMazeCharacter::Forward);
	Input->BindAxis(TEXT("MoveRight"), this, &AMazeCharacter::Right);
	Input->BindAxis(TEXT("Turn"), this, &AMazeCharacter::Turn);
	Input->BindAxis(TEXT("LookUp"), this, &AMazeCharacter::LookUp);
	Input->BindAction(TEXT("Jump"), IE_Pressed, this, &AMazeCharacter::JumpStart);
	Input->BindAction(TEXT("Jump"), IE_Released, this, &AMazeCharacter::JumpStop);
	Input->BindAction(TEXT("Sprint"), IE_Pressed, this, &AMazeCharacter::SprintStart);
	Input->BindAction(TEXT("Sprint"), IE_Released, this, &AMazeCharacter::SprintStop);
	Input->BindAction(TEXT("Crouch"), IE_Pressed, this, &AMazeCharacter::CrouchStart);
	Input->BindAction(TEXT("Crouch"), IE_Released, this, &AMazeCharacter::CrouchStop);
	Input->BindAction(TEXT("NewMaze"), IE_Pressed, this, &AMazeCharacter::RestartMaze);
	Input->BindKey(EKeys::L, IE_Pressed, this, &AMazeCharacter::ToggleHeadlamp);
}

void AMazeCharacter::ToggleHeadlamp()
{
	if (!IsLocallyControlled() || !Controller || Controller->IsMoveInputIgnored() || Controller->IsLookInputIgnored() ||
	    UGameplayStatics::IsGamePaused(this))
		return;

	ServerToggleHeadlamp();
}

void AMazeCharacter::ServerToggleHeadlamp_Implementation()
{
	// Unreal accepts this RPC only from the owning connection, targeting this pawn's entity.
	if (!Controller || Controller->IsMoveInputIgnored() || Controller->IsLookInputIgnored())
		return;

	if (ECSSubsystem && ECSSubsystem->ToggleHeadlamp(PlayerEntity))
	{
		ReplicatedItems = ECSSubsystem->ReadItems(PlayerEntity);
		RefreshHeadlamp();
		ForceNetUpdate();
	}
}

void AMazeCharacter::Forward(float Value)
{
	if (ECSSubsystem)
		ECSSubsystem->SetInputAxis(PlayerEntity, EMazeInputAxis::Forward, Value);
}

void AMazeCharacter::Right(float Value)
{
	if (ECSSubsystem)
		ECSSubsystem->SetInputAxis(PlayerEntity, EMazeInputAxis::Right, Value);
}

void AMazeCharacter::Turn(float Value)
{
	if (ECSSubsystem)
		ECSSubsystem->SetInputAxis(PlayerEntity, EMazeInputAxis::Yaw, Value);
}

void AMazeCharacter::LookUp(float Value)
{
	if (ECSSubsystem)
		ECSSubsystem->SetInputAxis(PlayerEntity, EMazeInputAxis::Pitch, Value);
}

void AMazeCharacter::SprintStart()
{
	if (ECSSubsystem && !ECSSubsystem->IsSprintHeld(PlayerEntity) && !HasAuthority())
		ServerSetSprint(true);

	if (ECSSubsystem)
		ECSSubsystem->SetInputAction(PlayerEntity, EMazeInputAction::Sprint, true);
}

void AMazeCharacter::SprintStop()
{
	if (ECSSubsystem && ECSSubsystem->IsSprintHeld(PlayerEntity) && !HasAuthority())
		ServerSetSprint(false);

	if (ECSSubsystem)
		ECSSubsystem->SetInputAction(PlayerEntity, EMazeInputAction::Sprint, false);
}

void AMazeCharacter::JumpStart()
{
	if (ECSSubsystem)
		ECSSubsystem->SetInputAction(PlayerEntity, EMazeInputAction::Jump, true);
}

void AMazeCharacter::CrouchStart()
{
	if (ECSSubsystem)
		ECSSubsystem->SetInputAction(PlayerEntity, EMazeInputAction::Crouch, true);
}

void AMazeCharacter::CrouchStop()
{
	if (ECSSubsystem)
		ECSSubsystem->SetInputAction(PlayerEntity, EMazeInputAction::Crouch, false);
}

void AMazeCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	TraceStanceCamera(*this, TEXT("crouch-before"), HalfHeightAdjust);
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	CompensateStanceCamera(*this, HalfHeightAdjust);
	RefreshStancePresentation();
	SmoothStanceCamera(*this, 0.f);
	TraceStanceCamera(*this, TEXT("crouch-after"), HalfHeightAdjust);
}

void AMazeCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	TraceStanceCamera(*this, TEXT("stand-before"), HalfHeightAdjust);
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	CompensateStanceCamera(*this, -HalfHeightAdjust);
	RefreshStancePresentation();
	SmoothStanceCamera(*this, 0.f);
	TraceStanceCamera(*this, TEXT("stand-after"), HalfHeightAdjust);
}

void AMazeCharacter::RefreshStancePresentation()
{
	const auto* Defaults = GetClass()->GetDefaultObject<AMazeCharacter>();
	const float StandingHalfHeight = Defaults->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	const float HeightAdjust = StandingHalfHeight - HalfHeight;
	TInlineComponentArray<USceneComponent*> Components(this);
	TInlineComponentArray<USceneComponent*> DefaultComponents(Defaults);

	for (auto* Component : Components)
	{
		const FName Name = Component->GetFName();

		// The local camera retains its current visual height and eases toward the confirmed stance.
		if (Name == TEXT("FirstPersonCamera") && IsLocallyControlled())
			continue;

		const auto* DefaultEntry = DefaultComponents.FindByPredicate(
		    [Name](const auto* Candidate)
		    {
			    return Candidate->GetFName() == Name;
		    });

		if (!DefaultEntry)
			continue;

		FVector Location = (*DefaultEntry)->GetRelativeLocation();

		if (Name == TEXT("FirstPersonCamera") || Name == TEXT("HeadlampLight") || Name == TEXT("PillTop"))
			Location.Z -= HeightAdjust;
		else if (Name == TEXT("PillBottom"))
			Location.Z += HeightAdjust;
		else if (Name == TEXT("PillBody"))
		{
			FVector Scale = (*DefaultEntry)->GetRelativeScale3D();
			const float Radius = GetCapsuleComponent()->GetUnscaledCapsuleRadius();

			Scale.Z *= FMath::Max(0.f, HalfHeight - Radius) / FMath::Max(1.f, StandingHalfHeight - Radius);
			Component->SetRelativeScale3D(Scale);
		}
		else
			continue;

		Component->SetRelativeLocation(Location);
	}
}

void AMazeCharacter::JumpStop()
{
	if (ECSSubsystem)
		ECSSubsystem->SetInputAction(PlayerEntity, EMazeInputAction::Jump, false);
}

void AMazeCharacter::RestartMaze()
{
	if (GetNetMode() == NM_Standalone)
		if (auto* PC = Cast<AMazePlayerController>(Controller))
			PC->StartNewGame();
}

void AMazeCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AMazeCharacter, ReplicatedVitals, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AMazeCharacter, ReplicatedExit, COND_OwnerOnly);
	DOREPLIFETIME(AMazeCharacter, ReplicatedItems);
}

void AMazeCharacter::OnRep_Items()
{
	if (ECSSubsystem)
		ECSSubsystem->ReceiveItems(PlayerEntity, ReplicatedItems);

	RefreshHeadlamp();
}

void AMazeCharacter::RefreshHeadlamp()
{
	const bool bVisible =
	    GetNetMode() != NM_DedicatedServer && ECSSubsystem && ECSSubsystem->ReadHeadlampEnabled(PlayerEntity);

	if (HeadlampLight && HeadlampLight->IsVisible() != bVisible)
		HeadlampLight->SetVisibility(bVisible);

	// Base aim includes replicated view pitch for other players; their cameras are not evaluated locally.
	if (HeadlampLight && bVisible)
	{
		// Refresh existing lights too: constructor defaults do not update a live character.
		const auto Lamp = FMazeItemSystem::HeadlampDefinition();

		HeadlampLight->SetIntensity(Lamp.IntensityLumens);
		HeadlampLight->SetInnerConeAngle(Lamp.InnerConeDegrees);
		HeadlampLight->SetOuterConeAngle(Lamp.OuterConeDegrees);
		HeadlampLight->SetWorldRotation(GetBaseAimRotation());
	}
}

void AMazeCharacter::OnRep_PlayerSnapshot()
{
	if (ECSSubsystem)
		ECSSubsystem->ReceivePlayer(PlayerEntity, ReplicatedVitals, ReplicatedExit);
}

void AMazeCharacter::ServerSetSprint_Implementation(bool bHeld)
{
	if (ECSSubsystem)
		ECSSubsystem->SetInputAction(PlayerEntity, EMazeInputAction::Sprint, bHeld);
}

void AMazeCharacter::ClearLocalInput()
{
	CameraMotion = FMazeCameraMotion();

	if (auto* Footsteps = FindComponentByClass<UMazeFootstepAudioComponent>())
		Footsteps->ResetPlayback();

	if (!IsLocallyControlled())
		return;

	SprintStop();
	CrouchStop();
	UnCrouch();
	StopJumping();

	if (ECSSubsystem)
		ECSSubsystem->ClearInput(PlayerEntity);
}
