#include "UI/MazeWidgets.h"
#include "Player/MazeKeyBindings.h"
#include "Components/InputKeySelector.h"
#include "UI/MazeText.h"
#include "UI/MazeInterfaceStyle.h"
#include "Sound/SoundBase.h"
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
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Animation/WidgetAnimation.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/BufferArchive.h"
#include "Slate/WidgetRenderer.h"
#include "Styling/CoreStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "WidgetBlueprint.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"

namespace MazeGlassUI
{
	using namespace MazeInterfaceStyle;

	bool Save(UObject* Asset)
	{
		UPackage* Package = Asset->GetOutermost();

		Package->MarkPackageDirty();

		FSavePackageArgs Args;

		Args.TopLevelFlags = RF_Public | RF_Standalone;

		const FString Filename =
		    FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());

		return UPackage::SavePackage(Package, Asset, *Filename, Args);
	}

	FSlateBrush Brush(UTexture2D* Texture, FLinearColor Tint = FLinearColor::White)
	{
		FSlateBrush Value;

		Value.SetResourceObject(Texture);
		Value.DrawAs = ESlateBrushDrawType::Image;
		Value.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
		Value.TintColor = Tint;

		return Value;
	}

	template <typename T> T* Make(UWidgetTree* Tree, const TCHAR* Name)
	{
		T* Result = Tree->ConstructWidget<T>(T::StaticClass(), Name);

		Result->SetFlags(RF_Transactional);
		Result->bIsVariable = true;

		return Result;
	}

	void Place(UCanvasPanel* Parent, UWidget* Widget, FVector2D Position, FVector2D Size)
	{
		auto* Slot = Parent->AddChildToCanvas(Widget);

		Slot->SetPosition(Position);
		Slot->SetSize(Size);
	}

	void Fill(UCanvasPanel* Parent, UWidget* Widget)
	{
		auto* Slot = Parent->AddChildToCanvas(Widget);

		Slot->SetAnchors(FAnchors(0, 0, 1, 1));
		Slot->SetOffsets(FMargin(0));
	}

	UTextBlock* Label(UWidgetTree* Tree, const TCHAR* Name, FText Text, int32 Size, FLinearColor Color = Ink)
	{
		auto* Widget = Make<UTextBlock>(Tree, Name);
		FSlateFontInfo Font = Widget->GetFont();

		Font.Size = Size;
		Font.TypefaceFontName = TEXT("Light");
		Font.LetterSpacing = Size >= 48 ? 260 : Size >= 24 ? 160 : 80;
		Widget->SetFont(Font);
		Widget->SetText(Text);
		Widget->SetColorAndOpacity(Color);
		Widget->SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.5f));
		Widget->SetShadowOffset(FVector2D(0, 1));
		Widget->SetVisibility(ESlateVisibility::HitTestInvisible);

		return Widget;
	}

	void Image(UWidgetTree* Tree,
	           UCanvasPanel* Parent,
	           const TCHAR* Name,
	           UTexture2D* Texture,
	           FVector2D Position,
	           FVector2D Size)
	{
		auto* Widget = Make<UImage>(Tree, Name);

		Widget->SetBrush(Brush(Texture));
		Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
		Place(Parent, Widget, Position, Size);
	}

	FSlateBrush Solid(FLinearColor Color)
	{
		FSlateBrush Result = *FCoreStyle::Get().GetBrush("WhiteBrush");

		Result.TintColor = Color;

		return Result;
	}

	FSlateBrush Outlined(FLinearColor Fill, FLinearColor Outline)
	{
		FSlateBrush Result = Solid(Fill);

		Result.DrawAs = ESlateBrushDrawType::RoundedBox;
		Result.OutlineSettings.CornerRadii = FVector4(3, 3, 3, 3);
		Result.OutlineSettings.Color = Outline;
		Result.OutlineSettings.Width = 1.f;
		Result.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;

		return Result;
	}

	void Rect(
	    UWidgetTree* Tree, UCanvasPanel* Parent, const FString& Name, FVector2D P, FVector2D Size, FLinearColor Color)
	{
		auto* Item = Make<UImage>(Tree, *Name);

		Item->SetBrush(Solid(Color));
		Item->SetVisibility(ESlateVisibility::HitTestInvisible);
		Place(Parent, Item, P, Size);
	}

	void Frame(UWidgetTree* Tree, UCanvasPanel* Parent, const FString& Name, FVector2D P, FVector2D Size)
	{
		Rect(Tree, Parent, Name + TEXT("Top"), P, {Size.X, 1}, Line);
		Rect(Tree, Parent, Name + TEXT("Bottom"), P + FVector2D(0, Size.Y), {Size.X, 1}, Line);
		Rect(Tree, Parent, Name + TEXT("Left"), P, {1, Size.Y}, Line);
		Rect(Tree, Parent, Name + TEXT("Right"), P + FVector2D(Size.X, 0), {1, Size.Y}, Line);

		for (int32 I = 0; I < 4; ++I)
		{
			const bool Right = (I & 1) != 0, Bottom = (I & 2) != 0;
			const FVector2D Corner = P + FVector2D(Right ? Size.X : 0, Bottom ? Size.Y : 0);

			Rect(Tree,
			     Parent,
			     Name + FString::Printf(TEXT("CornerH%d"), I),
			     Corner - FVector2D(Right ? 16 : 0, 0),
			     {16, 1},
			     Ink);
			Rect(Tree,
			     Parent,
			     Name + FString::Printf(TEXT("CornerV%d"), I),
			     Corner - FVector2D(0, Bottom ? 16 : 0),
			     {1, 16},
			     Ink);
		}
	}

	UButton* Button(UWidgetTree* Tree,
	                UCanvasPanel* Parent,
	                const TCHAR* Name,
	                const FText& Text,
	                UTexture2D*,
	                FVector2D Position,
	                FVector2D Size,
	                float = 0.f)
	{
		auto* Result = Make<UButton>(Tree, Name);
		FButtonStyle Style;
		const bool bPrimary =
		    FCString::Strcmp(Name, TEXT("NewGameButton")) == 0 || FCString::Strcmp(Name, TEXT("ResumeButton")) == 0;
		const FLinearColor DarkText = FLinearColor::FromSRGBColor(FColor(12, 37, 43));

		Style.SetNormal(Outlined(bPrimary ? Accent : Glass.CopyWithNewOpacity(0.54f),
		                         bPrimary ? Accent : Ink.CopyWithNewOpacity(0.68f)));
		Style.SetHovered(Outlined(Ink, Ink));
		Style.SetPressed(Outlined(Accent, Accent));
		Style.SetDisabled(Outlined(Glass.CopyWithNewOpacity(0.32f), Line));
		Style.SetNormalForeground(bPrimary ? DarkText : Ink)
		    .SetHoveredForeground(DarkText)
		    .SetPressedForeground(DarkText);
		Style.SetDisabledForeground(Muted.CopyWithNewOpacity(0.45f));
		Style.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));

		if (auto* Sound = LoadObject<USoundBase>(nullptr, TEXT("/Game/UI/Glass/S_UIHover")))
		{
			FSlateSound Hover;

			Hover.SetResourceObject(Sound);
			Style.SetHoveredSound(Hover);
		}

		if (auto* Sound = LoadObject<USoundBase>(nullptr, TEXT("/Game/UI/Glass/S_UIPress")))
		{
			FSlateSound Press;

			Press.SetResourceObject(Sound);
			Style.SetPressedSound(Press);
		}

		Result->SetStyle(Style);
		Result->SetCursor(EMouseCursor::Hand);

		auto* Content = Make<UCanvasPanel>(Tree, *(FString(Name) + TEXT("Content")));
		auto* TextWidget = Label(Tree, *(FString(Name) + TEXT("Label")), Text, Size.X > 390 ? 23 : 18);

		TextWidget->SetColorAndOpacity(FSlateColor::UseForeground());
		Place(Content, TextWidget, {24, Size.Y * 0.5}, {Size.X - 90, 38});
		CastChecked<UCanvasPanelSlot>(TextWidget->Slot)->SetAlignment({0, 0.5});
		TextWidget->SetJustification(ETextJustify::Left);

		auto* Chevron = Label(Tree, *(FString(Name) + TEXT("Chevron")), FText::AsCultureInvariant(TEXT("›")), 32);

		Chevron->SetColorAndOpacity(FSlateColor::UseForeground());
		Chevron->SetJustification(ETextJustify::Center);
		Place(Content, Chevron, {Size.X - 54, Size.Y * 0.5}, {30, 42});
		CastChecked<UCanvasPanelSlot>(Chevron->Slot)->SetAlignment({0, 0.5});
		Result->SetContent(Content);
		Place(Parent, Result, Position, Size);

		return Result;
	}

	void Caption(UWidgetTree* Tree, UCanvasPanel* Page, const TCHAR* Name, const FText& Text, float Y)
	{
		Place(Page, Label(Tree, Name, Text.ToUpper(), 18), {0, Y + 8}, {485, 42});
	}

	UComboBoxString* Combo(UWidgetTree* Tree, UCanvasPanel* Page, const TCHAR* Name, float Y, TArray<FText> Options)
	{
		auto* Widget = Make<UComboBoxString>(Tree, Name);

		// AddOption updates only transient runtime options; persist the editor defaults as well.
		if (auto* Property = FindFProperty<FArrayProperty>(Widget->GetClass(), TEXT("DefaultOptions")))
		{
			auto* Defaults = Property->ContainerPtrToValuePtr<TArray<FString>>(Widget);

			for (const FText& Option : Options)
				Defaults->Add(Option.ToString());
		}

		for (const FText& Option : Options)
			Widget->AddOption(Option.ToString());

		Widget->SetSelectedIndex(0);
		Widget->SetContentPadding(FMargin(18, 10));

		FSlateBrush Base = *FCoreStyle::Get().GetBrush("WhiteBrush");

		Base.TintColor = Glass;

		FSlateBrush Hover = Base;

		Hover.TintColor = Accent.CopyWithNewOpacity(0.16f);

		FComboBoxStyle Style = Widget->GetWidgetStyle();

		Style.ComboButtonStyle.ButtonStyle.SetNormal(Base).SetHovered(Hover).SetPressed(Hover);
		Style.ComboButtonStyle.ButtonStyle.SetNormalForeground(Ink).SetHoveredForeground(Accent).SetPressedForeground(
		    Ink);
		Style.ComboButtonStyle.SetMenuBorderBrush(Base);
		Style.ComboButtonStyle.DownArrowImage.TintColor = Accent;
		Widget->SetWidgetStyle(Style);

		FTableRowStyle Row = Widget->GetItemStyle();

		Row.SetEvenRowBackgroundBrush(Base)
		    .SetOddRowBackgroundBrush(Base)
		    .SetEvenRowBackgroundHoveredBrush(Hover)
		    .SetOddRowBackgroundHoveredBrush(Hover);
		Row.SetActiveBrush(Hover).SetInactiveBrush(Hover).SetTextColor(Ink).SetSelectedTextColor(Accent);
		Widget->SetItemStyle(Row);

		if (auto* Property = FindFProperty<FStructProperty>(Widget->GetClass(), TEXT("ForegroundColor")))
			*Property->ContainerPtrToValuePtr<FSlateColor>(Widget) = Ink;

		// This construction-only property intentionally has no runtime setter in UE 5.8.
		if (auto* Property = FindFProperty<FStructProperty>(Widget->GetClass(), TEXT("Font")))
		{
			auto* Font = Property->ContainerPtrToValuePtr<FSlateFontInfo>(Widget);

			*Font = GetDefault<UTextBlock>()->GetFont();
			Font->Size = 18;
			Font->TypefaceFontName = TEXT("Light");
			Font->LetterSpacing = 60;
		}

		Place(Page, Widget, {500, Y}, {385, 55});

		return Widget;
	}

	void Check(UWidgetTree* Tree, UCanvasPanel* Page, const TCHAR* Name, float Y, bool bChecked)
	{
		auto* Widget = Make<UCheckBox>(Tree, Name);
		FCheckBoxStyle Style = Widget->GetWidgetStyle();
		FSlateBrush Off = *FCoreStyle::Get().GetBrush("WhiteBrush");

		Off.ImageSize = FVector2D(20, 20);
		Off.TintColor = FLinearColor::Transparent;
		Off.DrawAs = ESlateBrushDrawType::RoundedBox;
		Off.OutlineSettings.CornerRadii = FVector4(2, 2, 2, 2);
		Off.OutlineSettings.Color = Muted;
		Off.OutlineSettings.Width = 1.f;
		Off.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;

		FSlateBrush On = Off;

		On.TintColor = Accent;

		FSlateBrush Hover = Off;

		Hover.TintColor = Muted;
		Style.SetUncheckedImage(Off).SetUncheckedHoveredImage(Hover).SetUncheckedPressedImage(On);
		Style.SetCheckedImage(On).SetCheckedHoveredImage(On).SetCheckedPressedImage(Hover);
		Style.SetPadding(FMargin(8));
		Widget->SetWidgetStyle(Style);

		Widget->SetIsChecked(bChecked);

		Place(Page, Widget, {620, Y}, {265, 55});
	}

	void Slider(UWidgetTree* Tree,
	            UCanvasPanel* Page,
	            const TCHAR* Name,
	            const TCHAR* TextName,
	            float Y,
	            float Min,
	            float Max,
	            float Value,
	            float Step,
	            FText ValueText)
	{
		auto* Widget = Make<USlider>(Tree, Name);

		Widget->SetMinValue(Min);
		Widget->SetMaxValue(Max);
		Widget->SetStepSize(Step);
		Widget->SetValue(Value);
		Widget->SetSliderBarColor(Line);
		Widget->SetSliderHandleColor(Accent);
		Place(Page, Widget, {500, Y + 8}, {275, 42});
		Place(Page, Label(Tree, TextName, ValueText, 18, Accent), {798, Y + 6}, {102, 44});
	}

	void Settings(UWidgetTree* Tree, UCanvasPanel* Stage, UTexture2D* Plate)
	{
		const TCHAR* Tabs[] = {TEXT("VideoTabButton"), TEXT("ControlsTabButton"), TEXT("GameTabButton")};
		const FText Titles[] = {NSLOCTEXT("Maze.Glass", "Video", "ВИДЕО"),
		                        NSLOCTEXT("Maze.Glass", "Controls", "УПРАВЛЕНИЕ"),
		                        NSLOCTEXT("Maze.Glass", "Game", "ИГРА")};

		for (int32 I = 0; I < 3; ++I)
		{
			Button(Tree, Stage, Tabs[I], Titles[I], Plate, {80, 304.0 + I * 76}, {370, 64});

			auto* Indicator =
			    Label(Tree, *(FString(Tabs[I]) + TEXT("Indicator")), FText::AsCultureInvariant(TEXT("›")), 28, Accent);

			Place(Stage, Indicator, {60, 314.0 + I * 76}, {24, 38});
			Indicator->SetVisibility(I == 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		}

		Rect(Tree, Stage, TEXT("SettingsGlass"), {570, 140}, {1270, 764}, Glass.CopyWithNewOpacity(0.86f));
		Frame(Tree, Stage, TEXT("SettingsFrame"), {570, 140}, {1270, 764});
		Place(Stage, Label(Tree, TEXT("SettingsSectionTitle"), Titles[0], 25), {640, 184}, {990, 50});
		Rect(Tree, Stage, TEXT("SettingsTitleRule"), {640, 250}, {1110, 1}, Line);

		auto* Pages = Make<UWidgetSwitcher>(Tree, TEXT("SettingsPages"));

		Place(Stage, Pages, {640, 300}, {1120, 570});

		auto* Video = Make<UCanvasPanel>(Tree, TEXT("VideoPage"));
		auto* Controls = Make<UCanvasPanel>(Tree, TEXT("ControlsPage"));
		auto* Game = Make<UCanvasPanel>(Tree, TEXT("GamePage"));

		Pages->AddChild(Video);
		Pages->AddChild(Controls);
		Pages->AddChild(Game);
		Pages->SetActiveWidgetIndex(0);

		const TArray<FText> Quality = {NSLOCTEXT("Maze.Arc", "Low", "Низкое"),
		                               NSLOCTEXT("Maze.Arc", "Medium", "Среднее"),
		                               NSLOCTEXT("Maze.Arc", "High", "Высокое"),
		                               NSLOCTEXT("Maze.Arc", "Epic", "Эпическое"),
		                               NSLOCTEXT("Maze.Arc", "Custom", "Пользовательское")};

		Caption(Tree, Video, TEXT("QualityLabel"), NSLOCTEXT("Maze.Arc", "Quality", "Качество графики"), 0);
		Combo(Tree, Video, TEXT("QualityCombo"), 0, Quality);
		Caption(Tree, Video, TEXT("ShadowsLabel"), NSLOCTEXT("Maze.Arc", "Shadows", "Качество теней"), 96);

		auto Shadows = Quality;

		Shadows[4] = NSLOCTEXT("Maze.Arc", "Cinematic", "Кинематографическое");
		Combo(Tree, Video, TEXT("ShadowsCombo"), 96, Shadows);
		Caption(Tree, Video, TEXT("FrameLimitLabel"), NSLOCTEXT("Maze.Arc", "FrameLimit", "Частота кадров"), 192);
		Combo(Tree,
		      Video,
		      TEXT("FrameLimitCombo"),
		      192,
		      {FText::AsCultureInvariant(TEXT("60 FPS")),
		       FText::AsCultureInvariant(TEXT("90 FPS")),
		       FText::AsCultureInvariant(TEXT("120 FPS")),
		       FText::AsCultureInvariant(TEXT("144 FPS")),
		       NSLOCTEXT("Maze.Arc", "Unlimited", "Без ограничения"),
		       NSLOCTEXT("Maze.Arc", "Current", "Текущее значение")});
		Caption(Tree, Video, TEXT("RenderScaleLabel"), NSLOCTEXT("Maze.Arc", "RenderScale", "Масштаб рендера"), 288);
		Slider(Tree,
		       Video,
		       TEXT("RenderScaleSlider"),
		       TEXT("RenderScaleText"),
		       288,
		       25,
		       100,
		       100,
		       5,
		       FText::AsCultureInvariant(TEXT("100%")));
		Caption(Tree, Video, TEXT("VSyncLabel"), NSLOCTEXT("Maze.Arc", "VSync", "Вертикальная синхронизация"), 384);
		Check(Tree, Video, TEXT("VSyncCheck"), 384, false);
		Caption(Tree, Controls, TEXT("SensitivityLabel"), MazeText::Widget(TEXT("SensitivityLabel")), 0);
		Slider(Tree,
		       Controls,
		       TEXT("SensitivitySlider"),
		       TEXT("SensitivityText"),
		       0,
		       0.1f,
		       3.f,
		       1.f,
		       0.05f,
		       FText::AsCultureInvariant(TEXT("1.00 x")));
		Caption(
		    Tree, Controls, TEXT("InvertYLabel"), NSLOCTEXT("Maze.Arc", "InvertY", "Инверсия мыши по вертикали"), 96);
		Check(Tree, Controls, TEXT("InvertYCheck"), 96, false);
		Place(Controls,
		      Label(Tree,
		            TEXT("BindingsTitle"),
		            NSLOCTEXT("Maze.Arc", "BindingsTitle", "УПРАВЛЕНИЕ ПЕРСОНАЖЕМ"),
		            18,
		            Accent),
		      {0, 176},
		      {900, 36});

		int32 BindingIndex = 0;

		for (const auto& Binding : MazeKeyBindings::Definitions())
		{
			const double X = (BindingIndex / 5) * 560.0;
			const double Y = 224.0 + (BindingIndex % 5) * 52.0;
			const FString Name = Binding.WidgetName().ToString();

			Place(Controls, Label(Tree, *(Name + TEXT("Label")), Binding.Label, 18), {X, Y + 6}, {280, 38});

			auto* Selector = Make<UInputKeySelector>(Tree, *Name);

			Selector->SetAllowModifierKeys(false);
			Selector->SetAllowGamepadKeys(false);
			Selector->SetEscapeKeys({EKeys::Escape});
			Selector->SetSelectedKey(FInputChord(Binding.DefaultKey));
			Selector->SetKeySelectionText(NSLOCTEXT("Maze.Keys", "PressKey", "Нажмите клавишу"));
			Selector->SetNoKeySpecifiedText(NSLOCTEXT("Maze.Keys", "Unbound", "Не назначено"));

			FSlateBrush Base = *FCoreStyle::Get().GetBrush("WhiteBrush");

			Base.TintColor = Glass;

			FSlateBrush Hover = Base;

			Hover.TintColor = Muted.CopyWithNewOpacity(0.18f);

			FButtonStyle ButtonStyle = Selector->GetButtonStyle();

			ButtonStyle.SetNormal(Base).SetHovered(Hover).SetPressed(Hover);
			Selector->SetButtonStyle(ButtonStyle);

			FTextBlockStyle TextStyle = Selector->GetTextStyle();

			TextStyle.SetFont(MazeInterfaceStyle::Font(16, 120)).SetColorAndOpacity(Ink);
			Selector->SetTextStyle(TextStyle);
			Place(Controls, Selector, {X + 292, Y}, {220, 42});
			++BindingIndex;
		}

		Place(Controls, Label(Tree, TEXT("KeyBindingStatus"), FText::GetEmpty(), 16, Accent), {0, 492}, {1100, 38});

		Caption(Tree, Game, TEXT("CompassLabel"), NSLOCTEXT("Maze.Arc", "Compass", "Компас на карте"), 0);
		Check(Tree, Game, TEXT("CompassCheck"), 0, true);
		Caption(Tree, Game, TEXT("CrosshairLabel"), NSLOCTEXT("Maze.Arc", "Crosshair", "Точка в центре экрана"), 96);
		Check(Tree, Game, TEXT("CrosshairCheck"), 96, true);
		Caption(
		    Tree, Game, TEXT("CameraMotionLabel"), NSLOCTEXT("Maze.Arc", "CameraMotion", "Покачивание камеры"), 192);
		Check(Tree, Game, TEXT("CameraMotionCheck"), 192, true);
		Place(Game,
		      Label(Tree, TEXT("AudioSettingsTitle"), NSLOCTEXT("Maze.Settings", "Audio", "ЗВУК"), 18, Accent),
		      {0, 288},
		      {900, 36});
		Caption(Tree, Game, TEXT("MenuMusicLabel"), NSLOCTEXT("Maze.Settings", "MenuMusic", "Музыка меню"), 332);
		Slider(Tree,
		       Game,
		       TEXT("MenuMusicSlider"),
		       TEXT("MenuMusicText"),
		       332,
		       0.f,
		       1.f,
		       1.f,
		       0.05f,
		       FText::AsCultureInvariant(TEXT("100%")));
		Caption(Tree,
		        Game,
		        TEXT("InterfaceSoundsLabel"),
		        NSLOCTEXT("Maze.Settings", "InterfaceSounds", "Звуки интерфейса"),
		        428);
		Slider(Tree,
		       Game,
		       TEXT("InterfaceSoundsSlider"),
		       TEXT("InterfaceSoundsText"),
		       428,
		       0.f,
		       1.f,
		       1.f,
		       0.05f,
		       FText::AsCultureInvariant(TEXT("100%")));

		Button(
		    Tree, Stage, TEXT("ResetButton"), NSLOCTEXT("Maze.Arc", "Reset", "ИСХОДНЫЕ"), Plate, {370, 980}, {300, 58});
		Button(Tree, Stage, TEXT("BackButton"), NSLOCTEXT("Maze.Arc", "Back", "НАЗАД"), Plate, {80, 980}, {230, 58});
	}

	void SyncWidgetGuids(UWidgetBlueprint* Blueprint)
	{
		// UE 5.8 tracks all source widget names, not just exposed Blueprint variables.
		TSet<FName> Names;

		Blueprint->ForEachSourceWidget(
		    [&](UWidget* Widget)
		    {
			    Names.Add(Widget->GetFName());
		    });

		for (const auto& Animation : Blueprint->Animations)
			if (Animation)
				Names.Add(Animation->GetFName());

		TArray<FName> OldNames;

		Blueprint->WidgetVariableNameToGuidMap.GetKeys(OldNames);

		for (FName Name : OldNames)
			if (!Names.Contains(Name))
				Blueprint->OnVariableRemoved(Name);

		for (FName Name : Names)
			if (!Blueprint->WidgetVariableNameToGuidMap.Contains(Name))
				Blueprint->OnVariableAdded(Name);
	}

	void BuildMenu(UWidgetBlueprint* Blueprint,
	               bool bPause,
	               bool bSettings,
	               UTexture2D* Backdrop,
	               UTexture2D* Shell,
	               UTexture2D* Plate)
	{
		Blueprint->Modify();

		if (Blueprint->WidgetTree)
			Blueprint->WidgetTree->Rename(
			    nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);

		auto* Tree = NewObject<UWidgetTree>(Blueprint, TEXT("WidgetTree"), RF_Transactional);

		Blueprint->WidgetTree = Tree;

		auto* Root = Make<UCanvasPanel>(Tree, TEXT("Root"));

		Tree->RootWidget = Root;

		auto* Scale = Make<UScaleBox>(Tree, TEXT("MenuScale"));

		Scale->SetStretch(EStretch::ScaleToFit);
		Fill(Root, Scale);

		auto* Bounds = Make<USizeBox>(Tree, TEXT("DesignBounds"));

		Bounds->SetWidthOverride(1920);
		Bounds->SetHeightOverride(1080);
		Scale->SetContent(Bounds);

		auto* Stage = Make<UCanvasPanel>(Tree, TEXT("InterfaceStage"));

		Bounds->SetContent(Stage);
		Rect(Tree, Stage, TEXT("MenuTint"), {0, 0}, {1920, 1080}, Glass.CopyWithNewOpacity(bSettings ? 0.25f : 0.05f));
		Place(Stage,
		      Label(Tree, TEXT("Title"), MazeText::Widget(TEXT("Title")), bSettings ? 50 : 78),
		      bSettings ? FVector2D(88, 74) : FVector2D(74, 246),
		      bSettings ? FVector2D(460, 90) : FVector2D(620, 118));
		Place(Stage,
		      Label(Tree, TEXT("VersionText"), MazeText::Widget(TEXT("VersionText")), 11, Muted),
		      {1580, 1008},
		      {250, 24});

		if (bSettings)
		{
			Place(Stage,
			      Label(Tree, TEXT("SettingsHeading"), NSLOCTEXT("Maze.Glass", "Settings", "НАСТРОЙКИ"), 27),
			      {88, 231},
			      {430, 50});
			Settings(Tree, Stage, Plate);
			Tree->FindWidget(TEXT("VersionText"))->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			if (bPause)
			{
				Place(Stage,
				      Label(Tree, TEXT("MenuHeading"), NSLOCTEXT("Maze.Glass", "Pause", "ПАУЗА"), 24),
				      {88, 412},
				      {540, 48});
				Rect(Tree, Stage, TEXT("HeadingRule"), {88, 470}, {520, 1}, Line);
			}

			float Y = bPause ? 494.f : 440.f;

			if (bPause)
			{
				Button(Tree,
				       Stage,
				       TEXT("ResumeButton"),
				       MazeText::Widget(TEXT("ResumeButtonLabel")),
				       Plate,
				       {88, Y},
				       {540, 72});
				Y += 88;
			}
			else
			{
				Button(Tree,
				       Stage,
				       TEXT("NewGameButton"),
				       MazeText::Widget(TEXT("NewGameButtonLabel")),
				       Plate,
				       {88, Y},
				       {540, 72});
				Y += 88;
			}

			Button(Tree,
			       Stage,
			       TEXT("SettingsButton"),
			       MazeText::Widget(TEXT("SettingsButtonLabel")),
			       Plate,
			       {88, Y},
			       {540, 72});
			Y += 88;

			if (bPause)
			{
				Button(Tree,
				       Stage,
				       TEXT("MainMenuButton"),
				       NSLOCTEXT("Maze.Glass", "Main", "ГЛАВНОЕ МЕНЮ"),
				       Plate,
				       {88, Y},
				       {540, 72});
				Y += 88;
			}

			Button(
			    Tree, Stage, TEXT("QuitButton"), NSLOCTEXT("Maze.Glass", "Quit", "ВЫХОД"), Plate, {88, Y}, {540, 72});
		}

		SyncWidgetGuids(Blueprint);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);

		if (Blueprint->Status != BS_Error && Save(Blueprint))
		{
			UE_LOG(LogTemp, Display, TEXT("Laby glass UI saved: %s"), *Blueprint->GetPathName());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Laby glass UI compilation/save failed: %s"), *Blueprint->GetPathName());
		}
	}

	void RestyleHUD(UTexture2D* Shell)
	{
		auto* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/UI/WBP_HUD"));

		if (!Blueprint)
			return;

		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(Blueprint);
		Blueprint->Modify();

		if (Blueprint->WidgetTree)
			Blueprint->WidgetTree->Rename(
			    nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);

		auto* Tree = NewObject<UWidgetTree>(Blueprint, TEXT("WidgetTree"), RF_Transactional);

		Blueprint->WidgetTree = Tree;

		auto* Root = Make<UCanvasPanel>(Tree, TEXT("Root"));

		Tree->RootWidget = Root;

		auto* Content = Make<UCanvasPanel>(Tree, TEXT("HUDContent"));

		Fill(Root, Content);

		auto Anchor = [](UWidget* Widget, FVector2D Point, FVector2D Alignment)
		{
			auto* Slot = CastChecked<UCanvasPanelSlot>(Widget->Slot);
			Slot->SetAnchors(FAnchors(Point.X, Point.Y));
			Slot->SetAlignment(Alignment);
		};
		auto* Crosshair = Make<UImage>(Tree, TEXT("Crosshair"));

		Crosshair->SetBrush(*FCoreStyle::Get().GetBrush("WhiteBrush"));
		Crosshair->SetColorAndOpacity(Accent);
		Crosshair->SetVisibility(ESlateVisibility::HitTestInvisible);
		Place(Content, Crosshair, {0, 0}, {3, 3});
		Anchor(Crosshair, {0.5, 0.5}, {0.5, 0.5});

		auto* Vitals = Make<UBorder>(Tree, TEXT("VitalsPanel"));

		Vitals->SetBrush(Outlined(Glass.CopyWithNewOpacity(0.80f), Line));
		Vitals->SetPadding(FMargin(0));
		Place(Content, Vitals, {42, -42}, {440, 142});
		Anchor(Vitals, {0, 1}, {0, 1});

		auto* VitalsItems = Make<UCanvasPanel>(Tree, TEXT("VitalsItems"));

		Vitals->SetContent(VitalsItems);

		for (int32 I = 0; I < 2; ++I)
		{
			const bool bHealth = I == 0;
			const TCHAR* TextName = bHealth ? TEXT("HealthText") : TEXT("StaminaText");
			auto* Icon = Label(Tree,
			                   bHealth ? TEXT("HealthIcon") : TEXT("StaminaIcon"),
			                   FText::AsCultureInvariant(bHealth ? TEXT("+") : TEXT("»")),
			                   24,
			                   Accent);

			Place(VitalsItems, Icon, {16, 10.0 + I * 63}, {32, 35});

			auto* Caption = Label(Tree, TextName, MazeText::Widget(TextName), 12, Ink);

			Place(VitalsItems, Caption, {58, 20.0 + I * 63}, {150, 28});

			const TCHAR* ValueName = bHealth ? TEXT("HealthValueText") : TEXT("StaminaValueText");
			auto* Value = Label(Tree, ValueName, FText::AsNumber(100), 14, bHealth ? Ink : Accent);

			Value->SetJustification(ETextJustify::Right);
			Place(VitalsItems, Value, {390, 20.0 + I * 63}, {36, 28});

			auto* Bar = Make<UProgressBar>(Tree, bHealth ? TEXT("HealthBar") : TEXT("StaminaBar"));
			FProgressBarStyle Style;
			FSlateBrush Track = *FCoreStyle::Get().GetBrush("WhiteBrush");

			Track.TintColor = Line.CopyWithNewOpacity(0.55f);
			Style.SetBackgroundImage(Track);

			FSlateBrush FillBrush = *FCoreStyle::Get().GetBrush("WhiteBrush");

			Style.SetFillImage(FillBrush);
			Bar->SetWidgetStyle(Style);
			Bar->SetBarFillStyle(EProgressBarFillStyle::Scale);
			Bar->SetBorderPadding(FVector2D::ZeroVector);
			Bar->SetPercent(1.f);
			Bar->SetFillColorAndOpacity(Accent);
			Place(VitalsItems, Bar, {232, 30.0 + I * 63}, {146, 6});
		}

		auto* Inventory = Make<UCanvasPanel>(Tree, TEXT("InventorySlots"));

		Place(Content, Inventory, {0, -32}, {328, 96});
		Anchor(Inventory, {0.5, 1}, {0.5, 1});

		for (int32 I = 0; I < 4; ++I)
		{
			const float X = I * 84.f;
			auto* Slot = Make<UImage>(Tree, *FString::Printf(TEXT("InventorySlot%d"), I + 1));

			Slot->SetBrush(Outlined(Glass.CopyWithNewOpacity(0.74f), Line));
			Slot->SetVisibility(ESlateVisibility::HitTestInvisible);
			Place(Inventory, Slot, {X, 20}, {76, 76});

			auto* Number =
			    Label(Tree, *FString::Printf(TEXT("InventorySlot%dNumber"), I + 1), FText::AsNumber(I + 1), 12, Muted);

			Number->SetJustification(ETextJustify::Center);
			Place(Inventory, Number, {X + 26, 0}, {24, 18});
		}

		for (bool bDeath : {true, false})
		{
			auto* Panel = Make<UBorder>(Tree, bDeath ? TEXT("DeathPanel") : TEXT("ExitPanel"));

			Panel->SetBrush(Outlined(Glass, Line));
			Panel->SetPadding(FMargin(0));
			Place(Content, Panel, {0, 0}, {720, 214});
			Anchor(Panel, {0.5, 0.5}, {0.5, 0.5});

			auto* Items = Make<UCanvasPanel>(Tree, bDeath ? TEXT("DeathItems") : TEXT("ExitItems"));

			Panel->SetContent(Items);
			Frame(Tree, Items, bDeath ? TEXT("DeathFrame") : TEXT("ExitFrame"), {0, 0}, {720, 214});

			auto* Title = Label(Tree,
			                    bDeath ? TEXT("DeathText") : TEXT("ExitText"),
			                    MazeText::Widget(bDeath ? TEXT("DeathText") : TEXT("ExitText")),
			                    30,
			                    bDeath ? Ink : Accent);

			Title->SetJustification(ETextJustify::Center);
			Place(Items, Title, {40, 64}, {640, 49});

			Panel->SetVisibility(ESlateVisibility::Collapsed);
		}

		SyncWidgetGuids(Blueprint);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);

		if (Blueprint->Status != BS_Error)
		{
			auto* Default = Cast<UMazeHUDWidget>(Blueprint->GeneratedClass->GetDefaultObject());

			if (Default)
			{
				Default->StaminaColor = Accent;
				Default->ExhaustedColor = Muted;
			}

			if (Save(Blueprint))
				UE_LOG(LogTemp, Display, TEXT("Laby glass HUD rebuilt and saved."));
		}
	}

	void RestyleMap()
	{
		auto* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/UI/WBP_ExplorationMap"));

		if (!Blueprint || !Blueprint->WidgetTree)
			return;

		Blueprint->Modify();

		auto* Tree = Blueprint->WidgetTree.Get();
		auto* Canvas = Cast<UCanvasPanel>(Tree->RootWidget);

		if (!Canvas)
			return;

		auto* Mini = Tree->FindWidget(TEXT("MinimapBounds"));

		if (!Mini)
		{
			Mini = Make<USizeBox>(Tree, TEXT("MinimapBounds"));
			Canvas->AddChild(Mini);
		}

		if (auto* Slot = Cast<UCanvasPanelSlot>(Mini->Slot))
		{
			Slot->SetAnchors(FAnchors(1, 0));
			Slot->SetAlignment({1, 0});
			Slot->SetPosition({-36, 28});
			Slot->SetSize({352, 248});
		}

		auto* Full = Tree->FindWidget(TEXT("FullMapBounds"));

		if (!Full)
		{
			Full = Make<USizeBox>(Tree, TEXT("FullMapBounds"));
			Canvas->AddChild(Full);
		}

		if (auto* Slot = Cast<UCanvasPanelSlot>(Full->Slot))
		{
			Slot->SetAnchors(FAnchors(0, 0, 1, 1));
			Slot->SetOffsets(FMargin(26));
		}

		SyncWidgetGuids(Blueprint);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);

		if (Blueprint->Status != BS_Error && Save(Blueprint))
			UE_LOG(LogTemp, Display, TEXT("Laby glass exploration map bounds saved."));
	}

	void Apply()
	{
		if (!GEditor || GEditor->PlayWorld)
		{
			UE_LOG(LogTemp, Warning, TEXT("Laby glass UI authoring is editor-only and does not run during Play."));

			return;
		}

		UTexture2D* Shell = nullptr;
		UTexture2D* Plate = nullptr;
		UTexture2D* HUDShell = nullptr;

		const TCHAR* Names[] = {TEXT("WBP_MainMenu"), TEXT("WBP_PauseMenu"), TEXT("WBP_Settings")};

		for (int32 I = 0; I < UE_ARRAY_COUNT(Names); ++I)
		{
			auto* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *(FString(TEXT("/Game/UI/")) + Names[I]));

			if (!Blueprint)
			{
				UE_LOG(LogTemp, Error, TEXT("Laby glass UI asset missing: %s"), Names[I]);
				continue;
			}

			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(Blueprint);
			BuildMenu(Blueprint, I == 1, I == 2, nullptr, Shell, Plate);
		}

		RestyleHUD(HUDShell);
		RestyleMap();
	}

	void Preview()
	{
		if (!GEditor || GEditor->PlayWorld)
			return;

		const FString Folder = FPaths::ProjectSavedDir() / TEXT("UI/GlassPreview");

		IFileManager::Get().MakeDirectory(*Folder, true);

		const auto Render = [&](const TCHAR* Name, TSharedRef<SWidget> Widget, FVector2D Size)
		{
			FWidgetRenderer Renderer(false);
			TStrongObjectPtr<UTextureRenderTarget2D> Target(NewObject<UTextureRenderTarget2D>());
			Target->ClearColor = FLinearColor::Transparent;
			Target->InitCustomFormat(FMath::RoundToInt(Size.X), FMath::RoundToInt(Size.Y), PF_FloatRGBA, true);
			Target->UpdateResourceImmediate(true);
			Renderer.DrawWidget(Target.Get(), Widget, Size, 0.f);

			// ScaleBox caches its allocated size during arrangement. Paint a second
			// layout pass so smaller previews include the same fitted layout as a viewport.
			Widget->Invalidate(EInvalidateWidgetReason::Layout);
			Renderer.DrawWidget(Target.Get(), Widget, Size, 0.f);

			// Keep linear light at float precision until encoding display sRGB once.
			// An 8-bit linear target loses shadow detail in the dark glass/backdrop.
			TArray<FFloat16Color> LinearPixels;
			TArray<FColor> Pixels;
			TArray64<uint8> Bytes;

			if (Target->GameThread_GetRenderTargetResource()->ReadFloat16Pixels(LinearPixels))
			{
				Pixels.Reserve(LinearPixels.Num());

				for (const FFloat16Color& Pixel : LinearPixels)
					Pixels.Add(FLinearColor(Pixel).ToFColorSRGB());

				FImageUtils::PNGCompressImageArray(Target->SizeX, Target->SizeY, Pixels, Bytes);

				if (!Bytes.IsEmpty() && FFileHelper::SaveArrayToFile(Bytes, *(Folder / (FString(Name) + TEXT(".png")))))
				{
					UE_LOG(LogTemp, Display, TEXT("Laby glass static preview: %s"), Name);
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("Laby glass preview could not be saved: %s"), Name);
				}
			}
		};
		const TCHAR* Names[] = {TEXT("WBP_MainMenu"),
		                        TEXT("WBP_PauseMenu"),
		                        TEXT("WBP_Settings"),
		                        TEXT("SettingsControls"),
		                        TEXT("SettingsGame"),
		                        TEXT("WBP_HUD"),
		                        TEXT("WBP_ExplorationMap"),
		                        TEXT("Minimap"),
		                        TEXT("Death"),
		                        TEXT("Exit"),
		                        TEXT("MainMenu1280")};

		for (int32 I = 0; I < UE_ARRAY_COUNT(Names); ++I)
		{
			const TCHAR* AssetName = I == 3 || I == 4 ? TEXT("WBP_Settings")
			                         : I == 7         ? TEXT("WBP_ExplorationMap")
			                         : I == 10        ? TEXT("WBP_MainMenu")
			                         : I >= 8         ? TEXT("WBP_HUD")
			                                          : Names[I];
			UClass* Class =
			    LoadClass<UUserWidget>(nullptr, *FString::Printf(TEXT("/Game/UI/%s.%s_C"), AssetName, AssetName));

			if (!Class)
				continue;

			TStrongObjectPtr<UUserWidget> Widget(NewObject<UUserWidget>(GetTransientPackage(), Class));

			Widget->SetDesignerFlags(EWidgetDesignFlags::Designing);
			Widget->Initialize();

			if (I >= 2 && I <= 4)
			{
				const int32 Page = I - 2;

				CastChecked<UWidgetSwitcher>(Widget->GetWidgetFromName(TEXT("SettingsPages")))
				    ->SetActiveWidgetIndex(Page);

				const TCHAR* Tabs[] = {TEXT("VideoTabButton"), TEXT("ControlsTabButton"), TEXT("GameTabButton")};
				const FText Titles[] = {NSLOCTEXT("Maze.Glass", "Video", "ВИДЕО"),
				                        NSLOCTEXT("Maze.Glass", "Controls", "УПРАВЛЕНИЕ"),
				                        NSLOCTEXT("Maze.Glass", "Game", "ИГРА")};

				CastChecked<UTextBlock>(Widget->GetWidgetFromName(TEXT("SettingsSectionTitle")))->SetText(Titles[Page]);

				for (int32 Tab = 0; Tab < 3; ++Tab)
				{
					CastChecked<UTextBlock>(Widget->GetWidgetFromName(*(FString(Tabs[Tab]) + TEXT("Label"))))
					    ->SetColorAndOpacity(Tab == Page ? Accent : Ink);
					Widget->GetWidgetFromName(*(FString(Tabs[Tab]) + TEXT("Indicator")))
					    ->SetVisibility(Tab == Page ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
				}

				for (const TCHAR* ComboName : {TEXT("QualityCombo"), TEXT("ShadowsCombo"), TEXT("FrameLimitCombo")})
					CastChecked<UComboBoxString>(Widget->GetWidgetFromName(ComboName))->SetSelectedIndex(2);
			}

			if (I == 8 || I == 9)
				Widget->GetWidgetFromName(I == 8 ? TEXT("DeathPanel") : TEXT("ExitPanel"))
				    ->SetVisibility(ESlateVisibility::HitTestInvisible);

			// Designer visibility normally ignores runtime Collapsed/Hidden. Respect the
			// chosen preview state on these transient instances, without altering assets.
			Widget->WidgetTree->ForEachWidget(
			    [](UWidget* Item)
			    {
				    Item->bHiddenInDesigner = Item->GetVisibility() == ESlateVisibility::Collapsed ||
				                              Item->GetVisibility() == ESlateVisibility::Hidden;
			    });

			const TSharedRef<SWidget> SlateWidget = Widget->TakeWidget();

			Widget->ForceLayoutPrepass();

			const FVector2D Size = I == 7    ? FVector2D(640, 640)
			                       : I == 10 ? FVector2D(1280, 720)
			                                 : FVector2D(1920, 1080);
			const TSharedRef<SOverlay> Surface =
			    SNew(SOverlay) +
			    SOverlay::Slot()[SNew(SBorder)
			                         .Padding(0)
			                         .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			                         .BorderBackgroundColor(FLinearColor::FromSRGBColor(FColor(2, 8, 7)))] +
			    SOverlay::Slot()[SlateWidget];
			TStrongObjectPtr<UUserWidget> Mini;

			if (I == 5)
			{
				UClass* MapClass =
				    LoadClass<UUserWidget>(nullptr, TEXT("/Game/UI/WBP_ExplorationMap.WBP_ExplorationMap_C"));

				Mini.Reset(NewObject<UUserWidget>(GetTransientPackage(), MapClass));
				Mini->SetDesignerFlags(EWidgetDesignFlags::Designing);
				Mini->Initialize();
				Surface->AddSlot()
				    .HAlign(HAlign_Right)
				    .VAlign(VAlign_Top)[SNew(SBox).WidthOverride(640).HeightOverride(640)[Mini->TakeWidget()]];
			}

			Render(Names[I], Surface, Size);

			Widget->ReleaseSlateResources(true);

			if (Mini.IsValid())
				Mini->ReleaseSlateResources(true);
		}

		Render(TEXT("Loading"), MazeInterfaceStyle::MakeLoadingScreen(), {1920, 1080});
	}

	FAutoConsoleCommand ApplyCommand(
	    TEXT("laby.UI.ApplyGlassStyle"),
	    TEXT("Explicitly author the glass menu Widget Blueprints and restyle the existing HUD. Does not start Play."),
	    FConsoleCommandDelegate::CreateStatic(&Apply));
	FAutoConsoleCommand PreviewCommand(
	    TEXT("laby.UI.PreviewGlassStyle"),
	    TEXT("Render menu designer previews into Saved/UI/GlassPreview without starting gameplay."),
	    FConsoleCommandDelegate::CreateStatic(&Preview));
}
