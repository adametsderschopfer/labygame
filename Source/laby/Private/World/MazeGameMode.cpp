#include "World/MazeGameMode.h"
#include "Player/MazePlayerController.h"
#include "Player/MazeCharacter.h"
#include "UI/MazeWidgets.h"
#include "World/MazeWorld.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "EngineUtils.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameSession.h"
#include "ECS/MazeECSSubsystem.h"
#include "World/MazeOnlineGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

AMazeGameMode::AMazeGameMode()
{
	DefaultPawnClass = AMazeCharacter::StaticClass();
	HUDClass = AMazeHUD::StaticClass();
	PlayerControllerClass = AMazePlayerController::StaticClass();
	GameStateClass = AMazeGameState::StaticClass();
	bPauseable = false;
}

void AMazeGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	if (GameSession)
		GameSession->MaxPlayers = 4;

	if (UGameplayStatics::HasOption(Options, TEXT("Room")))
		GetWorld()->GetSubsystem<UMazeECSSubsystem>()->OpenRoom();

	auto* Maze = GetWorld()->SpawnActor<AMazeWorld>();

	Maze->InitializeMaze();
	GetWorld()->SpawnActor<APlayerStart>(Maze->StartLocation(), FRotator::ZeroRotator);
}

void AMazeHUD::BeginPlay()
{
	Super::BeginPlay();

	if (!PlayerOwner || !PlayerOwner->IsLocalController())
		return;

	UClass* WidgetClass = LoadClass<UMazeHUDWidget>(nullptr, TEXT("/Game/UI/WBP_HUD.WBP_HUD_C"));

	if (WidgetClass)
	{
		HUDWidget = CreateWidget<UMazeHUDWidget>(PlayerOwner, WidgetClass);

		if (HUDWidget)
			HUDWidget->AddToViewport();
	}
	else
		UE_LOG(LogTemp, Error, TEXT("Missing /Game/UI/WBP_HUD. Open the editor to create UI assets."));
}

void AMazeHUD::EndPlay(const EEndPlayReason::Type Reason)
{
	if (HUDWidget)
		HUDWidget->RemoveFromParent();

	HUDWidget = nullptr;
	Super::EndPlay(Reason);
}

void AMazeGameMode::PreLogin(const FString& Options,
                             const FString& Address,
                             const FUniqueNetIdRepl& UniqueId,
                             FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);

	if (ErrorMessage.IsEmpty())
		ErrorMessage = GetWorld()->GetSubsystem<UMazeECSSubsystem>()->RoomAdmissionError();
}

void AMazeGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	auto* ECS = GetWorld()->GetSubsystem<UMazeECSSubsystem>();

	if (!ECS->ReadRoom().bActive || !NewPlayer->PlayerState)
		return;

	const bool bHost = NewPlayer->IsLocalController();

	if (!ECS->AddRoomMember(NewPlayer->PlayerState->GetPlayerId(), NewPlayer->PlayerState->GetPlayerName(), bHost))
	{
		GameSession->KickPlayer(NewPlayer, FText::FromString(TEXT("Room is full or already started")));

		return;
	}

	PublishRoom();
}

void AMazeGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// No pawn exists in the lobby. StartRoom creates everyone together.
}

void AMazeGameMode::Logout(AController* Exiting)
{
	if (Exiting->PlayerState)
		GetWorld()->GetSubsystem<UMazeECSSubsystem>()->RemoveRoomMember(Exiting->PlayerState->GetPlayerId());

	Super::Logout(Exiting);
	PublishRoom();
}

void AMazeGameMode::PublishRoom()
{
	if (auto* State = GetGameState<AMazeGameState>())
	{
		State->Room = GetWorld()->GetSubsystem<UMazeECSSubsystem>()->ReadRoom();
		State->ForceNetUpdate();
	}
}

void AMazeGameMode::StartRoom(APlayerController* Requester)
{
	auto* ECS = GetWorld()->GetSubsystem<UMazeECSSubsystem>();

	if (!Requester || !Requester->PlayerState || !ECS->StartRoom(Requester->PlayerState->GetPlayerId()))
		return;

	if (auto* Online = GetGameInstance<UMazeOnlineGameInstance>())
		Online->CloseAdmission();

	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		auto* PC = It->Get();

		if (PC && PC->PlayerState)
			RestartPlayerAtTransform(PC,
			                         FTransform(FRotator::ZeroRotator, ECS->RoomSpawn(PC->PlayerState->GetPlayerId())));
	}

	PublishRoom();
}

void AMazeGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMazeGameState, Room);
}

void AMazeGameState::OnRep_Room()
{
	if (auto* ECS = GetWorld()->GetSubsystem<UMazeECSSubsystem>())
		ECS->ReceiveRoom(Room);
}
