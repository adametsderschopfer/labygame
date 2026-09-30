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
	void ToggleMap();
	bool IsMapOpen() const
	{
		return ReadSession().bMapOpen;
	}

	void StartNewGame();
	void RefreshMenuAmbientVolume();
	virtual void PlayerTick(float DeltaTime) override;
	void ShowNetworkMenu(bool bJoinScreen = false, bool bSettingsScreen = false, bool bLocalJoin = false);
	UFUNCTION(Server, Reliable)
	void ServerStartRoom();
	bool IsMenuOpen() const
	{
		return ReadSession().bMenuOpen;
	}

	UFUNCTION(BlueprintCallable, Category = "Maze|UI")
	void ShowMenu(bool Settings = false);
	UFUNCTION(BlueprintCallable, Category = "Maze|UI")
	void CloseMenu();
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	void ToggleDevelopmentMenu();
	bool IsDevelopmentMenuOpen() const
	{
		return DevelopmentMenu.IsValid();
	}

	bool DevelopmentRevealMap() const
	{
		return bDevelopmentRevealMap;
	}

	bool DevelopmentShowRoute() const
	{
		return bDevelopmentShowRoute;
	}

#endif

private:
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	void ToggleDevelopmentCamera();
	void CloseDevelopmentMenu();
	void EnsureDevelopmentPresentation();
	void RefreshDevelopmentPresentation(float DeltaTime);
	void DestroyDevelopmentPresentation();
	void DevelopmentRestartSeed();
	void DevelopmentTeleport(bool bExit);
	void SetDevelopmentCollision(bool bEnabled);

	TSharedPtr<class SWidget> DevelopmentMenu;
	TSharedPtr<struct FMazeDevelopmentPresentation> DevelopmentPresentation;
	bool bDevelopmentRevealMap = false;
	bool bDevelopmentShowRoute = false;
#endif
	void RemoveMenuWidget();

	UPROPERTY(Transient)
	TObjectPtr<class UMazeMenuWidget> MenuWidget;

	UPROPERTY(Transient)
	TObjectPtr<class UMazeECSSubsystem> ECSSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<class UMazeExplorationMapWidget> ExplorationMap;
	TSharedPtr<class SWidget> NetworkMenu;
	bool bDisplayedRoom = false;
	bool bDisplayedStarted = false;

	FMazeSessionFragment ReadSession() const;
};
