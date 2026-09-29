#include "UI/MazeWidgets.h"
#include "UI/MazeExplorationMapWidget.h"
#include "UI/MazeInterfaceStyle.h"
#include "World/MazePreparationStatus.h"
#include "Player/MazeKeyBindings.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/InputKeySelector.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Slate/WidgetRenderer.h"
#include "Sound/SoundBase.h"
#include "Styling/CoreStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "WidgetBlueprint.h"

// A separate asset family, constructed from empty WidgetTrees. Legacy WBPs are never loaded or modified.
namespace MazeWardUI
{
	FLinearColor Ink()
	{
		return MazeInterfaceStyle::Palette().Ink;
	}

	FLinearColor Muted()
	{
		return MazeInterfaceStyle::Palette().Muted;
	}

	FLinearColor Mint()
	{
		return MazeInterfaceStyle::Palette().Accent;
	}

	FLinearColor Edge()
	{
		return MazeInterfaceStyle::Palette().Edge.CopyWithNewOpacity(.75f);
	}

	FLinearColor Glass()
	{
		return MazeInterfaceStyle::Palette().Glass.CopyWithNewOpacity(.83f);
	}

	FLinearColor HealthFill()
	{
		return FLinearColor(.31f, .16f, .17f);
	}

	FLinearColor StaminaFill()
	{
		return FLinearColor(.15f, .23f, .34f);
	}

	FLinearColor ExhaustedStaminaFill()
	{
		return FLinearColor(.09f, .13f, .18f);
	}

	FSlateBrush Panel(FLinearColor Fill, FLinearColor Line, float Radius = 3)
	{
		FSlateBrush B = *FCoreStyle::Get().GetBrush("WhiteBrush");

		B.TintColor = Fill;
		B.DrawAs = ESlateBrushDrawType::RoundedBox;
		B.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		B.OutlineSettings.Width = Line.A > 0 ? 1 : 0;
		B.OutlineSettings.Color = Line;

		return B;
	}

	template <typename T> T* Make(UWidgetTree* TTree, const FString& Name)
	{
		T* W = TTree->ConstructWidget<T>(T::StaticClass(), *Name);

		W->bIsVariable = true;
		W->SetFlags(RF_Transactional);

		return W;
	}

	UCanvasPanelSlot* Put(UCanvasPanel* Parent,
	                      UWidget* W,
	                      FVector2D P,
	                      FVector2D S,
	                      FVector2D Anchor = {0, 0},
	                      FVector2D Alignment = {0, 0})
	{
		auto* Slot = Parent->AddChildToCanvas(W);

		Slot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
		Slot->SetAlignment(Alignment);
		Slot->SetPosition(P);
		Slot->SetSize(S);

		return Slot;
	}

	void Fill(UCanvasPanel* Parent, UWidget* W)
	{
		auto* Slot = Parent->AddChildToCanvas(W);

		Slot->SetAnchors(FAnchors(0, 0, 1, 1));
		Slot->SetOffsets(FMargin(0));
	}

	UTextBlock* Text(UWidgetTree* Tree,
	                 UCanvasPanel* Parent,
	                 const FString& Name,
	                 FText Value,
	                 FVector2D P,
	                 FVector2D S,
	                 int Size = 20,
	                 FLinearColor Tint = Ink())
	{
		auto* W = Make<UTextBlock>(Tree, Name);
		auto Font = W->GetFont();

		Font.TypefaceFontName = TEXT("Light");
		Font.Size = Size;
		Font.LetterSpacing = 0;
		W->SetFont(Font);
		W->SetText(Value);
		W->SetColorAndOpacity(Tint);
		W->SetVisibility(ESlateVisibility::HitTestInvisible);
		Put(Parent, W, P, S);

		return W;
	}

	UImage* Shape(UWidgetTree* Tree,
	              UCanvasPanel* Parent,
	              const FString& Name,
	              FVector2D P,
	              FVector2D S,
	              FLinearColor Tint,
	              FLinearColor Line = FLinearColor::Transparent,
	              float Radius = 0)
	{
		auto* W = Make<UImage>(Tree, Name);

		W->SetBrush(Panel(Tint, Line, Radius));
		W->SetVisibility(ESlateVisibility::HitTestInvisible);
		Put(Parent, W, P, S);

		return W;
	}

	UImage* Stroke(UWidgetTree* Tree,
	               UCanvasPanel* Parent,
	               const FString& Name,
	               FVector2D A,
	               FVector2D B,
	               FLinearColor Tint,
	               float Width = 2)
	{
		const FVector2D D = B - A;
		auto* W = Shape(Tree, Parent, Name, (A + B) * .5, FVector2D(D.Size(), Width), Tint);

		CastChecked<UCanvasPanelSlot>(W->Slot)->SetAlignment({.5, .5});
		W->SetRenderTransformAngle(FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)));

		return W;
	}

	UButton* Button(UWidgetTree* Tree,
	                UCanvasPanel* Parent,
	                const TCHAR* Name,
	                FText Caption,
	                FVector2D P,
	                FVector2D S,
	                bool Primary = false,
	                bool CloseIcon = false,
	                bool CenteredArrow = false)
	{
		auto* W = Make<UButton>(Tree, Name);
		FButtonStyle Style;
		const FLinearColor Dark = MazeInterfaceStyle::Palette().OnAccent;

		Style.SetNormal(Panel(Primary ? Mint() : Glass(), Primary ? Mint() : Edge()));
		Style.SetHovered(Panel(MazeInterfaceStyle::Palette().HoverAccent, Mint()));
		Style.SetPressed(Panel(MazeInterfaceStyle::Palette().Pressed, Mint()));
		Style.SetDisabled(Panel(Glass(), Muted().CopyWithNewOpacity(.3)));
		Style.SetNormalForeground(Primary ? Dark : Ink()).SetHoveredForeground(Dark).SetPressedForeground(Dark);
		Style.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));

		for (bool Hover : {true, false})
		{
			auto* Sound = LoadObject<USoundBase>(
			    nullptr, Hover ? TEXT("/Game/UI/Glass/S_UIHover") : TEXT("/Game/UI/Glass/S_UIPress"));
			FSlateSound Snd;

			Snd.SetResourceObject(Sound);

			if (Hover)
				Style.SetHoveredSound(Snd);
			else
				Style.SetPressedSound(Snd);
		}

		W->SetStyle(Style);
		W->SetCursor(EMouseCursor::Hand);

		if (CloseIcon || CenteredArrow)
		{
			// Auto-sized content keeps the caption and icon together at every DPI scale.
			auto* Group = Make<UHorizontalBox>(Tree, FString(Name) + TEXT("Content"));
			auto* Label = Make<UTextBlock>(Tree, FString(Name) + TEXT("Caption"));
			auto Font = Label->GetFont();

			Group->SetVisibility(ESlateVisibility::HitTestInvisible);
			Font.TypefaceFontName = TEXT("Light");
			Font.Size = CloseIcon ? 18 : 22;
			Font.LetterSpacing = 0;
			Label->SetFont(Font);
			Label->SetText(Caption);
			Label->SetColorAndOpacity(FSlateColor::UseForeground());

			auto* CaptionSlot = Group->AddChildToHorizontalBox(Label);

			CaptionSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			CaptionSlot->SetVerticalAlignment(VAlign_Center);

			auto* Icon = Make<USizeBox>(Tree, FString(Name) + TEXT("Icon"));
			auto* Lines = Make<UCanvasPanel>(Tree, FString(Name) + TEXT("IconLines"));

			Icon->SetWidthOverride(14);
			Icon->SetHeightOverride(14);
			Icon->SetContent(Lines);

			if (CloseIcon)
			{
				Stroke(Tree, Lines, FString(Name) + TEXT("CrossA"), {2, 2}, {12, 12}, Dark, 1.5f);
				Stroke(Tree, Lines, FString(Name) + TEXT("CrossB"), {2, 12}, {12, 2}, Dark, 1.5f);
			}
			else
			{
				// Equal geometric arms avoid font baseline offsets; inherit all button-state colors.
				Stroke(Tree, Lines, FString(Name) + TEXT("ArrowA"), {4, 1}, {10, 7}, Ink(), 1.5f)
				    ->SetBrushTintColor(FSlateColor::UseForeground());
				Stroke(Tree, Lines, FString(Name) + TEXT("ArrowB"), {10, 7}, {4, 13}, Ink(), 1.5f)
				    ->SetBrushTintColor(FSlateColor::UseForeground());
			}

			auto* IconSlot = Group->AddChildToHorizontalBox(Icon);

			IconSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			IconSlot->SetPadding(FMargin(10, 0, 0, 0));
			IconSlot->SetVerticalAlignment(VAlign_Center);

			W->SetContent(Group);

			auto* ContentSlot = CastChecked<UButtonSlot>(Group->Slot);

			ContentSlot->SetPadding(FMargin(0));
			ContentSlot->SetHorizontalAlignment(HAlign_Center);
			ContentSlot->SetVerticalAlignment(VAlign_Center);
			Put(Parent, W, P, S);

			return W;
		}

		auto* Items = Make<UCanvasPanel>(Tree, FString(Name) + TEXT("Content"));
		// Authored captions use a different name from legacy text overrides.
		auto* Label =
		    Text(Tree, Items, FString(Name) + TEXT("Caption"), Caption, {26, S.Y * .5 - 17}, {S.X - 80, 40}, 22);

		Label->SetColorAndOpacity(FSlateColor::UseForeground());

		{
			auto* Chevron = Text(Tree,
			                     Items,
			                     FString(Name) + TEXT("Arrow"),
			                     FText::AsCultureInvariant(TEXT("›")),
			                     {S.X - 42, S.Y * .5 - 24},
			                     {24, 46},
			                     32);

			Chevron->SetColorAndOpacity(FSlateColor::UseForeground());
		}

		W->SetContent(Items);
		CastChecked<UButtonSlot>(Items->Slot)->SetPadding(FMargin(0));
		CastChecked<UButtonSlot>(Items->Slot)->SetHorizontalAlignment(HAlign_Fill);
		CastChecked<UButtonSlot>(Items->Slot)->SetVerticalAlignment(VAlign_Fill);
		Put(Parent, W, P, S);

		return W;
	}

	UCanvasPanel* Stage(UWidgetTree* Tree)
	{
		auto* Scale = Make<UScaleBox>(Tree, TEXT("WardResponsiveLayout"));

		Scale->SetStretch(EStretch::ScaleToFit);
		Tree->RootWidget = Scale;

		auto* Bounds = Make<USizeBox>(Tree, TEXT("WardDesignSize"));

		Bounds->SetWidthOverride(1920);
		Bounds->SetHeightOverride(1080);
		Scale->SetContent(Bounds);

		auto* Canvas = Make<UCanvasPanel>(Tree, TEXT("WardStage"));

		Bounds->SetContent(Canvas);

		return Canvas;
	}

	void Branding(UWidgetTree* Tree, UCanvasPanel* Canvas)
	{
		auto* Logo = Text(
		    Tree, Canvas, TEXT("WardLogo"), NSLOCTEXT("Maze.Ward", "Logo", "LABY"), {130, 190}, {470, 125}, 78, Ink());
		auto Font = Logo->GetFont();

		Font.LetterSpacing = 480;
		Logo->SetFont(Font);

		FString Version;

		GConfig->GetString(
		    TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), Version, GGameIni);

		auto* VersionLabel =
		    Text(Tree,
		         Canvas,
		         TEXT("WardVersion"),
		         FText::Format(NSLOCTEXT("Maze.Ward", "Version", "Версия {0}"), FText::FromString(Version)),
		         {1520, 1015},
		         {320, 30},
		         14,
		         Muted());

		VersionLabel->SetJustification(ETextJustify::Right);
	}

	void BuildMenu(UWidgetTree* Tree, bool Pause)
	{
		auto* Canvas = Stage(Tree);

		Branding(Tree, Canvas);

		double Y = 390;

		if (Pause)
		{
			Button(Tree,
			       Canvas,
			       TEXT("ResumeButton"),
			       NSLOCTEXT("Maze.Ward", "Continue", "Продолжить"),
			       {132, Y},
			       {320, 74},
			       true);
			Y += 92;
		}
		else
		{
			Button(Tree,
			       Canvas,
			       TEXT("NewGameButton"),
			       NSLOCTEXT("Maze.Ward", "NewGame", "Новая игра"),
			       {132, Y},
			       {320, 74},
			       true,
			       false,
			       true);
			Y += 92;
		}

		Button(Tree,
		       Canvas,
		       TEXT("SettingsButton"),
		       NSLOCTEXT("Maze.Ward", "Settings", "Настройки"),
		       {132, Y},
		       {320, 74},
		       false,
		       false,
		       !Pause);
		Y += 92;

		if (Pause)
		{
			Button(Tree,
			       Canvas,
			       TEXT("MainMenuButton"),
			       NSLOCTEXT("Maze.Ward", "MainMenu", "Главное меню"),
			       {132, Y},
			       {320, 74});
			Y += 92;
		}

		Button(Tree,
		       Canvas,
		       TEXT("QuitButton"),
		       NSLOCTEXT("Maze.Ward", "Quit", "Выход"),
		       {132, Y},
		       {320, 74},
		       false,
		       false,
		       !Pause);
	}

	void Caption(UWidgetTree* Tree, UCanvasPanel* Parent, const TCHAR* Name, FText Value, float Y)
	{
		Text(Tree, Parent, Name, Value, {0, Y + 12}, {490, 34}, 20);
	}

	void Slider(UWidgetTree* Tree,
	            UCanvasPanel* Parent,
	            const TCHAR* Name,
	            const TCHAR* ValueName,
	            float Y,
	            float Min,
	            float Max,
	            float Value)
	{
		auto* W = Make<USlider>(Tree, Name);

		W->SetMinValue(Min);
		W->SetMaxValue(Max);
		W->SetValue(Value);
		W->SetSliderBarColor(Muted());
		W->SetSliderHandleColor(Mint());

		FSliderStyle Style = W->GetWidgetStyle();

		Style.BarThickness = 2;
		Style.NormalThumbImage = Panel(Mint(), Mint(), 0);
		Style.NormalThumbImage.ImageSize = FVector2D(5, 20);
		Style.HoveredThumbImage = Style.NormalThumbImage;
		W->SetWidgetStyle(Style);
		Put(Parent, W, {520, Y + 9}, {300, 42});
		Text(Tree, Parent, ValueName, FText::AsCultureInvariant(TEXT("100%")), {850, Y + 12}, {90, 34}, 20, Mint());
	}

	void Check(UWidgetTree* Tree, UCanvasPanel* Parent, const TCHAR* Name, float Y)
	{
		auto* W = Make<UCheckBox>(Tree, Name);
		auto Style = W->GetWidgetStyle();
		auto Off = Panel(Glass(), Edge(), 1);

		Off.ImageSize = FVector2D(24, 24);

		auto On = Panel(Mint(), Mint(), 1);

		On.ImageSize = FVector2D(24, 24);
		Style.SetUncheckedImage(Off).SetUncheckedHoveredImage(On).SetUncheckedPressedImage(On);
		Style.SetCheckedImage(On).SetCheckedHoveredImage(On).SetCheckedPressedImage(Off);
		W->SetWidgetStyle(Style);
		Put(Parent, W, {520, Y + 8}, {44, 42});
	}

	void Combo(UWidgetTree* Tree, UCanvasPanel* Parent, const TCHAR* Name, float Y, const TArray<FText>& Options)
	{
		auto* W = Make<UComboBoxString>(Tree, Name);

		if (auto* Prop = FindFProperty<FArrayProperty>(W->GetClass(), TEXT("DefaultOptions")))
			for (const auto& Option : Options)
				Prop->ContainerPtrToValuePtr<TArray<FString>>(W)->Add(Option.ToString());

		for (const auto& Option : Options)
			W->AddOption(Option.ToString());

		W->SetSelectedIndex(2);
		W->SetContentPadding(FMargin(16, 8));

		auto Style = W->GetWidgetStyle();
		auto Normal = Panel(Glass(), Edge());
		auto Hover = Panel(MazeInterfaceStyle::Palette().HoverSurface, Mint());

		Style.ComboButtonStyle.ButtonStyle.SetNormal(Normal).SetHovered(Hover).SetPressed(Hover);
		Style.ComboButtonStyle.SetMenuBorderBrush(Normal);
		Style.ComboButtonStyle.DownArrowImage.TintColor = Mint();
		W->SetWidgetStyle(Style);

		auto Row = W->GetItemStyle();

		Row.SetEvenRowBackgroundBrush(Normal).SetOddRowBackgroundBrush(Normal);
		Row.SetEvenRowBackgroundHoveredBrush(Hover).SetOddRowBackgroundHoveredBrush(Hover);
		Row.SetActiveBrush(Hover).SetInactiveBrush(Hover).SetTextColor(Ink()).SetSelectedTextColor(Mint());
		W->SetItemStyle(Row);

		if (auto* Prop = FindFProperty<FStructProperty>(W->GetClass(), TEXT("Font")))
		{
			auto Font = GetDefault<UTextBlock>()->GetFont();

			Font.Size = 18;
			Font.TypefaceFontName = TEXT("Light");
			*Prop->ContainerPtrToValuePtr<FSlateFontInfo>(W) = Font;
		}

		if (auto* Prop = FindFProperty<FStructProperty>(W->GetClass(), TEXT("ForegroundColor")))
			*Prop->ContainerPtrToValuePtr<FSlateColor>(W) = Ink();

		Put(Parent, W, {520, Y}, {420, 58});
	}

	void BuildSettings(UWidgetTree* Tree)
	{
		auto* Canvas = Stage(Tree);

		Branding(Tree, Canvas);
		Shape(Tree, Canvas, TEXT("WardSettingsGlass"), {610, 120}, {1200, 790}, Glass(), Edge(), 6);
		Text(Tree,
		     Canvas,
		     TEXT("WardSettingsTitle"),
		     NSLOCTEXT("Maze.Ward", "Settings", "Настройки"),
		     {655, 150},
		     {450, 60},
		     34);

		const TCHAR* Names[] = {
		    TEXT("VideoTabButton"), TEXT("ControlsTabButton"), TEXT("GameTabButton"), TEXT("AudioTabButton")};
		const FText Titles[] = {NSLOCTEXT("Maze.Ward", "Video", "Видео"),
		                        NSLOCTEXT("Maze.Ward", "Controls", "Управление"),
		                        NSLOCTEXT("Maze.Ward", "Game", "Игра"),
		                        NSLOCTEXT("Maze.Ward", "Audio", "Звук")};

		for (int I = 0; I < 4; ++I)
		{
			Button(Tree, Canvas, Names[I], Titles[I], {132, 390. + I * 92}, {320, 74});

			auto* Indicator =
			    Shape(Tree, Canvas, FString(Names[I]) + TEXT("Indicator"), {133, 404. + I * 92}, {3, 46}, Mint());

			Indicator->SetVisibility(I == 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		}

		Text(Tree, Canvas, TEXT("SettingsSectionTitle"), Titles[0], {665, 255}, {1000, 40}, 22, Mint());
		Shape(Tree, Canvas, TEXT("WardSettingsRule"), {655, 318}, {1110, 1}, Edge().CopyWithNewOpacity(.4));

		auto* Pages = Make<UWidgetSwitcher>(Tree, TEXT("SettingsPages"));

		Put(Canvas, Pages, {665, 355}, {1075, 505});

		UCanvasPanel* Page[4];

		for (int I = 0; I < 4; ++I)
		{
			Page[I] = Make<UCanvasPanel>(Tree, FString::Printf(TEXT("WardPage%d"), I));
			Pages->AddChild(Page[I]);
		}

		const TArray<FText> Quality = {NSLOCTEXT("Maze.Ward", "Low", "Низкое"),
		                               NSLOCTEXT("Maze.Ward", "Medium", "Среднее"),
		                               NSLOCTEXT("Maze.Ward", "High", "Высокое"),
		                               NSLOCTEXT("Maze.Ward", "Epic", "Эпическое"),
		                               NSLOCTEXT("Maze.Ward", "Custom", "Пользовательское")};

		Caption(Tree, Page[0], TEXT("WardQualityLabel"), NSLOCTEXT("Maze.Ward", "Quality", "Качество графики"), 0);
		Combo(Tree, Page[0], TEXT("QualityCombo"), 0, Quality);
		Caption(Tree, Page[0], TEXT("WardShadowsLabel"), NSLOCTEXT("Maze.Ward", "Shadows", "Качество теней"), 92);

		auto Shadows = Quality;

		Shadows[4] = NSLOCTEXT("Maze.Ward", "Cinematic", "Кинематографическое");
		Combo(Tree, Page[0], TEXT("ShadowsCombo"), 92, Shadows);
		Caption(Tree, Page[0], TEXT("WardFPSLabel"), NSLOCTEXT("Maze.Ward", "FPS", "Частота кадров"), 184);
		Combo(Tree,
		      Page[0],
		      TEXT("FrameLimitCombo"),
		      184,
		      {FText::AsCultureInvariant(TEXT("60 FPS")),
		       FText::AsCultureInvariant(TEXT("90 FPS")),
		       FText::AsCultureInvariant(TEXT("120 FPS")),
		       FText::AsCultureInvariant(TEXT("144 FPS")),
		       NSLOCTEXT("Maze.Ward", "Unlimited", "Без ограничения"),
		       NSLOCTEXT("Maze.Ward", "Current", "Текущее значение")});
		Caption(Tree, Page[0], TEXT("WardRenderLabel"), NSLOCTEXT("Maze.Ward", "Render", "Масштаб рендера"), 276);
		Slider(Tree, Page[0], TEXT("RenderScaleSlider"), TEXT("RenderScaleText"), 276, 25, 100, 100);
		Caption(
		    Tree, Page[0], TEXT("WardVSyncLabel"), NSLOCTEXT("Maze.Ward", "VSync", "Вертикальная синхронизация"), 368);
		Check(Tree, Page[0], TEXT("VSyncCheck"), 368);
		Caption(Tree,
		        Page[1],
		        TEXT("WardSensitivityLabel"),
		        NSLOCTEXT("Maze.Ward", "Sensitivity", "Чувствительность мыши"),
		        0);
		Slider(Tree, Page[1], TEXT("SensitivitySlider"), TEXT("SensitivityText"), 0, .1, 3, 1);
		Caption(
		    Tree, Page[1], TEXT("WardInvertLabel"), NSLOCTEXT("Maze.Ward", "Invert", "Инверсия мыши по вертикали"), 72);
		Check(Tree, Page[1], TEXT("InvertYCheck"), 72);

		int I = 0;
		const int Rows = FMath::DivideAndRoundUp(MazeKeyBindings::Definitions().Num(), 2);

		for (const auto& Binding : MazeKeyBindings::Definitions())
		{
			const double X = (I / Rows) * 535., Y = 154. + (I % Rows) * 46.;
			const FString Name = Binding.WidgetName().ToString();

			Text(Tree, Page[1], Name + TEXT("Label"), Binding.Label, {X, Y + 8}, {275, 34}, 18);

			auto* W = Make<UInputKeySelector>(Tree, Name);

			W->SetAllowModifierKeys(false);
			W->SetAllowGamepadKeys(false);
			W->SetEscapeKeys({EKeys::Escape});
			W->SetSelectedKey(FInputChord(Binding.DefaultKey));
			W->SetTextBlockVisibility(ESlateVisibility::Hidden);
			W->SetKeySelectionText(NSLOCTEXT("Maze.Keys", "PressKey", "Нажмите клавишу"));

			auto Style = W->GetButtonStyle();

			Style.SetNormal(Panel(Glass(), Edge()))
			    .SetHovered(Panel(MazeInterfaceStyle::Palette().HoverSurface, Mint()));
			W->SetButtonStyle(Style);

			auto TextStyle = W->GetTextStyle();
			auto Font = GetDefault<UTextBlock>()->GetFont();

			Font.Size = 17;
			Font.TypefaceFontName = TEXT("Light");
			Font.LetterSpacing = 0;
			TextStyle.SetFont(Font).SetColorAndOpacity(Ink());
			W->SetTextStyle(TextStyle);
			Put(Page[1], W, {X + 275, Y}, {210, 42});
			++I;
		}

		Text(Tree, Page[1], TEXT("KeyBindingStatus"), FText::GetEmpty(), {0, 480}, {1030, 24}, 15, Mint());
		Caption(Tree, Page[2], TEXT("WardCompassLabel"), NSLOCTEXT("Maze.Ward", "Compass", "Компас на карте"), 0);
		Check(Tree, Page[2], TEXT("CompassCheck"), 0);
		Caption(Tree,
		        Page[2],
		        TEXT("WardCrosshairLabel"),
		        NSLOCTEXT("Maze.Ward", "Crosshair", "Точка в центре экрана"),
		        92);
		Check(Tree, Page[2], TEXT("CrosshairCheck"), 92);
		Caption(Tree, Page[2], TEXT("WardCameraLabel"), NSLOCTEXT("Maze.Ward", "Camera", "Покачивание камеры"), 184);
		Check(Tree, Page[2], TEXT("CameraMotionCheck"), 184);
		Caption(Tree, Page[2], TEXT("WardLanguageLabel"), NSLOCTEXT("Maze.Settings", "Language", "Язык"), 276);
		Combo(Tree,
		      Page[2],
		      TEXT("LanguageCombo"),
		      276,
		      {FText::AsCultureInvariant(TEXT("Русский")),
		       FText::AsCultureInvariant(TEXT("English")),
		       FText::AsCultureInvariant(TEXT("Español"))});
		Caption(Tree, Page[3], TEXT("WardMusicLabel"), NSLOCTEXT("Maze.Ward", "Music", "Музыка меню"), 0);
		Slider(Tree, Page[3], TEXT("MenuMusicSlider"), TEXT("MenuMusicText"), 0, 0, 1, 1);
		Caption(Tree, Page[3], TEXT("WardSoundsLabel"), NSLOCTEXT("Maze.Ward", "Sounds", "Звуки интерфейса"), 92);
		Slider(Tree, Page[3], TEXT("InterfaceSoundsSlider"), TEXT("InterfaceSoundsText"), 92, 0, 1, 1);
		Pages->SetActiveWidgetIndex(0);
		Button(Tree, Canvas, TEXT("BackButton"), NSLOCTEXT("Maze.Ward", "Back", "Назад"), {132, 930}, {320, 64});
		Button(Tree,
		       Canvas,
		       TEXT("ResetButton"),
		       NSLOCTEXT("Maze.Ward", "Reset", "Сбросить настройки"),
		       {1450, 930},
		       {360, 64});
	}

	void BuildHUD(UWidgetTree* Tree)
	{
		auto* Root = Make<UCanvasPanel>(Tree, TEXT("WardRoot"));

		Tree->RootWidget = Root;

		auto* Content = Make<UCanvasPanel>(Tree, TEXT("HUDContent"));

		Fill(Root, Content);

		auto* Dot = Shape(Tree, Content, TEXT("Crosshair"), {0, 0}, {3, 3}, Mint());
		auto* DotSlot = CastChecked<UCanvasPanelSlot>(Dot->Slot);

		DotSlot->SetAnchors(FAnchors(.5, .5));
		DotSlot->SetAlignment({.5, .5});

		auto* Vitals = Make<UCanvasPanel>(Tree, TEXT("VitalsPanel"));

		Put(Content, Vitals, {38, -123}, {243, 9}, {0, 1}, {0, 1});

		auto* Stamina = Make<UCanvasPanel>(Tree, TEXT("StaminaPanel"));

		Put(Content, Stamina, {38, -104}, {243, 9}, {0, 1}, {0, 1});

		for (int I = 0; I < 2; ++I)
		{
			auto* PanelWidget = I == 0 ? Vitals : Stamina;
			const double Width = 243.;
			const FString Prefix = I == 0 ? TEXT("WardHealth") : TEXT("WardStamina");

			Shape(Tree,
			      PanelWidget,
			      Prefix + TEXT("Track"),
			      {0, 0},
			      {Width, 9},
			      MazeInterfaceStyle::Palette().Track,
			      FLinearColor::Transparent,
			      3);

			auto* Bar = Make<UProgressBar>(Tree, I == 0 ? TEXT("HealthBar") : TEXT("StaminaBar"));
			FProgressBarStyle Style;

			Style.SetBackgroundImage(Panel(FLinearColor::Transparent, FLinearColor::Transparent, 0));
			Style.SetFillImage(Panel(FLinearColor::White, FLinearColor::Transparent, 3));
			Bar->SetWidgetStyle(Style);
			Bar->SetBarFillStyle(EProgressBarFillStyle::Scale);
			Bar->SetBorderPadding({0, 0});
			Bar->SetPercent(1);
			Bar->SetFillColorAndOpacity(I == 0 ? HealthFill() : StaminaFill());
			Put(PanelWidget, Bar, {0, 0}, {Width, 9});
		}

		auto* Inventory = Make<UCanvasPanel>(Tree, TEXT("InventorySlots"));

		Inventory->SetVisibility(ESlateVisibility::HitTestInvisible);
		Put(Content, Inventory, {38, -38}, {243, 54}, {0, 1}, {0, 1});

		for (int I = 0; I < 4; ++I)
		{
			const double X = I * 63.;
			const FString Name = FString::Printf(TEXT("WardSlot%d"), I + 1);

			Shape(Tree, Inventory, Name, {X, 0}, {54, 54}, Glass(), Edge(), 3);
			Stroke(
			    Tree, Inventory, Name + TEXT("EmptyA"), {X + 8, 8}, {X + 46, 46}, Muted().CopyWithNewOpacity(.12), 1);
			Stroke(
			    Tree, Inventory, Name + TEXT("EmptyB"), {X + 46, 8}, {X + 8, 46}, Muted().CopyWithNewOpacity(.12), 1);
		}

		for (bool Death : {true, false})
		{
			auto* PanelWidget = Make<UCanvasPanel>(Tree, Death ? TEXT("DeathPanel") : TEXT("ExitPanel"));

			Put(Content, PanelWidget, {0, 0}, {660, 180}, {.5, .5}, {.5, .5});
			Shape(Tree,
			      PanelWidget,
			      Death ? TEXT("WardDeathGlass") : TEXT("WardExitGlass"),
			      {0, 0},
			      {660, 180},
			      Glass(),
			      Edge(),
			      5);

			auto* Label = Text(Tree,
			                   PanelWidget,
			                   Death ? TEXT("WardDeathCaption") : TEXT("WardExitCaption"),
			                   Death ? NSLOCTEXT("Maze.Ward", "Death", "Связь потеряна")
			                         : NSLOCTEXT("Maze.Ward", "ExitFound", "Выход найден"),
			                   {30, 62},
			                   {600, 56},
			                   32,
			                   Mint());

			Label->SetJustification(ETextJustify::Center);
			PanelWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	void BuildMap(UWidgetTree* Tree)
	{
		auto* Root = Make<UCanvasPanel>(Tree, TEXT("WardMapRoot"));

		Tree->RootWidget = Root;
		Put(Root, Make<USizeBox>(Tree, TEXT("MinimapBounds")), {-38, -38}, {256, 256}, {1, 1}, {1, 1});

		auto* Full = Make<USizeBox>(Tree, TEXT("FullMapBounds"));
		auto* Slot = Root->AddChildToCanvas(Full);

		Slot->SetAnchors(FAnchors(0, 0, 1, 1));
		Slot->SetOffsets(FMargin(0));

		auto* Controls = Make<UCanvasPanel>(Tree, TEXT("FullMapControls"));

		Controls->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Fill(Root, Controls);

		auto* Close = Button(Tree,
		                     Controls,
		                     TEXT("CloseMapButton"),
		                     NSLOCTEXT("Maze.Ward", "CloseMap", "Закрыть"),
		                     {-32, 32},
		                     {176, 48},
		                     true,
		                     true);
		auto* CloseSlot = CastChecked<UCanvasPanelSlot>(Close->Slot);

		CloseSlot->SetAnchors(FAnchors(1, 0));
		CloseSlot->SetAlignment({1, 0});
	}

	bool BuildAsset(const TCHAR* Name, UClass* Parent, TFunctionRef<void(UWidgetTree*)> Build)
	{
		const FString Path = FString(TEXT("/Game/UI/Ward/")) + Name;
		UWidgetBlueprint* BP = LoadObject<UWidgetBlueprint>(nullptr, *Path, nullptr, LOAD_NoWarn);

		if (!BP)
		{
			BP = CastChecked<UWidgetBlueprint>(
			    FKismetEditorUtilities::CreateBlueprint(Parent,
			                                            CreatePackage(*Path),
			                                            FName(Name),
			                                            BPTYPE_Normal,
			                                            UWidgetBlueprint::StaticClass(),
			                                            UWidgetBlueprintGeneratedClass::StaticClass()));
			FAssetRegistryModule::AssetCreated(BP);
		}

		if (BP->ParentClass != Parent)
			return false;

		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(BP);
		BP->Modify();

		if (BP->WidgetTree)
			BP->WidgetTree->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);

		BP->WidgetTree = NewObject<UWidgetTree>(BP, TEXT("WidgetTree"), RF_Transactional);
		Build(BP->WidgetTree);

		TArray<FName> Old;

		BP->WidgetVariableNameToGuidMap.GetKeys(Old);

		for (FName N : Old)
			BP->OnVariableRemoved(N);

		BP->ForEachSourceWidget(
		    [&](UWidget* W)
		    {
			    BP->OnVariableAdded(W->GetFName());
		    });
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
		FKismetEditorUtilities::CompileBlueprint(BP);

		if (BP->Status == BS_Error)
			return false;

		if (auto* HUD = Cast<UMazeHUDWidget>(BP->GeneratedClass->GetDefaultObject()))
		{
			HUD->StaminaColor = StaminaFill();
			HUD->ExhaustedColor = ExhaustedStaminaFill();
		}

		FSavePackageArgs Args;

		Args.TopLevelFlags = RF_Public | RF_Standalone;
		BP->MarkPackageDirty();

		bool Saved = UPackage::SavePackage(
		    BP->GetOutermost(),
		    BP,
		    *FPackageName::LongPackageNameToFilename(Path, FPackageName::GetAssetPackageExtension()),
		    Args);

		UE_LOG(
		    LogTemp, Display, TEXT("Ward UI %s: %s"), *Path, Saved ? TEXT("compiled and saved") : TEXT("SAVE FAILED"));

		return Saved;
	}

	void Build()
	{
		if (!GEditor || GEditor->PlayWorld)
		{
			UE_LOG(LogTemp, Warning, TEXT("Ward UI authoring requires stopped Play."));

			return;
		}

		bool OK = BuildAsset(TEXT("WBP_WardMainMenu"),
		                     UMazeMenuWidget::StaticClass(),
		                     [](UWidgetTree* T)
		                     {
			                     BuildMenu(T, false);
		                     });

		OK &= BuildAsset(TEXT("WBP_WardPauseMenu"),
		                 UMazeMenuWidget::StaticClass(),
		                 [](UWidgetTree* T)
		                 {
			                 BuildMenu(T, true);
		                 });
		OK &= BuildAsset(TEXT("WBP_WardSettings"), UMazeMenuWidget::StaticClass(), BuildSettings);
		OK &= BuildAsset(TEXT("WBP_WardHUD"), UMazeHUDWidget::StaticClass(), BuildHUD);
		OK &= BuildAsset(TEXT("WBP_WardMap"), UMazeExplorationMapWidget::StaticClass(), BuildMap);
		UE_LOG(LogTemp,
		       Display,
		       TEXT("Ward UI build complete: %s"),
		       OK ? TEXT("SUCCESS (5 independent WBPs)") : TEXT("FAILED"));
	}

	void BuildMainMenu()
	{
		if (!GEditor || GEditor->PlayWorld)
		{
			UE_LOG(LogTemp, Warning, TEXT("Ward UI authoring requires stopped Play."));

			return;
		}

		BuildAsset(TEXT("WBP_WardMainMenu"),
		           UMazeMenuWidget::StaticClass(),
		           [](UWidgetTree* Tree)
		           {
			           BuildMenu(Tree, false);
		           });
	}

	void BuildHUDAndMap()
	{
		if (!GEditor || GEditor->PlayWorld)
		{
			UE_LOG(LogTemp, Warning, TEXT("Ward UI authoring requires stopped Play."));

			return;
		}

		const bool HUDSaved = BuildAsset(TEXT("WBP_WardHUD"), UMazeHUDWidget::StaticClass(), BuildHUD);
		const bool MapSaved = BuildAsset(TEXT("WBP_WardMap"), UMazeExplorationMapWidget::StaticClass(), BuildMap);

		UE_LOG(
		    LogTemp, Display, TEXT("Ward HUD/map build: %s"), HUDSaved && MapSaved ? TEXT("SUCCESS") : TEXT("FAILED"));
	}

	void BuildHUDOnly()
	{
		if (!GEditor || GEditor->PlayWorld)
		{
			UE_LOG(LogTemp, Warning, TEXT("Ward HUD authoring requires stopped Play."));

			return;
		}

		BuildAsset(TEXT("WBP_WardHUD"), UMazeHUDWidget::StaticClass(), BuildHUD);
	}

	void BuildSettingsOnly()
	{
		if (!GEditor || GEditor->PlayWorld)
		{
			UE_LOG(LogTemp, Warning, TEXT("Ward settings authoring requires stopped Play."));

			return;
		}

		BuildAsset(TEXT("WBP_WardSettings"), UMazeMenuWidget::StaticClass(), BuildSettings);
	}

	void Preview()
	{
		if (!GEditor || GEditor->PlayWorld)
			return;

		const FString Folder = FPaths::ProjectSavedDir() / TEXT("UI/WardPreview");

		IFileManager::Get().MakeDirectory(*Folder, true);

		const TCHAR* Names[] = {TEXT("WBP_WardMainMenu"),
		                        TEXT("WBP_WardPauseMenu"),
		                        TEXT("WBP_WardSettings"),
		                        TEXT("WBP_WardHUD"),
		                        TEXT("WBP_WardMap")};

		for (int I = 0; I < 9; ++I)
		{
			const TCHAR* Name = Names[I == 8 ? 4 : I < 5 ? I : 2];
			const int32 Width = I == 8 ? 800 : 1920;
			auto* Class = LoadClass<UUserWidget>(nullptr, *FString::Printf(TEXT("/Game/UI/Ward/%s.%s_C"), Name, Name));

			if (!Class)
				continue;

			TStrongObjectPtr<UUserWidget> W(NewObject<UUserWidget>(GetTransientPackage(), Class));

			W->SetDesignerFlags(EWidgetDesignFlags::Designing);
			W->Initialize();

			if (I >= 5 && I < 8)
			{
				const TCHAR* Sections[] = {
				    TEXT("ShowControlSettings"), TEXT("ShowGameSettings"), TEXT("ShowAudioSettings")};

				if (UFunction* Function = W->FindFunction(Sections[I - 5]))
					W->ProcessEvent(Function, nullptr);
			}

			if (I == 8)
				W->GetWidgetFromName(TEXT("FullMapControls"))->SetVisibility(ESlateVisibility::Collapsed);

			W->WidgetTree->ForEachWidget(
			    [](UWidget* Item)
			    {
				    Item->bHiddenInDesigner = Item->GetVisibility() == ESlateVisibility::Collapsed ||
				                              Item->GetVisibility() == ESlateVisibility::Hidden;
			    });

			TSharedRef<SWidget> Slate = W->TakeWidget();

			// InputKeySelector's text visibility is a Slate-only setter in UE 5.8.
			// Apply after TakeWidget, matching the native runtime binding stage.
			W->WidgetTree->ForEachWidget(
			    [](UWidget* Item)
			    {
				    if (auto* Key = Cast<UInputKeySelector>(Item))
					    Key->SetTextBlockVisibility(ESlateVisibility::Hidden);
			    });

			if (auto* Sensitivity = Cast<UTextBlock>(W->GetWidgetFromName(TEXT("SensitivityText"))))
				Sensitivity->SetText(FText::AsCultureInvariant(TEXT("1.00 x")));

			FWidgetRenderer Renderer(false);
			TStrongObjectPtr<UTextureRenderTarget2D> Target(NewObject<UTextureRenderTarget2D>());

			Target->ClearColor = MazeInterfaceStyle::Palette().Glass;
			Target->InitCustomFormat(Width, 1080, PF_FloatRGBA, true);
			Target->UpdateResourceImmediate(true);
			Renderer.DrawWidget(Target.Get(), Slate, FVector2D(Width, 1080), 0);
			Slate->Invalidate(EInvalidateWidgetReason::Layout);
			Renderer.DrawWidget(Target.Get(), Slate, FVector2D(Width, 1080), 0);

			TArray<FFloat16Color> Linear;
			TArray<FColor> Pixels;
			TArray64<uint8> Bytes;

			if (Target->GameThread_GetRenderTargetResource()->ReadFloat16Pixels(Linear))
			{
				for (const auto& Pixel : Linear)
					Pixels.Add(FLinearColor(Pixel).ToFColorSRGB());

				FImageUtils::PNGCompressImageArray(Width, 1080, Pixels, Bytes);
				FFileHelper::SaveArrayToFile(Bytes, *(Folder / FString::Printf(TEXT("%s_%d.png"), Name, I)));
			}

			W->ReleaseSlateResources(true);
		}

		// Same Slate widget as MoviePlayer; fixed status, no world or gameplay is started.
		TSharedRef<SWidget> Loading = MazeInterfaceStyle::MakeLoadingScreen();
		FMazePreparationStatus Status;

		Status.Stage = EMazePreparationStage::Assets;
		MazeInterfaceStyle::UpdateLoadingScreen(Loading, Status);

		FWidgetRenderer LoadingRenderer(false);
		TStrongObjectPtr<UTextureRenderTarget2D> LoadingTarget(NewObject<UTextureRenderTarget2D>());

		LoadingTarget->ClearColor = MazeInterfaceStyle::Palette().Glass;
		LoadingTarget->InitCustomFormat(1920, 1080, PF_FloatRGBA, true);
		LoadingTarget->UpdateResourceImmediate(true);
		LoadingRenderer.DrawWidget(LoadingTarget.Get(), Loading, {1920, 1080}, 0);

		TArray<FFloat16Color> Linear;
		TArray<FColor> Pixels;
		TArray64<uint8> Bytes;

		if (LoadingTarget->GameThread_GetRenderTargetResource()->ReadFloat16Pixels(Linear))
		{
			for (const auto& Pixel : Linear)
				Pixels.Add(FLinearColor(Pixel).ToFColorSRGB());

			FImageUtils::PNGCompressImageArray(1920, 1080, Pixels, Bytes);
			FFileHelper::SaveArrayToFile(Bytes, *(Folder / TEXT("Loading.png")));
		}

		UE_LOG(LogTemp, Display, TEXT("Ward UI previews saved."));
	}

	FAutoConsoleCommand BuildCommand(
	    TEXT("laby.UI.BuildWard"),
	    TEXT("Create the independent Ward Widget Blueprints; replaces only generated Ward layouts."),
	    FConsoleCommandDelegate::CreateStatic(&Build));
	FAutoConsoleCommand PreviewCommand(TEXT("laby.UI.PreviewWard"),
	                                   TEXT("Render Ward static design previews."),
	                                   FConsoleCommandDelegate::CreateStatic(&Preview));
	FAutoConsoleCommand BuildHUDMapCommand(TEXT("laby.UI.BuildWardHUDMap"),
	                                       TEXT("Replace only generated Ward HUD and map layouts, with Play stopped."),
	                                       FConsoleCommandDelegate::CreateStatic(&BuildHUDAndMap));
	FAutoConsoleCommand BuildHUDOnlyCommand(TEXT("laby.UI.BuildWardHUD"),
	                                        TEXT("Replace only the generated Ward HUD layout, with Play stopped."),
	                                        FConsoleCommandDelegate::CreateStatic(&BuildHUDOnly));
	FAutoConsoleCommand BuildSettingsOnlyCommand(
	    TEXT("laby.UI.BuildWardSettings"),
	    TEXT("Replace only the generated Ward settings layout, with Play stopped."),
	    FConsoleCommandDelegate::CreateStatic(&BuildSettingsOnly));
	FAutoConsoleCommand BuildMainMenuCommand(TEXT("laby.UI.BuildWardMainMenu"),
	                                         TEXT("Replace only the generated Ward main menu, with Play stopped."),
	                                         FConsoleCommandDelegate::CreateStatic(&BuildMainMenu));
}
