#include "MazeCharacter.h"
#include "MazePlayerController.h"
#include "MazeECSSubsystem.h"
#include "MazeVitalsSystem.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

AMazeCharacter::AMazeCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	GetCapsuleComponent()->InitCapsuleSize(34, 90);
	auto* Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0, 0, 70));
	Camera->bUsePawnControlRotation = true;
	Camera->FieldOfView = 95;
	bUseControllerRotationYaw = true;
	GetCharacterMovement()->MaxWalkSpeed = 450;
	GetCharacterMovement()->JumpZVelocity = 420;
	GetCharacterMovement()->AirControl = 0.25f;
}

void AMazeCharacter::BeginPlay()
{
	Super::BeginPlay();
	ECSSubsystem = GetWorld()->GetSubsystem<UMazeECSSubsystem>();
	check(ECSSubsystem);
	PlayerEntity = ECSSubsystem->CreatePlayer();
}

void AMazeCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
	if (ECSSubsystem) ECSSubsystem->DestroyPlayer(PlayerEntity);
	PlayerEntity = FMassEntityHandle();
	ECSSubsystem = nullptr;
	Super::EndPlay(Reason);
}

FMazeVitals AMazeCharacter::GetVitals() const
{
	if (ECSSubsystem) return ECSSubsystem->ReadVitals(PlayerEntity);
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
	auto* Movement = GetCharacterMovement();
	if (!ECSSubsystem) return;
	// Input focus and physics are observations, not gameplay state owned by the Actor.
	if (const auto* PC = Cast<APlayerController>(Controller))
	{
		if (!PC->IsInputKeyDown(EKeys::LeftShift)) SprintStop();
		if (!PC->IsInputKeyDown(EKeys::SpaceBar)) JumpStop();
	}
	FMazePlayerPoseFragment Pose;
	Pose.Location = GetActorLocation();
	Pose.Forward = GetActorForwardVector();
	Pose.Right = GetActorRightVector();
	Pose.Velocity = Movement->Velocity;
	Pose.Acceleration = Movement->GetCurrentAcceleration();
	Pose.bOnGround = Movement->IsMovingOnGround();
	Pose.bInputEnabled = Controller && !Controller->IsMoveInputIgnored();
	Pose.Sensitivity = GetDefault<UMazePreferences>()->GetSensitivity();
	const auto Command = ECSSubsystem->ResolvePlayer(PlayerEntity, Pose);
	Movement->MaxWalkSpeed = Command.Speed;
	if (Command.bDead)
	{
		StopJumping();
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}
	else
	{
		AddMovementInput(Command.Movement);
		if (Command.bStartJump) Jump();
		else if (!Command.bJumpHeld) StopJumping();
	}
	AddControllerYawInput(Command.Yaw);
	AddControllerPitchInput(Command.Pitch);
}

bool AMazeCharacter::CanJumpInternal_Implementation() const
{
	const FMazeVitals Vitals = GetVitals();
	return FMazeVitalsSystem::IsAlive(Vitals) && (bWasJumping || FMazeVitalsSystem::CanJump(Vitals))
		&& Super::CanJumpInternal_Implementation();
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

float AMazeCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!ECSSubsystem || !FMazeVitalsSystem::IsAlive(GetVitals()) || !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.f || !CanBeDamaged()) return 0.f;
	const float Accepted = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	const float Applied = ECSSubsystem ? ECSSubsystem->ApplyDamage(PlayerEntity, Accepted) : 0.f;
	if (!FMazeVitalsSystem::IsAlive(GetVitals()))
	{
		if (ECSSubsystem) ECSSubsystem->SetInputAction(PlayerEntity, EMazeInputAction::Sprint, false);
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
	Input->BindAction(TEXT("NewMaze"), IE_Pressed, this, &AMazeCharacter::RestartMaze);
}
void AMazeCharacter::Forward(float Value) { if (ECSSubsystem) ECSSubsystem->SetInputAxis(PlayerEntity, EMazeInputAxis::Forward, Value); }
void AMazeCharacter::Right(float Value) { if (ECSSubsystem) ECSSubsystem->SetInputAxis(PlayerEntity, EMazeInputAxis::Right, Value); }
void AMazeCharacter::Turn(float Value) { if (ECSSubsystem) ECSSubsystem->SetInputAxis(PlayerEntity, EMazeInputAxis::Yaw, Value); }
void AMazeCharacter::LookUp(float Value) { if (ECSSubsystem) ECSSubsystem->SetInputAxis(PlayerEntity, EMazeInputAxis::Pitch, Value); }
void AMazeCharacter::SprintStart() { if (ECSSubsystem) ECSSubsystem->SetInputAction(PlayerEntity, EMazeInputAction::Sprint, true); }
void AMazeCharacter::SprintStop() { if (ECSSubsystem) ECSSubsystem->SetInputAction(PlayerEntity, EMazeInputAction::Sprint, false); }
void AMazeCharacter::JumpStart() { if (ECSSubsystem) ECSSubsystem->SetInputAction(PlayerEntity, EMazeInputAction::Jump, true); }
void AMazeCharacter::JumpStop() { if (ECSSubsystem) ECSSubsystem->SetInputAction(PlayerEntity, EMazeInputAction::Jump, false); }
void AMazeCharacter::RestartMaze()
{
	if (GetNetMode() == NM_Standalone)
		if (auto* PC = Cast<AMazePlayerController>(Controller)) PC->StartNewGame();
}
