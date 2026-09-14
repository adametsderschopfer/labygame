#include "MazeCharacter.h"
#include "MazePlayerController.h"
#include "MazeVitalsSubsystem.h"
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
	VitalsSubsystem = GetWorld()->GetSubsystem<UMazeVitalsSubsystem>();
	check(VitalsSubsystem);
	VitalsEntity = VitalsSubsystem->CreatePlayer();
}

void AMazeCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
	if (VitalsSubsystem) VitalsSubsystem->DestroyPlayer(VitalsEntity);
	VitalsEntity = FMassEntityHandle();
	VitalsSubsystem = nullptr;
	Super::EndPlay(Reason);
}

FMazeVitals AMazeCharacter::GetVitals() const
{
	if (VitalsSubsystem) return VitalsSubsystem->ReadVitals(VitalsEntity);
	FMazeVitals Missing;
	Missing.Health = Missing.Stamina = 0.f;
	return Missing;
}

void AMazeCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	auto* Movement = GetCharacterMovement();
	// Also clear a held sprint when focus/input is flushed by the pause menu.
	if (const auto* PC = Cast<AMazePlayerController>(Controller))
		if (PC->IsMenuOpen() || !PC->IsInputKeyDown(EKeys::LeftShift)) bSprintRequested = false;
	const FMazeVitals Vitals = GetVitals();
	const bool bCanRun = bSprintRequested && FMazeVitalsSystem::CanSprint(Vitals);
	const bool bRunning = bCanRun && Movement->IsMovingOnGround()
		&& Movement->Velocity.SizeSquared2D() > 1.f
		&& !Movement->GetCurrentAcceleration().IsNearlyZero();
	if (VitalsSubsystem) VitalsSubsystem->SetLocomotion(VitalsEntity, bRunning, Movement->IsMovingOnGround());
	Movement->MaxWalkSpeed = bCanRun ? 750.f : 450.f;
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
	if (VitalsSubsystem)
	{
		VitalsSubsystem->SpendJumpStamina(VitalsEntity);
		VitalsSubsystem->SetLocomotion(VitalsEntity, false, false);
	}
	Super::OnJumped_Implementation();
}

float AMazeCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!VitalsSubsystem || !FMazeVitalsSystem::IsAlive(GetVitals()) || !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.f || !CanBeDamaged()) return 0.f;
	const float Accepted = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	const float Applied = VitalsSubsystem ? VitalsSubsystem->ApplyDamage(VitalsEntity, Accepted) : 0.f;
	if (!FMazeVitalsSystem::IsAlive(GetVitals()))
	{
		bSprintRequested = false;
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
	Input->BindAction(TEXT("Jump"), IE_Pressed, this, &ACharacter::Jump);
	Input->BindAction(TEXT("Jump"), IE_Released, this, &ACharacter::StopJumping);
	Input->BindAction(TEXT("Sprint"), IE_Pressed, this, &AMazeCharacter::SprintStart);
	Input->BindAction(TEXT("Sprint"), IE_Released, this, &AMazeCharacter::SprintStop);
	Input->BindAction(TEXT("NewMaze"), IE_Pressed, this, &AMazeCharacter::RestartMaze);
}
void AMazeCharacter::Forward(float Value) { AddMovementInput(GetActorForwardVector(), Value); }
void AMazeCharacter::Right(float Value) { AddMovementInput(GetActorRightVector(), Value); }
void AMazeCharacter::Turn(float Value) { AddControllerYawInput(Value * GetDefault<UMazePreferences>()->GetSensitivity()); }
void AMazeCharacter::LookUp(float Value) { AddControllerPitchInput(Value * GetDefault<UMazePreferences>()->GetSensitivity()); }
void AMazeCharacter::SprintStart() { bSprintRequested = FMazeVitalsSystem::IsAlive(GetVitals()); }
void AMazeCharacter::SprintStop() { bSprintRequested = false; GetCharacterMovement()->MaxWalkSpeed = 450; }
void AMazeCharacter::RestartMaze()
{
	if (GetNetMode() == NM_Standalone)
		if (auto* PC = Cast<AMazePlayerController>(Controller)) PC->StartNewGame();
}
