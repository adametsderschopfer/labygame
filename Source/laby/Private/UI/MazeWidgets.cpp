#include "UI/MazeWidgets.h"
#include "UI/MazeText.h"
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
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"
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
		Button->OnClicked.AddUniqueDynamic(this, &UMazeMenuWidget::ResetSensitivity);

	const float Sensitivity = GetDefault<UMazePreferences>()->GetSensitivity();

	if (auto* Slider = Cast<USlider>(GetWidgetFromName(TEXT("SensitivitySlider"))))
	{
		Slider->SetMinValue(0.1f);
		Slider->SetMaxValue(3.f);
		Slider->SetStepSize(0.05f);
		Slider->SetValue(Sensitivity);
		Slider->OnValueChanged.AddUniqueDynamic(this, &UMazeMenuWidget::ChangeSensitivity);
		Slider->OnMouseCaptureEnd.AddUniqueDynamic(this, &UMazeMenuWidget::SaveSensitivity);
		Slider->OnControllerCaptureEnd.AddUniqueDynamic(this, &UMazeMenuWidget::SaveSensitivity);
	}

	Text(this, TEXT("SensitivityText"), SensitivityText(Sensitivity));
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
	GetMutableDefault<UMazePreferences>()->MouseSensitivity = FMath::Clamp(Value, 0.1f, 3.f);
	Text(this, TEXT("SensitivityText"), SensitivityText(GetDefault<UMazePreferences>()->GetSensitivity()));
}

void UMazeMenuWidget::SaveSensitivity()
{
	auto* Preferences = GetMutableDefault<UMazePreferences>();

	Preferences->SetSensitivity(Preferences->GetSensitivity());
}

void UMazeMenuWidget::ResetSensitivity()
{
	GetMutableDefault<UMazePreferences>()->SetSensitivity(1.f);

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
		     FText::Format(NSLOCTEXT("Maze.HUD", "Health", "HEALTH  {Value} / 100"),
		                   FFormatNamedArguments{{TEXT("Value"), FMath::CeilToInt(Vitals.Health)}}));
		Text(this,
		     TEXT("StaminaText"),
		     FText::Format(Vitals.bExhausted
		                       ? NSLOCTEXT("Maze.HUD", "StaminaRecovering", "STAMINA / RECOVERING  {Value} / 100")
		                       : NSLOCTEXT("Maze.HUD", "Stamina", "STAMINA  {Value} / 100"),
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
	Text(this, TEXT("ExitText"), NSLOCTEXT("Maze.Widgets", "ExitText", "EXIT REACHED"));
	Visible(this, TEXT("MinimapPanel"), !bDead && (!Controller || Controller->IsMinimapVisible()));
	Visible(this, TEXT("SessionText"), !bDead);

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
	Layer = Super::NativePaint(Args, Geometry, CullingRect, Elements, Layer, Style, bParentEnabled);

	const auto* Controller = GetOwningPlayer<AMazePlayerController>();
	const auto* Player = Controller ? Cast<AMazeCharacter>(Controller->GetPawn()) : nullptr;

	if (IsDesignTime() || !Player || Controller->IsMenuOpen() || !FMazeVitalsSystem::IsAlive(Player->GetVitals()))
		return Layer;

	const float Width = FMath::Min(560.f, Geometry.GetLocalSize().X - 80.f);

	if (Width <= 0.f)
		return Layer;

	const float Center = Geometry.GetLocalSize().X * 0.5f;
	const float Top = 24.f;
	const float HalfArc = 90.f;
	// The minimap projects +X right and -Y up, so north is world -Y.
	const float Heading = FRotator::ClampAxis(Controller->GetControlRotation().Yaw + 90.f);
	const FLinearColor Tint = Style.GetColorAndOpacityTint();
	const FLinearColor White(0.92f, 0.95f, 1.f);
	const FLinearColor Accent(1.f, 0.75f, 0.25f);
	const auto FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FSlateFontInfo CardinalFont = FCoreStyle::GetDefaultFontStyle("Bold", 18);
	const FSlateFontInfo IntermediateFont = FCoreStyle::GetDefaultFontStyle("Regular", 13);
	const TCHAR* Directions[] = {
	    TEXT("N"), TEXT("NE"), TEXT("E"), TEXT("SE"), TEXT("S"), TEXT("SW"), TEXT("W"), TEXT("NW")};

	++Layer;

	auto Line = [&](FVector2D A, FVector2D B, FLinearColor Color, float Thickness)
	{
		const TArray<FVector2D> ShadowPoints{A + FVector2D(0, 1), B + FVector2D(0, 1)};
		const TArray<FVector2D> Points{A, B};
		FSlateDrawElement::MakeLines(Elements,
		                             Layer,
		                             Geometry.ToPaintGeometry(),
		                             ShadowPoints,
		                             ESlateDrawEffect::None,
		                             FLinearColor(0.f, 0.f, 0.f, Color.A * 0.7f) * Tint,
		                             true,
		                             Thickness + 2.f);
		FSlateDrawElement::MakeLines(Elements,
		                             Layer + 1,
		                             Geometry.ToPaintGeometry(),
		                             Points,
		                             ESlateDrawEffect::None,
		                             Color * Tint,
		                             true,
		                             Thickness);
	};

	// Wrap each tick relative to the camera to keep the tape continuous across north.
	for (int32 Tick = 0; Tick < 72; ++Tick)
	{
		const float Delta = FMath::FindDeltaAngleDegrees(Heading, Tick * 5.f);

		if (FMath::Abs(Delta) >= HalfArc)
			continue;

		const float X = Center + Delta / HalfArc * Width * 0.5f;
		const float Alpha = FMath::Clamp((HalfArc - FMath::Abs(Delta)) / 20.f, 0.f, 1.f);
		const bool bDirection = Tick % 9 == 0;
		const float Height = bDirection ? 12.f : (Tick % 3 == 0 ? 8.f : 4.f);
		FLinearColor Color = Tick == 0 ? Accent : White;

		Color.A = Alpha;
		Line({X, Top + 32.f}, {X, Top + 32.f + Height}, Color, bDirection ? 1.5f : 1.f);

		if (bDirection)
		{
			const FString Label(Directions[Tick / 9]);
			const auto& Font = Tick % 18 == 0 ? CardinalFont : IntermediateFont;
			const FVector2D Size = FontMeasure->Measure(Label, Font);
			const FVector2D Position(X - Size.X * 0.5f, Top + 17.f - Size.Y * 0.5f);

			FSlateDrawElement::MakeText(
			    Elements,
			    Layer,
			    Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Position + FVector2D(1, 1))),
			    Label,
			    Font,
			    ESlateDrawEffect::None,
			    FLinearColor(0.f, 0.f, 0.f, Alpha * 0.8f) * Tint);
			FSlateDrawElement::MakeText(Elements,
			                            Layer + 1,
			                            Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Position)),
			                            Label,
			                            Font,
			                            ESlateDrawEffect::None,
			                            Color * Tint);
		}
	}

	// Fixed opposing chevrons frame the current look direction without degree numbers.
	Layer += 2;
	Line({Center - 5.f, Top - 5.f}, {Center, Top}, Accent, 2.f);
	Line({Center, Top}, {Center + 5.f, Top - 5.f}, Accent, 2.f);
	Line({Center - 4.f, Top + 53.f}, {Center, Top + 48.f}, Accent, 2.f);
	Line({Center, Top + 48.f}, {Center + 4.f, Top + 53.f}, Accent, 2.f);

	return Layer + 1;
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
