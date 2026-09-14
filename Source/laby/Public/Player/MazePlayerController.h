#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ECS/MazeECSFragments.h"
#include "MazePlayerController.generated.h"

UCLASS(Config = GameUserSettings)
class LABY_API UMazePreferences : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(Config)
	float MouseSensitivity = 1.f;

	float GetSensitivity() const
	{
		return FMath::Clamp(MouseSensitivity, 0.1f, 3.f);
	}

	void SetSensitivity(float Value);
};

UCLASS()
class LABY_API AMazePlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	void ToggleMenu();
	void StartNewGame();
	virtual void PlayerTick(float DeltaTime) override;
	void ShowNetworkMenu(bool bJoinScreen = false, bool bSettingsScreen = false, bool bLocalJoin = false);
	UFUNCTION(Server, Reliable)
	void ServerStartRoom();
	bool IsMenuOpen() const
	{
		return ReadSession().bMenuOpen;
	}

	bool IsMinimapVisible() const
	{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST

		return ReadSession().bMinimapVisible;

#else

		return true;

#endif
	}

	UFUNCTION(BlueprintCallable, Category = "Maze|UI")
	void ShowMenu(bool Settings = false);
	UFUNCTION(BlueprintCallable, Category = "Maze|UI")
	void CloseMenu();

private:
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	void ToggleMinimap();
#endif
	void RemoveMenuWidget();

	UPROPERTY(Transient)
	TObjectPtr<class UMazeMenuWidget> MenuWidget;

	UPROPERTY(Transient)
	TObjectPtr<class UMazeECSSubsystem> ECSSubsystem;
	TSharedPtr<class SWidget> NetworkMenu;
	bool bDisplayedRoom = false;
	bool bDisplayedStarted = false;

	FMazeSessionFragment ReadSession() const;
};
