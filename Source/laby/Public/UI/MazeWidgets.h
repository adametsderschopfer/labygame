#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MazeWidgets.generated.h"

// These widgets only present ECS snapshots and forward input to the controller.
UCLASS(Abstract, Blueprintable)
class LABY_API UMazeMenuWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

private:
	UFUNCTION()
	void Resume();
	UFUNCTION()
	void NewGame();
	UFUNCTION()
	void Settings();
	UFUNCTION()
	void Back();
	UFUNCTION()
	void Quit();
	UFUNCTION()
	void ResetSensitivity();
	UFUNCTION()
	void ChangeSensitivity(float Value);
	UFUNCTION()
	void SaveSensitivity();
};

UCLASS(Abstract, Blueprintable)
class LABY_API UMazeHUDWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Maze|Style")
	FLinearColor StaminaColor = FLinearColor(0.2f, 0.85f, 0.65f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Maze|Style")
	FLinearColor ExhaustedColor = FLinearColor(1.f, 0.55f, 0.12f);
};

// Place this native widget in a Widget Blueprint; its geometry follows the designer slot.
UCLASS(Blueprintable)
class LABY_API UMazeMinimapWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Style")
	FLinearColor WallColor = FLinearColor(0.65f, 0.71f, 0.76f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Style")
	FLinearColor StartColor = FLinearColor(0.15f, 0.5f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Style")
	FLinearColor ExitColor = FLinearColor(0.2f, 1.f, 0.4f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Maze|Style")
	FLinearColor PlayerColor = FLinearColor(1.f, 0.75f, 0.12f);

protected:
	virtual int32 NativePaint(const FPaintArgs& Args,
	                          const FGeometry& Geometry,
	                          const FSlateRect& CullingRect,
	                          FSlateWindowElementList& Elements,
	                          int32 Layer,
	                          const FWidgetStyle& Style,
	                          bool bParentEnabled) const override;
};
