#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "MazeGameMode.generated.h"

UCLASS()
class LABY_API AMazeGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AMazeGameMode();
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
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
