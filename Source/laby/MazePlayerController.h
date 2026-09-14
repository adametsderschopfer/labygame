#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MazeECSFragments.h"
#include "MazePlayerController.generated.h"

UCLASS(Config=GameUserSettings)
class LABY_API UMazePreferences : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(Config) float MouseSensitivity = 1.f;
	float GetSensitivity() const { return FMath::Clamp(MouseSensitivity, 0.1f, 3.f); }
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
	bool IsMenuOpen() const { return ReadSession().bMenuOpen; }
	bool IsMinimapVisible() const
	{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
		return ReadSession().bMinimapVisible;
#else
		return true;
#endif
	}
private:
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	void ToggleMinimap();
#endif
	void ShowMenu(bool Settings = false);
	void CloseMenu();
	void RemoveMenuWidget();
	TSharedPtr<class SWidget> MenuWidget;
	UPROPERTY(Transient) TObjectPtr<class UMazeECSSubsystem> ECSSubsystem;
	FMazeSessionFragment ReadSession() const;
};
