#include "UI/MazeWidgets.h"
#include "UI/MazeText.h"
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

namespace MazeArcUI
{
	const FLinearColor Ink(0.83f, 0.85f, 0.86f);
	const FLinearColor Muted(0.39f, 0.43f, 0.46f);
	const FLinearColor Accent(0.8f, 0.65f, 0.39f);

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

	float Smooth(float A, float B, float V)
	{
		const float T = FMath::Clamp((V - A) / (B - A), 0.f, 1.f);

		return T * T * (3.f - 2.f * T);
	}

	// Baked editor-only artwork: four UI textures, no runtime capture, blur, material tick or extra scene.
	FLinearColor Art(int32 Kind, float U, float V)
	{
		if (Kind == 0)
		{
			const float X = (U - 0.73f) * 1.78f, Y = V - 0.46f;
			const float R = FMath::Sqrt(X * X + Y * Y);
			float Tone = 0.006f + 0.045f * FMath::Exp(-R * 3.4f);
			// A quiet perspective relief of architectural ribs on the right.
			const float Perspective = FMath::Atan2(Y, X);
			const float Rib = FMath::Pow(FMath::Max(0.f, FMath::Cos(Perspective * 8.f + R * 17.f)), 26.f);

			Tone += Rib * 0.035f * Smooth(0.42f, 0.8f, U);

			const float Grain = FMath::Frac(FMath::Sin(U * 9452.f + V * 12932.f) * 43758.f);

			Tone += Grain * 0.002f;

			return FLinearColor(Tone * 0.92f, Tone, Tone * 1.08f, 1.f);
		}

		if (Kind == 1)
		{
			const float X = U - 0.06f, Y = V - 0.5f;
			const float R = FMath::Sqrt(X * X + Y * Y);
			const float Angle = FMath::Atan2(Y, X);
			const float A = 1.f - Smooth(1.48f, 1.56f, FMath::Abs(Angle));
			const float Outer = (1.f - Smooth(0.594f, 0.598f, R)) * Smooth(0.442f, 0.448f, R);
			const float Inner = (1.f - Smooth(0.434f, 0.438f, R)) * Smooth(0.418f, 0.422f, R);
			const float Rim = FMath::Exp(-FMath::Square((R - 0.59f) * 700.f));
			const float InnerRim = FMath::Exp(-FMath::Square((R - 0.45f) * 640.f));
			const float Brushed = FMath::Sin(R * 9500.f) * 0.003f;
			float Tone = 0.055f + 0.12f * (1.f - V) + Rim * 0.35f + InnerRim * 0.09f + Brushed;
			const float Notch = FMath::Frac((Angle + PI) / PI * 72.f);

			if (R > 0.557f && R < 0.578f && Notch < 0.065f)
				Tone *= 0.18f;

			return FLinearColor(Tone * 0.94f, Tone, Tone * 1.04f, FMath::Max(Outer, Inner * 0.85f) * A);
		}

		const float Edge = Kind == 2 ? 0.29f + 0.15f * FMath::Sin(U * PI) : 0.23f + 0.24f * FMath::Sin(U * PI);
		const float D = Edge - FMath::Abs(V - 0.5f);
		const float Alpha = Smooth(0.f, 0.012f, D) * Smooth(0.f, 0.025f, U) * (1.f - Smooth(0.976f, 1.f, U));
		const float Bevel = FMath::Exp(-FMath::Square((D - 0.018f) * 90.f));
		float Tone = (Kind == 2 ? 0.15f : 0.045f) + (1.f - V) * 0.075f + Bevel * (V < 0.5f ? 0.25f : 0.06f);

		Tone += FMath::Sin(V * 450.f) * 0.002f;

		return FLinearColor(Tone * 0.94f, Tone, Tone * 1.03f, Alpha);
	}

	UTexture2D* Texture(const TCHAR* Name, int32 Kind, int32 Width, int32 Height)
	{
		const FString Path = FString(TEXT("/Game/UI/Arc/")) + Name;
		auto* Result = LoadObject<UTexture2D>(nullptr, *Path);

		if (Result)
			return Result;

		UPackage* Package = CreatePackage(*Path);

		Result = NewObject<UTexture2D>(Package, Name, RF_Public | RF_Standalone | RF_Transactional);

		TArray<FColor> Pixels;

		Pixels.SetNumUninitialized(Width * Height);

		for (int32 Y = 0; Y < Height; ++Y)
			for (int32 X = 0; X < Width; ++X)
				Pixels[Y * Width + X] = Art(Kind, (X + 0.5f) / Width, (Y + 0.5f) / Height).ToFColor(true);

		Result->PreEditChange(nullptr);
		Result->Source.Init(Width, Height, 1, 1, TSF_BGRA8, reinterpret_cast<const uint8*>(Pixels.GetData()));
		Result->SRGB = true;
		Result->CompressionSettings = TC_EditorIcon;
		Result->LODGroup = TEXTUREGROUP_UI;
		Result->MipGenSettings = TMGS_NoMipmaps;
		Result->NeverStream = true;
		Result->Filter = TF_Bilinear;
		Result->PostEditChange();
		FAssetRegistryModule::AssetCreated(Result);

		if (!Save(Result))
			UE_LOG(LogTemp, Error, TEXT("Laby arc texture save failed: %s"), *Path);

		return Result;
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
		Font.TypefaceFontName = TEXT("Regular");
		Widget->SetFont(Font);
		Widget->SetText(Text);
		Widget->SetColorAndOpacity(Color);
		Widget->SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.5f));
		Widget->SetShadowOffset(FVector2D(0, 2));
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

	UButton* Button(UWidgetTree* Tree,
	                UCanvasPanel* Parent,
	                const TCHAR* Name,
	                const FText& Text,
	                UTexture2D* Plate,
	                FVector2D Position,
	                FVector2D Size,
	                float Angle = 0.f)
	{
		auto* Result = Make<UButton>(Tree, Name);
		FButtonStyle Style;

		Style.SetNormal(Brush(Plate, FLinearColor(0.68f, 0.71f, 0.74f)));
		Style.SetHovered(Brush(Plate, FLinearColor(1.f, 0.88f, 0.67f)));
		Style.SetPressed(Brush(Plate, FLinearColor(0.44f, 0.4f, 0.31f)));
		Style.SetDisabled(Brush(Plate, FLinearColor(0.25f, 0.27f, 0.29f)));
		Style.SetNormalForeground(Ink);
		Style.SetHoveredForeground(FLinearColor::White);
		Style.SetNormalPadding(FMargin(28, 10));
		Style.SetPressedPadding(FMargin(30, 12, 26, 8));
		Result->SetStyle(Style);

		auto* TextWidget = Label(Tree, *(FString(Name) + TEXT("Label")), Text, Size.X > 390 ? 28 : 21);

		Result->SetContent(TextWidget);
		CastChecked<UButtonSlot>(TextWidget->Slot)->SetHorizontalAlignment(HAlign_Center);
		CastChecked<UButtonSlot>(TextWidget->Slot)->SetVerticalAlignment(VAlign_Center);
		Result->SetRenderTransformAngle(Angle);
		Place(Parent, Result, Position, Size);

		return Result;
	}

	void Caption(UWidgetTree* Tree, UCanvasPanel* Page, const TCHAR* Name, const FText& Text, float Y)
	{
		Place(Page, Label(Tree, Name, Text, 24), {0, Y + 8}, {475, 42});
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

		Base.TintColor = FLinearColor(0.033f, 0.041f, 0.047f);

		FSlateBrush Hover = Base;

		Hover.TintColor = FLinearColor(0.11f, 0.1f, 0.077f);

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
			Font->Size = 21;
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
		Off.TintColor = FLinearColor(0.1f, 0.12f, 0.13f);

		FSlateBrush On = Off;

		On.TintColor = Accent;

		FSlateBrush Hover = Off;

		Hover.TintColor = FLinearColor(0.35f, 0.31f, 0.23f);
		Style.SetUncheckedImage(Off).SetUncheckedHoveredImage(Hover).SetUncheckedPressedImage(On);
		Style.SetCheckedImage(On).SetCheckedHoveredImage(On).SetCheckedPressedImage(Hover);
		Style.SetPadding(FMargin(8));
		Widget->SetWidgetStyle(Style);

		Widget->SetIsChecked(bChecked);
		Widget->SetContent(
		    Label(Tree, *(FString(Name) + TEXT("Label")), NSLOCTEXT("Maze.Arc", "Enabled", "Включено"), 21));
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
		Widget->SetSliderBarColor(FLinearColor(0.17f, 0.2f, 0.22f));
		Widget->SetSliderHandleColor(Accent);
		Place(Page, Widget, {500, Y + 8}, {275, 42});
		Place(Page, Label(Tree, TextName, ValueText, 22, Accent), {798, Y + 6}, {102, 44});
	}

	void Settings(UWidgetTree* Tree, UCanvasPanel* Stage, UTexture2D* Plate)
	{
		Button(Tree,
		       Stage,
		       TEXT("VideoTabButton"),
		       NSLOCTEXT("Maze.Arc", "Video", "ВИДЕО"),
		       Plate,
		       {210, 280},
		       {355, 108},
		       -9);
		Button(Tree,
		       Stage,
		       TEXT("ControlsTabButton"),
		       NSLOCTEXT("Maze.Arc", "Controls", "УПРАВЛЕНИЕ"),
		       Plate,
		       {295, 485},
		       {355, 108});
		Button(Tree,
		       Stage,
		       TEXT("GameTabButton"),
		       NSLOCTEXT("Maze.Arc", "Game", "ИГРА"),
		       Plate,
		       {210, 690},
		       {355, 108},
		       9);

		auto* Pages = Make<UWidgetSwitcher>(Tree, TEXT("SettingsPages"));

		Place(Stage, Pages, {805, 275}, {900, 555});

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
		Place(Video,
		      Label(Tree,
		            TEXT("VideoHint"),
		            NSLOCTEXT("Maze.Arc", "VideoHint", "Параметры качества применяются к текущему изображению."),
		            18,
		            Muted),
		      {0, 490},
		      {900, 40});

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
		      {0, 238},
		      {900, 40});
		Place(Controls,
		      Label(Tree,
		            TEXT("BindingsMovement"),
		            NSLOCTEXT("Maze.Arc",
		                      "BindingsMovement",
		                      "W A S D   Движение     /     SHIFT   Бег     /     SPACE   Прыжок"),
		            21),
		      {0, 300},
		      {910, 50});
		Place(
		    Controls,
		    Label(Tree,
		          TEXT("BindingsActions"),
		          NSLOCTEXT("Maze.Arc", "BindingsActions", "CTRL   Присесть     /     L   Фонарик     /     M   Карта"),
		          21),
		    {0, 365},
		    {910, 50});
		Place(Controls,
		      Label(Tree,
		            TEXT("BindingsMenu"),
		            NSLOCTEXT("Maze.Arc", "BindingsMenu", "ESC   Меню     /     TAB и ENTER   Выбор пункта"),
		            21),
		      {0, 430},
		      {910, 50});

		Caption(Tree, Game, TEXT("FieldOfViewLabel"), NSLOCTEXT("Maze.Arc", "FOV", "Поле зрения"), 0);
		Slider(Tree,
		       Game,
		       TEXT("FieldOfViewSlider"),
		       TEXT("FieldOfViewText"),
		       0,
		       70,
		       110,
		       95,
		       5,
		       FText::AsCultureInvariant(TEXT("95°")));
		Caption(Tree, Game, TEXT("CompassLabel"), NSLOCTEXT("Maze.Arc", "Compass", "Компас на карте"), 96);
		Check(Tree, Game, TEXT("CompassCheck"), 96, true);
		Caption(Tree, Game, TEXT("CrosshairLabel"), NSLOCTEXT("Maze.Arc", "Crosshair", "Точка в центре экрана"), 192);
		Check(Tree, Game, TEXT("CrosshairCheck"), 192, true);
		Caption(Tree, Game, TEXT("CompactHUDLabel"), NSLOCTEXT("Maze.Arc", "CompactHUD", "Компактные индикаторы"), 288);
		Check(Tree, Game, TEXT("CompactHUDCheck"), 288, false);
		Caption(
		    Tree, Game, TEXT("CameraMotionLabel"), NSLOCTEXT("Maze.Arc", "CameraMotion", "Покачивание камеры"), 384);
		Check(Tree, Game, TEXT("CameraMotionCheck"), 384, true);

		Place(
		    Stage,
		    Label(Tree,
		          TEXT("SettingsStatus"),
		          NSLOCTEXT("Maze.Arc", "SettingsStatus", "Изменения сохраняются кнопкой «Применить». Назад — отмена."),
		          18,
		          Muted),
		    {805, 848},
		    {975, 40});
		Button(Tree,
		       Stage,
		       TEXT("ApplyButton"),
		       NSLOCTEXT("Maze.Arc", "Apply", "ПРИМЕНИТЬ"),
		       Plate,
		       {800, 900},
		       {300, 84});
		Button(Tree,
		       Stage,
		       TEXT("ResetButton"),
		       NSLOCTEXT("Maze.Arc", "Reset", "ИСХОДНЫЕ"),
		       Plate,
		       {1120, 900},
		       {300, 84});
		Button(Tree, Stage, TEXT("BackButton"), NSLOCTEXT("Maze.Arc", "Back", "НАЗАД"), Plate, {1440, 900}, {300, 84});
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

		auto* Background = Make<UImage>(Tree, TEXT("Background"));

		Background->SetBrush(Brush(Backdrop));
		Background->SetVisibility(ESlateVisibility::HitTestInvisible);
		Fill(Root, Background);

		auto* Scale = Make<UScaleBox>(Tree, TEXT("MenuScale"));

		Scale->SetStretch(EStretch::ScaleToFit);
		Fill(Root, Scale);

		auto* Bounds = Make<USizeBox>(Tree, TEXT("DesignBounds"));

		Bounds->SetWidthOverride(1920);
		Bounds->SetHeightOverride(1080);
		Scale->SetContent(Bounds);

		auto* Stage = Make<UCanvasPanel>(Tree, TEXT("ArcStage"));

		Bounds->SetContent(Stage);
		Image(Tree,
		      Stage,
		      TEXT("ArcShell"),
		      Shell,
		      bSettings ? FVector2D(-180, -180) : FVector2D(-100, -180),
		      {1440, 1440});
		Place(Stage,
		      Label(Tree, TEXT("BrandMark"), FText::AsCultureInvariant(TEXT("L /")), 28, Accent),
		      {92, 68},
		      {200, 60});
		Place(Stage,
		      Label(Tree, TEXT("Subtitle"), MazeText::Subtitle(bSettings, bPause), 22, Muted),
		      bSettings ? FVector2D(805, 160) : FVector2D(148, 585),
		      {920, 50});

		if (bSettings)
		{
			Place(Stage,
			      Label(Tree, TEXT("SettingsHeading"), NSLOCTEXT("Maze.Arc", "SettingsHeading", "НАСТРОЙКИ"), 54),
			      {805, 80},
			      {1000, 90});
			Settings(Tree, Stage, Plate);
		}
		else
		{
			Place(Stage, Label(Tree, TEXT("Title"), MazeText::Widget(TEXT("Title")), 76), {142, 446}, {470, 120});

			if (bPause)
			{
				Button(Tree,
				       Stage,
				       TEXT("ResumeButton"),
				       MazeText::Widget(TEXT("ResumeButtonLabel")),
				       Plate,
				       {605, 180},
				       {515, 125},
				       -12);
				Button(Tree,
				       Stage,
				       TEXT("SettingsButton"),
				       MazeText::Widget(TEXT("SettingsButtonLabel")),
				       Plate,
				       {800, 395},
				       {515, 125},
				       -4);
				Button(Tree,
				       Stage,
				       TEXT("MainMenuButton"),
				       NSLOCTEXT("Maze.Arc", "MainMenu", "ГЛАВНОЕ МЕНЮ"),
				       Plate,
				       {800, 610},
				       {515, 125},
				       4);
				Button(Tree,
				       Stage,
				       TEXT("QuitButton"),
				       MazeText::Widget(TEXT("QuitButtonLabel")),
				       Plate,
				       {605, 825},
				       {515, 125},
				       12);
			}
			else
			{
				Button(Tree,
				       Stage,
				       TEXT("NewGameButton"),
				       MazeText::Widget(TEXT("NewGameButtonLabel")),
				       Plate,
				       {710, 260},
				       {535, 130},
				       -10);
				Button(Tree,
				       Stage,
				       TEXT("SettingsButton"),
				       MazeText::Widget(TEXT("SettingsButtonLabel")),
				       Plate,
				       {865, 475},
				       {535, 130});
				Button(Tree,
				       Stage,
				       TEXT("QuitButton"),
				       MazeText::Widget(TEXT("QuitButtonLabel")),
				       Plate,
				       {710, 690},
				       {535, 130},
				       10);
			}

			Place(Stage,
			      Label(Tree,
			            TEXT("NavigationHint"),
			            NSLOCTEXT("Maze.Arc", "Navigation", "TAB   ВЫБОР     /     ENTER   ПОДТВЕРДИТЬ"),
			            17,
			            Muted),
			      {112, 988},
			      {1000, 40});
		}

		SyncWidgetGuids(Blueprint);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);

		if (Blueprint->Status != BS_Error && Save(Blueprint))
		{
			UE_LOG(LogTemp, Display, TEXT("Laby arc UI saved: %s"), *Blueprint->GetPathName());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Laby arc UI compilation/save failed: %s"), *Blueprint->GetPathName());
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
		auto* Version = Label(Tree, TEXT("VersionText"), MazeText::Widget(TEXT("VersionText")), 11, Muted);

		Place(Content, Version, {34, 26}, {240, 20});

		auto* Crosshair = Make<UImage>(Tree, TEXT("Crosshair"));

		Crosshair->SetBrush(*FCoreStyle::Get().GetBrush("WhiteBrush"));
		Crosshair->SetColorAndOpacity(Ink);
		Crosshair->SetVisibility(ESlateVisibility::HitTestInvisible);
		Place(Content, Crosshair, {0, 0}, {3, 3});
		Anchor(Crosshair, {0.5, 0.5}, {0.5, 0.5});

		auto* Vitals = Make<UBorder>(Tree, TEXT("VitalsPanel"));

		Vitals->SetBrush(Brush(Shell));
		Vitals->SetPadding(FMargin(0));
		Place(Content, Vitals, {30, -58}, {420, 158});
		Anchor(Vitals, {0, 1}, {0, 1});

		auto* VitalsItems = Make<UCanvasPanel>(Tree, TEXT("VitalsItems"));

		Vitals->SetContent(VitalsItems);

		auto* Mark = Label(Tree, TEXT("VitalsMark"), FText::AsCultureInvariant(TEXT("I /")), 28, Accent);

		Place(VitalsItems, Mark, {27, 63}, {46, 46});

		for (int32 I = 0; I < 2; ++I)
		{
			const bool bHealth = I == 0;
			const TCHAR* TextName = bHealth ? TEXT("HealthText") : TEXT("StaminaText");
			auto* Caption = Label(Tree, TextName, MazeText::Widget(TextName), 12, Ink);

			Place(VitalsItems, Caption, {87, 38.0 + I * 49}, {288, 23});

			auto* Bar = Make<UProgressBar>(Tree, bHealth ? TEXT("HealthBar") : TEXT("StaminaBar"));
			FProgressBarStyle Style;
			FSlateBrush Track = *FCoreStyle::Get().GetBrush("WhiteBrush");

			Track.TintColor = FLinearColor(0.025f, 0.03f, 0.032f);
			Style.SetBackgroundImage(Track);

			FSlateBrush FillBrush = *FCoreStyle::Get().GetBrush("WhiteBrush");

			Style.SetFillImage(FillBrush);
			Bar->SetWidgetStyle(Style);
			Bar->SetBarFillStyle(EProgressBarFillStyle::Scale);
			Bar->SetPercent(1.f);
			Bar->SetFillColorAndOpacity(bHealth ? FLinearColor(0.65f, 0.72f, 0.66f) : Accent);
			Place(VitalsItems, Bar, {87, 65.0 + I * 49}, {278, 4});
		}

		auto* Hint = Label(
		    Tree, TEXT("HUDHint"), NSLOCTEXT("Maze.Arc", "HUDHint", "ESC   ПАУЗА     /     M   КАРТА"), 11, Muted);

		Place(Content, Hint, {57, -35}, {440, 20});
		Anchor(Hint, {0, 1}, {0, 1});

		// Keep the explicit F7 developer view available separately from the exploration map.
		auto* DevMap = Make<UBorder>(Tree, TEXT("MinimapPanel"));

		DevMap->SetBrushColor(FLinearColor(0.015f, 0.019f, 0.022f, 0.95f));
		DevMap->SetPadding(FMargin(18));
		Place(Content, DevMap, {-34, 30}, {312, 312});
		Anchor(DevMap, {1, 0}, {1, 0});

		auto* Map = Make<UMazeMinimapWidget>(Tree, TEXT("Minimap"));

		Map->WallColor = Ink;
		Map->PlayerColor = Accent;
		DevMap->SetContent(Map);
		DevMap->SetVisibility(ESlateVisibility::Collapsed);

		for (const TCHAR* Name : {TEXT("SessionText"), TEXT("DeveloperHint")})
		{
			auto* LabelWidget = Label(Tree, Name, MazeText::Widget(Name), 11, Muted);

			Place(Content, LabelWidget, {34, FName(Name) == FName(TEXT("SessionText")) ? 62.0 : 82.0}, {720, 20});
			LabelWidget->SetVisibility(ESlateVisibility::Collapsed);
		}

		for (bool bDeath : {true, false})
		{
			auto* Panel = Make<UBorder>(Tree, bDeath ? TEXT("DeathPanel") : TEXT("ExitPanel"));

			Panel->SetBrush(Brush(Shell));
			Panel->SetPadding(FMargin(0));
			Place(Content, Panel, {0, 0}, {680, 188});
			Anchor(Panel, {0.5, 0.5}, {0.5, 0.5});

			auto* Items = Make<UCanvasPanel>(Tree, bDeath ? TEXT("DeathItems") : TEXT("ExitItems"));

			Panel->SetContent(Items);

			auto* Title = Label(Tree,
			                    bDeath ? TEXT("DeathText") : TEXT("ExitText"),
			                    MazeText::Widget(bDeath ? TEXT("DeathText") : TEXT("ExitText")),
			                    30,
			                    bDeath ? FLinearColor(0.8f, 0.39f, 0.28f) : Ink);

			Title->SetJustification(ETextJustify::Center);
			Place(Items, Title, {50, 58}, {580, 49});

			auto* Message = Label(
			    Tree, bDeath ? TEXT("DeathHint") : TEXT("ExitHint"), MazeText::Widget(TEXT("DeathHint")), 13, Muted);

			Message->SetJustification(ETextJustify::Center);
			Place(Items, Message, {50, 114}, {580, 26});
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
				Default->ExhaustedColor = FLinearColor(0.9f, 0.35f, 0.2f);
			}

			if (Save(Blueprint))
				UE_LOG(LogTemp, Display, TEXT("Laby arc HUD rebuilt and saved."));
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
			Slot->SetAnchors(FAnchors(1, 1));
			Slot->SetAlignment({1, 1});
			Slot->SetPosition({-36, -64});
			Slot->SetSize({312, 312});
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
			Slot->SetOffsets(FMargin(40));
		}

		SyncWidgetGuids(Blueprint);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);

		if (Blueprint->Status != BS_Error && Save(Blueprint))
			UE_LOG(LogTemp, Display, TEXT("Laby arc exploration map bounds saved."));
	}

	void Apply()
	{
		if (!GEditor || GEditor->PlayWorld)
		{
			UE_LOG(LogTemp, Warning, TEXT("Laby arc UI authoring is editor-only and does not run during Play."));

			return;
		}

		UTexture2D* Backdrop = Texture(TEXT("T_MenuBackdrop"), 0, 1024, 576);
		UTexture2D* Shell = Texture(TEXT("T_ArcShell"), 1, 1024, 1024);
		UTexture2D* Plate = Texture(TEXT("T_ArcButton"), 2, 512, 128);
		UTexture2D* HUDShell = Texture(TEXT("T_HUDShell"), 3, 512, 256);
		const TCHAR* Names[] = {TEXT("WBP_MainMenu"), TEXT("WBP_PauseMenu"), TEXT("WBP_Settings")};

		for (int32 I = 0; I < UE_ARRAY_COUNT(Names); ++I)
		{
			auto* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *(FString(TEXT("/Game/UI/")) + Names[I]));

			if (!Blueprint)
			{
				UE_LOG(LogTemp, Error, TEXT("Laby arc UI asset missing: %s"), Names[I]);
				continue;
			}

			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(Blueprint);
			BuildMenu(Blueprint, I == 1, I == 2, Backdrop, Shell, Plate);
		}

		RestyleHUD(HUDShell);
		RestyleMap();
	}

	void Preview()
	{
		if (!GEditor || GEditor->PlayWorld)
			return;

		const FString Folder = FPaths::ProjectSavedDir() / TEXT("UI/ArcPreview");

		IFileManager::Get().MakeDirectory(*Folder, true);

		const TCHAR* Names[] = {TEXT("WBP_MainMenu"),
		                        TEXT("WBP_PauseMenu"),
		                        TEXT("WBP_Settings"),
		                        TEXT("SettingsControls"),
		                        TEXT("SettingsGame"),
		                        TEXT("WBP_HUD"),
		                        TEXT("WBP_ExplorationMap"),
		                        TEXT("Minimap")};

		for (int32 I = 0; I < UE_ARRAY_COUNT(Names); ++I)
		{
			const TCHAR* AssetName = I == 3 || I == 4 ? TEXT("WBP_Settings")
			                         : I == 7         ? TEXT("WBP_ExplorationMap")
			                                          : Names[I];
			UClass* Class =
			    LoadClass<UUserWidget>(nullptr, *FString::Printf(TEXT("/Game/UI/%s.%s_C"), AssetName, AssetName));

			if (!Class)
				continue;

			TStrongObjectPtr<UUserWidget> Widget(NewObject<UUserWidget>(GetTransientPackage(), Class));

			if (I >= 6)
				Widget->SetDesignerFlags(EWidgetDesignFlags::Designing);

			Widget->Initialize();

			const TSharedRef<SWidget> SlateWidget = Widget->TakeWidget();

			if (I == 3 || I == 4)
			{
				auto* Pages = Cast<UWidgetSwitcher>(Widget->GetWidgetFromName(TEXT("SettingsPages")));

				if (Pages)
					Pages->SetActiveWidgetIndex(I - 2);

				const TCHAR* Tabs[] = {TEXT("VideoTabButton"), TEXT("ControlsTabButton"), TEXT("GameTabButton")};

				for (int32 Tab = 0; Tab < 3; ++Tab)
					if (auto* Button = Cast<UButton>(Widget->GetWidgetFromName(Tabs[Tab])))
						Button->SetBackgroundColor(Tab == I - 2 ? FLinearColor(0.8f, 0.68f, 0.45f)
						                                        : FLinearColor(0.48f, 0.51f, 0.54f));
			}

			Widget->ForceLayoutPrepass();

			// A fresh widget per view avoids stale ScaleBox/invalidation geometry across virtual windows.
			FWidgetRenderer Renderer(false);
			const FVector2D Size = I == 7 ? FVector2D(640, 640) : FVector2D(1920, 1080);
			TStrongObjectPtr<UTextureRenderTarget2D> Target(Renderer.DrawWidget(SlateWidget, Size));
			FBufferArchive Bytes;

			if (Target.IsValid() && FImageUtils::ExportRenderTarget2DAsPNG(Target.Get(), Bytes))
			{
				FFileHelper::SaveArrayToFile(Bytes, *(Folder / (FString(Names[I]) + TEXT(".png"))));
				UE_LOG(LogTemp, Display, TEXT("Laby arc static preview: %s"), Names[I]);
			}

			Widget->ReleaseSlateResources(true);
		}
	}

	FAutoConsoleCommand ApplyCommand(
	    TEXT("laby.UI.ApplyArcStyle"),
	    TEXT("Explicitly author the arc menu Widget Blueprints and restyle the existing HUD. Does not start Play."),
	    FConsoleCommandDelegate::CreateStatic(&Apply));
	FAutoConsoleCommand PreviewCommand(
	    TEXT("laby.UI.PreviewArcStyle"),
	    TEXT("Render menu designer previews into Saved/UI/ArcPreview without starting gameplay."),
	    FConsoleCommandDelegate::CreateStatic(&Preview));
}
