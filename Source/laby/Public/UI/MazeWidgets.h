#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Framework/Commands/InputChord.h"
#include "MazeWidgets.generated.h"

// These widgets only present ECS snapshots and forward input to the controller.
UCLASS(Abstract, Blueprintable)
class LABY_API UMazeMenuWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
	virtual int32 NativePaint(const FPaintArgs& Args,
	                          const FGeometry& Geometry,
	                          const FSlateRect& CullingRect,
	                          FSlateWindowElementList& Elements,
	                          int32 Layer,
	                          const FWidgetStyle& Style,
	                          bool bParentEnabled) const override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

private:
	bool bReadingSettings = false;
	struct FKeyLabelScroll
	{
		FString Text;
		float Width = 0.f;
		float Elapsed = 0.f;
	};
	TMap<FName, FKeyLabelScroll> KeyLabelScroll;

	void BindSettingsEvents();
	void SaveVideoSettings();
	void ReadKeyBindings();
	void ClearBindingInput();
	bool IsSelectingBinding() const;
	UFUNCTION()
	void ChangeCheckSetting(bool bChecked);
	UFUNCTION()
	void ChangeVideoOption(FString Selected, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void ChangeQuality(FString Selected, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void ChangeBinding(FInputChord SelectedKey);
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
	void ChangeSensitivity(float Value);
	UFUNCTION()
	void ReturnToMainMenu();
	UFUNCTION()
	void ShowVideoSettings();
	UFUNCTION()
	void ShowControlSettings();
	UFUNCTION()
	void ShowGameSettings();
	UFUNCTION()
	void ApplySettings();
	UFUNCTION()
	void ResetSettings();
	UFUNCTION()
	void ChangeRenderScale(float Value);
	void SelectSettingsSection(int32 Index);
	void ReadSettingsIntoControls();
};

UCLASS(Abstract, Blueprintable)
class LABY_API UMazeHUDWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
	virtual int32 NativePaint(const FPaintArgs& Args,
	                          const FGeometry& Geometry,
	                          const FSlateRect& CullingRect,
	                          FSlateWindowElementList& Elements,
	                          int32 Layer,
	                          const FWidgetStyle& Style,
	                          bool bParentEnabled) const override;

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Maze|Style")
	FLinearColor StaminaColor = FLinearColor(0.2f, 0.85f, 0.65f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Maze|Style")
	FLinearColor ExhaustedColor = FLinearColor(1.f, 0.55f, 0.12f);
};
