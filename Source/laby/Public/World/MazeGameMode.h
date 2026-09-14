#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/GameStateBase.h"
#include "ECS/MazeECSFragments.h"
#include "MazeGameMode.generated.h"

UCLASS()
class LABY_API AMazeGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AMazeGameMode();
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void PreLogin(const FString& Options,
	                      const FString& Address,
	                      const FUniqueNetIdRepl& UniqueId,
	                      FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	void StartRoom(APlayerController* Requester);
	void PublishRoom();
};

// Replication mirror only; the server ECS owns the room.
UCLASS()
class LABY_API AMazeGameState : public AGameStateBase
{
	GENERATED_BODY()
public:
	UPROPERTY(ReplicatedUsing = OnRep_Room)
	FMazeRoomFragment Room;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UFUNCTION()
	void OnRep_Room();
};

UCLASS()
class LABY_API AMazeHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<class UMazeHUDWidget> HUDWidget;
};
