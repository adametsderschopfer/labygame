#include "Player/MazePlayerController.h"
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
#include "MazeDevelopmentUI.h"
#include "ECS/MazeDevelopmentSystem.h"
#include "ECS/MazeECSSubsystem.h"
#include "Player/MazeCharacter.h"
#include "World/MazeWorld.h"
#include "World/MazeLocationSubsystem.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

bool ExecuteMazeDevelopmentTeleport(AMazeCharacter& Character, bool bExit)
{
	auto* World = Character.GetWorld();
	auto* ECS = World ? World->GetSubsystem<UMazeECSSubsystem>() : nullptr;
	auto* Location = World ? World->GetSubsystem<UMazeLocationSubsystem>() : nullptr;
	FMazeDevelopmentTeleport Command;

	if (!ECS || !Location || !Location->AreAssetsReady() || !Location->GetFailure().IsEmpty() ||
	    !ECS->RequestDevelopmentTeleport(Character.GetPlayerEntity(), Character.GetActorLocation(), bExit, Command))
		return false;

	AMazeWorld* Representation = nullptr;

	for (TActorIterator<AMazeWorld> It(World); It; ++It)
		if (It->GetMazeEntity() == ECS->ReadSession().Maze)
		{
			Representation = *It;
			break;
		}

	if (!Representation || ECS->ReadMaze(Representation->GetMazeEntity()).Revision != Command.Revision)
		return false;

	FCollisionQueryParams Query(SCENE_QUERY_STAT(MazeDevelopmentTeleport), false, &Character);
	FHitResult Floor;

	if (!World->LineTraceSingleByObjectType(Floor,
	                                        Command.FloorLocation + FVector(0, 0, 150),
	                                        Command.FloorLocation - FVector(0, 0, 350),
	                                        FCollisionObjectQueryParams(ECC_WorldStatic),
	                                        Query) ||
	    Floor.ImpactNormal.Z < 0.7f)
		return false;

	Character.ClearLocalInput();

	const FVector Target =
	    Floor.ImpactPoint + FVector(0, 0, Character.GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 4.f);

	if (!Character.TeleportTo(Target, Character.GetActorRotation()))
		return false;

	Character.GetCharacterMovement()->StopMovementImmediately();
	Character.GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	// The normal engine observation on the next character tick updates ECS pose/progress.
	Representation->InvalidateDevelopmentTeleportReadiness();

	return true;
}

void AMazePlayerController::DevelopmentRestartSeed()
{
	if (!ECSSubsystem || GetNetMode() != NM_Standalone)
		return;

	const auto Maze = ECSSubsystem->ReadMaze(ReadSession().Maze);

	if (!Maze.Data || Maze.Seed == 0)
		return;

	const FString Options = FString::Printf(TEXT("Room=1?QuickStart=1?DevelopmentSeed=%d"), Maze.Seed);

	CloseDevelopmentMenu();
	UGameplayStatics::OpenLevel(this, TEXT("/Game/Maps/Maze"), true, Options);
}

void AMazePlayerController::DevelopmentTeleport(bool bExit)
{
	const auto* Location = GetWorld()->GetSubsystem<UMazeLocationSubsystem>();
	auto* MazePawn = Cast<AMazeCharacter>(GetPawn());
	const bool bSuccess =
	    Location && Location->IsReady() && MazePawn && ExecuteMazeDevelopmentTeleport(*MazePawn, bExit);

	if (DevelopmentPresentation)
	{
		DevelopmentPresentation->bWaitingTeleport = bSuccess;
		DevelopmentPresentation->Status =
		    bSuccess ? MazeDevelopmentText(TEXT("Teleported. Preparing surroundings..."),
		                                   TEXT("Телепорт выполнен. Подготовка окружения..."),
		                                   TEXT("Teletransporte realizado. Preparando el entorno..."))
		             : MazeDevelopmentText(TEXT("Teleport unavailable: resources or capsule fit."),
		                                   TEXT("Телепорт недоступен: ресурсы или место для капсулы."),
		                                   TEXT("Teletransporte no disponible: recursos o espacio para la cápsula."));
		DevelopmentPresentation->RefreshWait = 0;
	}
}

void AMazePlayerController::SetDevelopmentCollision(bool bEnabled)
{
	auto* Viewport = GetWorld()->GetGameViewport();

	if (!DevelopmentPresentation || !Viewport)
		return;

	if (!DevelopmentPresentation->bSavedCollision)
	{
		DevelopmentPresentation->Viewport = Viewport;
		DevelopmentPresentation->bSavedCollision = true;
		DevelopmentPresentation->bOriginalCollision = Viewport->EngineShowFlags.Collision;
		DevelopmentPresentation->bOriginalVolumes = Viewport->EngineShowFlags.Volumes;
	}

	Viewport->EngineShowFlags.SetCollision(bEnabled);
	Viewport->ToggleShowCollision();

	if (!bEnabled && DevelopmentPresentation->bOriginalVolumes)
	{
		Viewport->EngineShowFlags.SetVolumes(true);
		Viewport->ToggleShowVolumes();
	}
}

void AMazeCharacter::FellOutOfWorld(const UDamageType& DamageType)
{
	const auto* ECS = GetWorld()->GetSubsystem<UMazeECSSubsystem>();

	if (ECS && ECS->ShouldDevelopmentRescue(GetPlayerEntity()))
	{
		// Engine KillZ would destroy the Actor directly, bypassing the health/damage system.
		// ECS authorizes the development rescue; the common adapter performs the teleport.
		ExecuteMazeDevelopmentTeleport(*this, false);

		return;
	}

	Super::FellOutOfWorld(DamageType);
}

#endif
