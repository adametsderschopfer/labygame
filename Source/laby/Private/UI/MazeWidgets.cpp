#include "UI/MazeWidgets.h"
#include "UI/MazeText.h"
#include "UI/MazeInterfacePreferences.h"
#include "Blueprint/WidgetTree.h"
#include "Player/MazeCharacter.h"
#include "Player/MazePlayerController.h"
#include "ECS/MazeVitalsSystem.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/InputKeySelector.h"
#include "Components/ProgressBar.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
	FText SensitivityText(float Value)
	{
		FNumberFormattingOptions Options;

		Options.MinimumFractionalDigits = 2;
		Options.MaximumFractionalDigits = 2;

		return FText::Format(NSLOCTEXT("Maze.Settings", "SensitivityValue", "{Value} x"),
		                     FFormatNamedArguments{{TEXT("Value"), FText::AsNumber(Value, &Options)}});
	}

	void Text(UUserWidget* Owner, const TCHAR* Name, const FText& Value)
	{
		if (auto* Widget = Cast<UTextBlock>(Owner->GetWidgetFromName(Name)))
			Widget->SetText(Value);
	}

	void Visible(UUserWidget* Owner, const TCHAR* Name, bool bVisible)
	{
		if (auto* Widget = Owner->GetWidgetFromName(Name))
			Widget->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UMazeMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);

	for (const auto& Entry : MazeText::WidgetLabels())
		Text(this, *Entry.Key.ToString(), Entry.Value);

	Text(this,
	     TEXT("Subtitle"),
	     MazeText::Subtitle(GetWidgetFromName(TEXT("SensitivitySlider")) != nullptr,
	                        GetWidgetFromName(TEXT("ResumeButton")) != nullptr));
	Visible(this, TEXT("NavigationHint"), false);

	if (auto* Button = Cast<UButton>(GetWidgetFromName(TEXT("ResumeButton"))))
		Button->OnClicked.AddUniqueDynamic(this, &UMazeMenuWidget::Resume);

	if (auto* Button = Cast<UButton>(GetWidgetFromName(TEXT("NewGameButton"))))
		Button->OnClicked.AddUniqueDynamic(this, &UMazeMenuWidget::NewGame);

	if (auto* Button = Cast<UButton>(GetWidgetFromName(TEXT("SettingsButton"))))
		Button->OnClicked.AddUniqueDynamic(this, &UMazeMenuWidget::Settings);

	if (auto* Button = Cast<UButton>(GetWidgetFromName(TEXT("BackButton"))))
		Button->OnClicked.AddUniqueDynamic(this, &UMazeMenuWidget::Back);

	if (auto* Button = Cast<UButton>(GetWidgetFromName(TEXT("QuitButton"))))
		Button->OnClicked.AddUniqueDynamic(this, &UMazeMenuWidget::Quit);

	if (auto* Button = Cast<UButton>(GetWidgetFromName(TEXT("ResetButton"))))
		Button->OnClicked.AddUniqueDynamic(this, &UMazeMenuWidget::ResetSettings);

	if (auto* Button = Cast<UButton>(GetWidgetFromName(TEXT("MainMenuButton"))))
		Button->OnClicked.AddUniqueDynamic(this, &UMazeMenuWidget::ReturnToMainMenu);

	if (auto* Button = Cast<UButton>(GetWidgetFromName(TEXT("VideoTabButton"))))
		Button->OnClicked.AddUniqueDynamic(this, &UMazeMenuWidget::ShowVideoSettings);

	if (auto* Button = Cast<UButton>(GetWidgetFromName(TEXT("ControlsTabButton"))))
		Button->OnClicked.AddUniqueDynamic(this, &UMazeMenuWidget::ShowControlSettings);

	if (auto* Button = Cast<UButton>(GetWidgetFromName(TEXT("GameTabButton"))))
		Button->OnClicked.AddUniqueDynamic(this, &UMazeMenuWidget::ShowGameSettings);

	if (auto* Slider = Cast<USlider>(GetWidgetFromName(TEXT("RenderScaleSlider"))))
		Slider->OnValueChanged.AddUniqueDynamic(this, &UMazeMenuWidget::ChangeRenderScale);

	const float Sensitivity = GetDefault<UMazePreferences>()->GetSensitivity();

	if (auto* Slider = Cast<USlider>(GetWidgetFromName(TEXT("SensitivitySlider"))))
	{
		Slider->SetMinValue(0.1f);
		Slider->SetMaxValue(3.f);
		Slider->SetStepSize(0.05f);
		Slider->SetValue(Sensitivity);
		Slider->OnValueChanged.AddUniqueDynamic(this, &UMazeMenuWidget::ChangeSensitivity);
	}

	Text(this, TEXT("SensitivityText"), SensitivityText(Sensitivity));
	ReadSettingsIntoControls();
	BindSettingsEvents();
	SelectSettingsSection(0);
}

void UMazeMenuWidget::NativeDestruct()
{
	KeyLabelScroll.Reset();

	if (WidgetTree)
		WidgetTree->ForEachWidget(
		    [this](UWidget* Widget)
		    {
			    if (auto* Button = Cast<UButton>(Widget))
				    Button->OnClicked.RemoveAll(this);

			    if (auto* Slider = Cast<USlider>(Widget))
				    Slider->OnValueChanged.RemoveAll(this);

			    if (auto* Check = Cast<UCheckBox>(Widget))
				    Check->OnCheckStateChanged.RemoveAll(this);

			    if (auto* Combo = Cast<UComboBoxString>(Widget))
				    Combo->OnSelectionChanged.RemoveAll(this);

			    if (auto* Selector = Cast<UInputKeySelector>(Widget))
				    Selector->OnKeySelected.RemoveAll(this);
		    });

	Super::NativeDestruct();
}

FReply UMazeMenuWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	// Escape cancels key capture before it can close the settings menu.
	if (IsSelectingBinding())
		return FReply::Unhandled();

	if (Event.GetKey() == EKeys::Escape)
	{
		if (auto* Controller = GetOwningPlayer<AMazePlayerController>())
		{
			Controller->ToggleMenu();

			return FReply::Handled();
		}
	}

	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

void UMazeMenuWidget::Resume()
{
	if (auto* Controller = GetOwningPlayer<AMazePlayerController>())
		Controller->CloseMenu();
}

void UMazeMenuWidget::NewGame()
{
	if (auto* Controller = GetOwningPlayer<AMazePlayerController>())
		Controller->StartNewGame();
}

void UMazeMenuWidget::Settings()
{
	if (auto* Controller = GetOwningPlayer<AMazePlayerController>())
		Controller->ShowMenu(true);
}

void UMazeMenuWidget::Back()
{
	if (auto* Controller = GetOwningPlayer<AMazePlayerController>())
		Controller->ShowMenu();
}

void UMazeMenuWidget::Quit()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UMazeMenuWidget::ChangeSensitivity(float Value)
{
	Text(this, TEXT("SensitivityText"), SensitivityText(FMath::Clamp(Value, 0.1f, 3.f)));

	if (!bReadingSettings && FMath::IsFinite(Value))
		GetMutableDefault<UMazePreferences>()->SetSensitivity(Value);
}

void UMazeHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	for (const auto& Entry : MazeText::WidgetLabels())
		Text(this, *Entry.Key.ToString(), Entry.Value);

	Visible(this, TEXT("DeathHint"), false);
	Visible(this, TEXT("ExitHint"), false);

	const TCHAR* Section = TEXT("/Script/EngineSettings.GeneralProjectSettings");
	FString Version;

	GConfig->GetString(Section, TEXT("ProjectVersion"), Version, GGameIni);

#if WITH_EDITOR
	FConfigFile CurrentGameConfig;

	if (FConfigCacheIni::LoadLocalIniFile(CurrentGameConfig, TEXT("Game"), true, nullptr, true))
		CurrentGameConfig.GetString(Section, TEXT("ProjectVersion"), Version);

#endif
	Text(this,
	     TEXT("VersionText"),
	     FText::Format(NSLOCTEXT("Maze.HUD", "Version", "ALPHA {Version}"),
	                   FFormatNamedArguments{{TEXT("Version"), FText::AsCultureInvariant(Version)}}));
	NativeTick(FGeometry(), 0.f);
}

void UMazeHUDWidget::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
	Super::NativeTick(Geometry, DeltaSeconds);

	if (IsDesignTime() || !GetWorld())
		return;

	const auto Preferences = FMazeInterfacePreferences::Read();

	Visible(this, TEXT("Crosshair"), Preferences.bShowCrosshair);
	Visible(this, TEXT("HealthText"), !Preferences.bCompactHUD);
	Visible(this, TEXT("StaminaText"), !Preferences.bCompactHUD);
	Visible(this, TEXT("HealthValueText"), !Preferences.bCompactHUD);
	Visible(this, TEXT("StaminaValueText"), !Preferences.bCompactHUD);

	const auto* Controller = GetOwningPlayer<AMazePlayerController>();

	// Hide the content, not the root: the widget must keep ticking while a menu is open.
	Visible(this, TEXT("HUDContent"), !Controller || (!Controller->IsMenuOpen() && !Controller->IsMapOpen()));

	const auto* Player = Controller ? Cast<AMazeCharacter>(Controller->GetPawn()) : nullptr;

	Visible(this, TEXT("VitalsPanel"), Player != nullptr);

	bool bDead = false;
	int32 ReachedExit = 0;

	if (Player)
	{
		const FMazeVitals Vitals = Player->GetVitals();

		bDead = !FMazeVitalsSystem::IsAlive(Vitals);
		ReachedExit = Player->GetReachedExit();
		Text(this, TEXT("HealthValueText"), FText::AsNumber(FMath::CeilToInt(Vitals.Health)));
		Text(this, TEXT("StaminaValueText"), FText::AsNumber(FMath::CeilToInt(Vitals.Stamina)));
		Text(this,
		     TEXT("StaminaText"),
		     Vitals.bExhausted ? NSLOCTEXT("Maze.HUD", "StaminaRecovering", "ВОССТАНОВЛЕНИЕ")
		                       : MazeText::Widget(TEXT("StaminaText")));

		if (auto* Bar = Cast<UProgressBar>(GetWidgetFromName(TEXT("HealthBar"))))
			Bar->SetPercent(FMath::Clamp(Vitals.Health / FMazeVitals::Maximum, 0.f, 1.f));

		if (auto* Bar = Cast<UProgressBar>(GetWidgetFromName(TEXT("StaminaBar"))))
		{
			Bar->SetPercent(FMath::Clamp(Vitals.Stamina / FMazeVitals::Maximum, 0.f, 1.f));
			Bar->SetFillColorAndOpacity(Vitals.bExhausted ? ExhaustedColor : StaminaColor);
		}
	}

	Visible(this, TEXT("DeathPanel"), bDead);
	Visible(this, TEXT("ExitPanel"), !bDead && ReachedExit != 0);
	Text(this, TEXT("ExitText"), NSLOCTEXT("Maze.Widgets", "ExitText", "ВЫХОД НАЙДЕН"));
}

int32 UMazeHUDWidget::NativePaint(const FPaintArgs& Args,
                                  const FGeometry& Geometry,
                                  const FSlateRect& CullingRect,
                                  FSlateWindowElementList& Elements,
                                  int32 Layer,
                                  const FWidgetStyle& Style,
                                  bool bParentEnabled) const
{
	return Super::NativePaint(Args, Geometry, CullingRect, Elements, Layer, Style, bParentEnabled);
}
