#include "MazeCharacter.h"
#include "MazePlayerController.h"
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

void AMazeCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	auto* Movement = GetCharacterMovement();
	// Also clear a held sprint when focus/input is flushed by the pause menu.
	if (const auto* PC = Cast<AMazePlayerController>(Controller))
		if (PC->IsMenuOpen() || !PC->IsInputKeyDown(EKeys::LeftShift)) bSprintRequested = false;
	const bool bCanRun = bSprintRequested && Vitals.CanSprint();
	const bool bRunning = bCanRun && Movement->IsMovingOnGround()
		&& Movement->Velocity.SizeSquared2D() > 1.f
		&& !Movement->GetCurrentAcceleration().IsNearlyZero();
	Vitals.Update(DeltaSeconds, bRunning);
	Movement->MaxWalkSpeed = bSprintRequested && Vitals.CanSprint() ? 750.f : 450.f;
}

float AMazeCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!Vitals.IsAlive() || !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.f || !CanBeDamaged()) return 0.f;
	const float Applied = Vitals.Damage(Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser));
	if (!Vitals.IsAlive())
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
void AMazeCharacter::SprintStart() { bSprintRequested = Vitals.CanSprint(); }
void AMazeCharacter::SprintStop() { bSprintRequested = false; GetCharacterMovement()->MaxWalkSpeed = 450; }
void AMazeCharacter::RestartMaze()
{
	if (GetNetMode() == NM_Standalone)
		if (auto* PC = Cast<AMazePlayerController>(Controller)) PC->StartNewGame();
}
