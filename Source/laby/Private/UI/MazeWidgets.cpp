#include "UI/MazeWidgets.h"
#include "UI/MazeText.h"
#include "UI/MazeInterfacePreferences.h"
#include "Blueprint/WidgetTree.h"
#include "Player/MazeCharacter.h"
#include "Player/MazePlayerController.h"
#include "ECS/MazeVitalsSystem.h"
#include "World/MazeWorld.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "EngineUtils.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

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

	if (auto* Button = Cast<UButton>(GetWidgetFromName(TEXT("ApplyButton"))))
		Button->OnClicked.AddUniqueDynamic(this, &UMazeMenuWidget::ApplySettings);

	if (auto* Slider = Cast<USlider>(GetWidgetFromName(TEXT("FieldOfViewSlider"))))
		Slider->OnValueChanged.AddUniqueDynamic(this, &UMazeMenuWidget::ChangeFieldOfView);

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
	SelectSettingsSection(0);
}

void UMazeMenuWidget::NativeDestruct()
{
	if (WidgetTree)
		WidgetTree->ForEachWidget(
		    [this](UWidget* Widget)
		    {
			    if (auto* Button = Cast<UButton>(Widget))
				    Button->OnClicked.RemoveAll(this);

			    if (auto* Slider = Cast<USlider>(Widget))
				    Slider->OnValueChanged.RemoveAll(this);
		    });

	Super::NativeDestruct();
}

FReply UMazeMenuWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
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
}

void UMazeMenuWidget::SaveSensitivity()
{
	ApplySettings();
}

void UMazeMenuWidget::ResetSensitivity()
{
	if (auto* Slider = Cast<USlider>(GetWidgetFromName(TEXT("SensitivitySlider"))))
		Slider->SetValue(1.f);

	ChangeSensitivity(1.f);
}

void UMazeHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	for (const auto& Entry : MazeText::WidgetLabels())
		Text(this, *Entry.Key.ToString(), Entry.Value);

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
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	Visible(this, TEXT("DeveloperHint"), false);
#endif
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

	const auto* Controller = GetOwningPlayer<AMazePlayerController>();

	// Hide the content, not the root: the widget must keep ticking while a menu is open.
	Visible(this, TEXT("HUDContent"), !Controller || !Controller->IsMenuOpen());

	const auto* Player = Controller ? Cast<AMazeCharacter>(Controller->GetPawn()) : nullptr;

	Visible(this, TEXT("VitalsPanel"), Player != nullptr);

	bool bDead = false;
	int32 ReachedExit = 0;

	if (Player)
	{
		const FMazeVitals Vitals = Player->GetVitals();

		bDead = !FMazeVitalsSystem::IsAlive(Vitals);
		ReachedExit = Player->GetReachedExit();
		Text(this,
		     TEXT("HealthText"),
		     FText::Format(NSLOCTEXT("Maze.HUD", "Health", "СОСТОЯНИЕ   {Value}"),
		                   FFormatNamedArguments{{TEXT("Value"), FMath::CeilToInt(Vitals.Health)}}));
		Text(this,
		     TEXT("StaminaText"),
		     FText::Format(Vitals.bExhausted ? NSLOCTEXT("Maze.HUD", "StaminaRecovering", "ВОССТАНОВЛЕНИЕ   {Value}")
		                                     : NSLOCTEXT("Maze.HUD", "Stamina", "ВЫНОСЛИВОСТЬ   {Value}"),
		                   FFormatNamedArguments{{TEXT("Value"), FMath::CeilToInt(Vitals.Stamina)}}));

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
	Visible(this, TEXT("MinimapPanel"), !bDead && (!Controller || Controller->IsMinimapVisible()));
	Visible(this, TEXT("SessionText"), !bDead && Controller && Controller->IsMinimapVisible());
	Visible(this, TEXT("DeveloperHint"), !bDead && Controller && Controller->IsMinimapVisible());

	for (TActorIterator<AMazeWorld> It(GetWorld()); It; ++It)
	{
		if (const auto Data = It->GetGeneratedData())
			Text(this,
			     TEXT("SessionText"),
			     FText::Format(NSLOCTEXT("Maze.HUD", "Session", "SESSION {Seed} | {Size} x {Size} | START A"),
			                   FFormatNamedArguments{{TEXT("Seed"), FText::AsCultureInvariant(LexToString(It->Seed))},
			                                         {TEXT("Size"), Data->Layout.Size}}));

		break;
	}
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

int32 UMazeMinimapWidget::NativePaint(const FPaintArgs& Args,
                                      const FGeometry& Geometry,
                                      const FSlateRect& CullingRect,
                                      FSlateWindowElementList& Elements,
                                      int32 Layer,
                                      const FWidgetStyle& Style,
                                      bool bParentEnabled) const
{
	Layer = Super::NativePaint(Args, Geometry, CullingRect, Elements, Layer, Style, bParentEnabled);
#if UE_BUILD_SHIPPING || UE_BUILD_TEST

	return Layer;

#endif

	if (!GetWorld() || IsDesignTime())
		return Layer;

	const auto* Controller = GetOwningPlayer();

	for (TActorIterator<AMazeWorld> It(GetWorld()); It; ++It)
	{
		const auto Data = It->GetGeneratedData();

		if (!Data || Data->Layout.Size <= 0 || Data->Layout.Walls.Num() != Data->Layout.Size * Data->Layout.Size)
			break;

		const FMazeLayout& Layout = Data->Layout;
		const float Size = FMath::Min(Geometry.GetLocalSize().X, Geometry.GetLocalSize().Y);
		const float Step = Size / Layout.Size;

		++Layer;

		auto Line = [&](FVector2D A, FVector2D B, FLinearColor Color, float Width = 1.f)
		{
			TArray<FVector2D> Points{A, B};
			FSlateDrawElement::MakeLines(Elements,
			                             Layer,
			                             Geometry.ToPaintGeometry(),
			                             Points,
			                             ESlateDrawEffect::None,
			                             Color * Style.GetColorAndOpacityTint(),
			                             true,
			                             Width);
		};
		auto Marker = [&](FVector2D P, FLinearColor Color)
		{
			Line(P - FVector2D(3, 0), P + FVector2D(3, 0), Color, 6.f);
		};

		for (int32 Y = 0; Y < Layout.Size; ++Y)
			for (int32 X = 0; X < Layout.Size; ++X)
			{
				const uint8 W = Layout.Walls[Y * Layout.Size + X];
				const float PX = X * Step, PY = Y * Step;

				if (!Layout.HasFloor(Y * Layout.Size + X))
				{
					const FLinearColor HoleColor(1.f, 0.3f, 0.1f);
					Line({PX + Step * 0.2f, PY + Step * 0.2f}, {PX + Step * 0.8f, PY + Step * 0.8f}, HoleColor);
					Line({PX + Step * 0.8f, PY + Step * 0.2f}, {PX + Step * 0.2f, PY + Step * 0.8f}, HoleColor);
				}

				if (W & 1)
					Line({PX, PY}, {PX + Step, PY}, WallColor);

				if (W & 8)
					Line({PX, PY}, {PX, PY + Step}, WallColor);

				if (X == Layout.Size - 1 && (W & 2))
					Line({PX + Step, PY}, {PX + Step, PY + Step}, WallColor);

				if (Y == Layout.Size - 1 && (W & 4))
					Line({PX, PY + Step}, {PX + Step, PY + Step}, WallColor);
			}

		auto Project = [&](FVector World)
		{
			const FVector Local = World - It->GetActorLocation();

			return FVector2D(FMath::Clamp(Local.X / It->GetCellSize() * Step, 0.f, Size),
			                 FMath::Clamp(Local.Y / It->GetCellSize() * Step, 0.f, Size));
		};

		++Layer;
		Marker(Project(It->StartLocation()), StartColor);

		for (int32 I = 0; I < Layout.Exits.Num(); ++I)
		{
			const int32 Cell = Layout.Exits[I];
			FVector2D P((Cell % Layout.Size + 0.5f) * Step, (Cell / Layout.Size + 0.5f) * Step);

			if (I == 0)
				P.Y = 0;

			if (I == 1)
				P.X = Size;

			if (I == 2)
				P.Y = Size;

			Marker(P, ExitColor);
		}

		if (Controller && Controller->GetPawn())
		{
			const FVector2D P = Project(Controller->GetPawn()->GetActorLocation());
			const float Angle = FMath::DegreesToRadians(Controller->GetControlRotation().Yaw);
			const FVector2D Direction(FMath::Cos(Angle), FMath::Sin(Angle)), Side(-Direction.Y, Direction.X);
			const FVector2D Tip = P + Direction * 8, A = P - Direction * 5 + Side * 4, B = P - Direction * 5 - Side * 4;

			++Layer;
			Line(Tip, A, PlayerColor, 2);
			Line(Tip, B, PlayerColor, 2);
			Line(A, B, PlayerColor, 2);
		}

		break;
	}

	return Layer;
}
