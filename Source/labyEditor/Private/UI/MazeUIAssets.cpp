#include "LiveCoding/MazeAutoLiveCoding.h"
#include "Diagnostics/MazeDiagnosticsToolset.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "Modules/ModuleManager.h"
#include "UI/MazeWidgets.h"
#include "UI/MazeText.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Styling/CoreStyle.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"

namespace
{
	const FLinearColor Background(0.015f, 0.025f, 0.04f, 1.f);

	template <typename T> T* Make(UWidgetTree* Tree, const TCHAR* Name)
	{
		T* Widget = Tree->ConstructWidget<T>(T::StaticClass(), FName(Name));

		Widget->SetFlags(RF_Transactional);
		Widget->bIsVariable = true;

		return Widget;
	}

	UTextBlock* Label(UWidgetTree* Tree,
	                  const TCHAR* Name,
	                  const FText& Value,
	                  int32 Size = 18,
	                  FLinearColor Color = FLinearColor::White)
	{
		auto* Text = Make<UTextBlock>(Tree, Name);

		Text->SetText(Value);

		// Keep UMG's asset-backed font: Slate's shared composite font is not serialized.
		FSlateFontInfo Font = Text->GetFont();

		Font.Size = Size;
		Font.TypefaceFontName = FName("Regular");
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));

		return Text;
	}

	void Row(UVerticalBox* Box, UWidget* Widget, FMargin Padding = FMargin(0, 0, 0, 18))
	{
		Box->AddChildToVerticalBox(Widget)->SetPadding(Padding);
	}

	void Place(UCanvasPanel* Canvas,
	           UWidget* Widget,
	           FVector2D Position,
	           FVector2D Size,
	           FVector2D Anchor = FVector2D::ZeroVector,
	           FVector2D Alignment = FVector2D::ZeroVector)
	{
		auto* Slot = Canvas->AddChildToCanvas(Widget);

		Slot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
		Slot->SetAlignment(Alignment);
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
	}

	void Fill(UCanvasPanel* Canvas, UWidget* Widget)
	{
		auto* Slot = Canvas->AddChildToCanvas(Widget);

		Slot->SetAnchors(FAnchors(0, 0, 1, 1));
		Slot->SetOffsets(FMargin(0));
	}

	UBorder* Panel(UWidgetTree* Tree, const TCHAR* Name, FLinearColor Color = Background)
	{
		auto* Border = Make<UBorder>(Tree, Name);

		Border->SetBrush(*FCoreStyle::Get().GetBrush("WhiteBrush"));
		Border->SetBrushColor(Color);
		Border->SetPadding(FMargin(16));

		return Border;
	}

	void Button(UWidgetTree* Tree, UVerticalBox* Box, const TCHAR* Name, const FText& Value)
	{
		auto* Widget = Make<UButton>(Tree, Name);

		Widget->SetContent(Label(Tree, *FString::Printf(TEXT("%sLabel"), Name), Value, 20));

		// The content padding is editable on the button's slot in the UMG designer.
		if (auto* Slot = Cast<UButtonSlot>(Widget->GetContent()->Slot))
			Slot->SetPadding(FMargin(20, 15));

		Row(Box, Widget, FMargin(0, 5));
	}

	void Menu(UWidgetTree* Tree, bool bPause, bool bSettings)
	{
		auto* Root = Make<UCanvasPanel>(Tree, TEXT("Root"));

		Tree->RootWidget = Root;

		auto* Backdrop = Panel(Tree, TEXT("Background"));

		Backdrop->SetPadding(FMargin(24));
		Fill(Root, Backdrop);

		auto* Scale = Make<UScaleBox>(Tree, TEXT("MenuScale"));

		Scale->SetStretch(EStretch::ScaleToFit);
		Backdrop->SetContent(Scale);

		auto* Size = Make<USizeBox>(Tree, TEXT("MenuWidth"));

		Size->SetWidthOverride(460);
		Scale->SetContent(Size);

		auto* Items = Make<UVerticalBox>(Tree, TEXT("MenuItems"));

		Size->SetContent(Items);
		Row(Items, Label(Tree, TEXT("Title"), MazeText::Widget(TEXT("Title")), 44, FLinearColor(0.75f, 0.9f, 1)));

		if (bSettings)
		{
			Row(Items, Label(Tree, TEXT("Subtitle"), MazeText::Subtitle(true, false), 24));
			Row(Items, Label(Tree, TEXT("SensitivityLabel"), MazeText::Widget(TEXT("SensitivityLabel"))));
			Row(Items, Label(Tree, TEXT("SensitivityText"), MazeText::Widget(TEXT("SensitivityText")), 22));

			auto* Slider = Make<USlider>(Tree, TEXT("SensitivitySlider"));

			Slider->SetMinValue(0.1f);
			Slider->SetMaxValue(3.f);
			Slider->SetValue(1.f);
			Row(Items, Slider, FMargin(0, 8, 0, 24));
			Row(Items, Label(Tree, TEXT("SettingsHint"), MazeText::Widget(TEXT("SettingsHint")), 14));
			Button(Tree, Items, TEXT("ResetButton"), MazeText::Widget(TEXT("ResetButtonLabel")));
			Button(Tree, Items, TEXT("BackButton"), MazeText::Widget(TEXT("BackButtonLabel")));
		}
		else
		{
			Row(Items, Label(Tree, TEXT("Subtitle"), MazeText::Subtitle(false, bPause)));

			if (bPause)
				Button(Tree, Items, TEXT("ResumeButton"), MazeText::Widget(TEXT("ResumeButtonLabel")));

			Button(Tree, Items, TEXT("NewGameButton"), MazeText::Widget(TEXT("NewGameButtonLabel")));
			Button(Tree, Items, TEXT("SettingsButton"), MazeText::Widget(TEXT("SettingsButtonLabel")));
			Button(Tree, Items, TEXT("QuitButton"), MazeText::Widget(TEXT("QuitButtonLabel")));
		}
	}

	void HUD(UWidgetTree* Tree)
	{
		auto* Root = Make<UCanvasPanel>(Tree, TEXT("Root"));

		Root->SetVisibility(ESlateVisibility::HitTestInvisible);
		Tree->RootWidget = Root;

		auto* Content = Make<UCanvasPanel>(Tree, TEXT("HUDContent"));

		Fill(Root, Content);
		Place(
		    Content, Label(Tree, TEXT("VersionText"), MazeText::Widget(TEXT("VersionText")), 14), {20, 20}, {260, 24});

		auto* Crosshair = Make<UImage>(Tree, TEXT("Crosshair"));

		Crosshair->SetBrush(*FCoreStyle::Get().GetBrush("WhiteBrush"));
		Place(Content, Crosshair, {0, 0}, {4, 4}, {0.5f, 0.5f}, {0.5f, 0.5f});

		auto* Vitals = Panel(Tree, TEXT("VitalsPanel"), FLinearColor(0.015f, 0.025f, 0.04f, 0.85f));

		Place(Content, Vitals, {20, -74}, {320, 126}, {0, 1}, {0, 1});

		auto* Bars = Make<UVerticalBox>(Tree, TEXT("VitalsItems"));

		Vitals->SetContent(Bars);
		Row(Bars, Label(Tree, TEXT("StaminaText"), MazeText::Widget(TEXT("StaminaText")), 14), FMargin(0, 0, 0, 6));

		auto* Stamina = Make<UProgressBar>(Tree, TEXT("StaminaBar"));

		Stamina->SetPercent(1.f);
		Stamina->SetFillColorAndOpacity(FLinearColor(0.2f, 0.85f, 0.65f));
		Row(Bars, Stamina, FMargin(0, 0, 0, 14));
		Row(Bars, Label(Tree, TEXT("HealthText"), MazeText::Widget(TEXT("HealthText")), 14), FMargin(0, 0, 0, 6));

		auto* Health = Make<UProgressBar>(Tree, TEXT("HealthBar"));

		Health->SetPercent(1.f);
		Health->SetFillColorAndOpacity(FLinearColor(0.9f, 0.22f, 0.25f));
		Row(Bars, Health, FMargin(0));

		for (bool bDeath : {true, false})
		{
			auto* Message =
			    Panel(Tree,
			          bDeath ? TEXT("DeathPanel") : TEXT("ExitPanel"),
			          bDeath ? FLinearColor(0.08f, 0.01f, 0.02f, 0.94f) : FLinearColor(0.01f, 0.06f, 0.04f, 0.9f));

			Place(Content, Message, {0, 0}, {480, 120}, {0.5f, 0.5f}, {0.5f, 0.5f});

			auto* Items = Make<UVerticalBox>(Tree, bDeath ? TEXT("DeathItems") : TEXT("ExitItems"));

			Message->SetContent(Items);
			Row(Items,
			    Label(Tree,
			          bDeath ? TEXT("DeathText") : TEXT("ExitText"),
			          bDeath ? MazeText::Widget(TEXT("DeathText")) : MazeText::Widget(TEXT("ExitText")),
			          28,
			          bDeath ? FLinearColor(1, 0.3f, 0.3f) : FLinearColor(0.3f, 1, 0.6f)));
			Message->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	void RepairFonts(const FString& PackageName)
	{
		auto* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *PackageName);

		if (!Blueprint || !Blueprint->WidgetTree)
			return;

		const FSlateFontInfo DefaultFont = GetDefault<UTextBlock>()->GetFont();

		if (!DefaultFont.FontObject)
		{
			UE_LOG(LogTemp, Error, TEXT("Laby UI font repair: default UMG font is unavailable."));

			return;
		}

		int32 Repaired = 0;

		Blueprint->WidgetTree->ForEachWidget(
		    [&](UWidget* Widget)
		    {
			    auto* Text = Cast<UTextBlock>(Widget);

			    if (!Text)
				    return;

			    const FText Localized = Text->GetFName() == FName(TEXT("Subtitle"))
			                                ? MazeText::Subtitle(PackageName.EndsWith(TEXT("WBP_Settings")),
			                                                     PackageName.EndsWith(TEXT("WBP_PauseMenu")))
			                                : MazeText::Widget(Text->GetFName());
			    const bool bUpdateText = !Localized.IsEmpty() && !Text->GetText().IdenticalTo(Localized);
			    const bool bRepairFont = !Text->GetFont().FontObject;

			    if (!bUpdateText && !bRepairFont)
				    return;

			    if (Repaired == 0)
				    Blueprint->Modify();

			    Text->Modify();

			    if (bUpdateText)
				    Text->SetText(Localized);

			    if (bRepairFont)
			    {
				    FSlateFontInfo Font = Text->GetFont();
				    Font.FontObject = DefaultFont.FontObject;
				    Font.CompositeFont.Reset();
				    Text->SetFont(Font);
			    }

			    ++Repaired;
		    });

		if (Repaired == 0)
			return;

		FKismetEditorUtilities::CompileBlueprint(Blueprint);

		if (Blueprint->Status == BS_Error)
		{
			UE_LOG(LogTemp, Error, TEXT("Laby UI font repair compilation failed: %s"), *PackageName);

			return;
		}

		UPackage* Package = Blueprint->GetOutermost();

		Package->MarkPackageDirty();

		FSavePackageArgs Args;

		Args.TopLevelFlags = RF_Public | RF_Standalone;

		const FString Filename =
		    FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());

		if (UPackage::SavePackage(Package, Blueprint, *Filename, Args))
		{
			UE_LOG(LogTemp, Display, TEXT("Laby UI fonts repaired: %s (%d labels)"), *PackageName, Repaired);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to save repaired Laby UI fonts: %s"), *PackageName);
		}
	}

	FAutoConsoleCommand RepairFontsCommand(
	    TEXT("laby.UI.RepairFonts"),
	    TEXT("Repair missing font assets in Laby Widget Blueprints, preserving layout and custom fonts."),
	    FConsoleCommandDelegate::CreateLambda(
	        []()
	        {
		        for (const TCHAR* Name :
		             {TEXT("WBP_HUD"), TEXT("WBP_MainMenu"), TEXT("WBP_PauseMenu"), TEXT("WBP_Settings")})
			        RepairFonts(FString(TEXT("/Game/UI/")) + Name);
	        }));

	void CreateAsset(
	    const TCHAR* Name, bool bHUD, bool bPause = false, bool bSettings = false, bool bExploration = false)
	{
		const FString PackageName = FString(TEXT("/Game/UI/")) + Name;

		// Never rebuild an existing asset: its layout belongs to the designer.
		if (FPackageName::DoesPackageExist(PackageName) || FindPackage(nullptr, *PackageName))
		{
			RepairFonts(PackageName);

			return;
		}

		// A runtime class introduced by Live Coding may not be in the on-disk import library yet.
		// Resolve the registered class instead of importing its generated construction symbol.
		UClass* ParentClass = bExploration
		                          ? LoadClass<UUserWidget>(nullptr, TEXT("/Script/laby.MazeExplorationMapWidget"))
		                          : (bHUD ? UMazeHUDWidget::StaticClass() : UMazeMenuWidget::StaticClass());

		if (!ParentClass)
		{
			UE_LOG(LogTemp, Error, TEXT("Laby UI parent class is unavailable: %s"), *PackageName);

			return;
		}

		UPackage* Package = CreatePackage(*PackageName);
		auto* Blueprint = CastChecked<UWidgetBlueprint>(
		    FKismetEditorUtilities::CreateBlueprint(ParentClass,
		                                            Package,
		                                            FName(Name),
		                                            BPTYPE_Normal,
		                                            UWidgetBlueprint::StaticClass(),
		                                            UWidgetBlueprintGeneratedClass::StaticClass()));

		if (bExploration)
		{
			auto* Tree = Blueprint->WidgetTree.Get();
			auto* Canvas = Make<UCanvasPanel>(Tree, TEXT("MapCanvas"));

			Tree->RootWidget = Canvas;
			Place(Canvas, Make<USizeBox>(Tree, TEXT("MinimapBounds")), {-24, -24}, {272, 272}, {1, 1}, {1, 1});
		}
		else if (bHUD)
			HUD(Blueprint->WidgetTree);
		else
			Menu(Blueprint->WidgetTree, bPause, bSettings);

		FKismetEditorUtilities::CompileBlueprint(Blueprint);

		if (Blueprint->Status == BS_Error)
		{
			UE_LOG(LogTemp, Error, TEXT("Laby UI blueprint compilation failed: %s"), *PackageName);

			return;
		}

		FAssetRegistryModule::AssetCreated(Blueprint);
		Package->MarkPackageDirty();

		FSavePackageArgs Args;

		Args.TopLevelFlags = RF_Public | RF_Standalone;

		const FString Filename =
		    FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());

		if (UPackage::SavePackage(Package, Blueprint, *Filename, Args))
		{
			UE_LOG(LogTemp, Display, TEXT("Laby editable UI created: %s"), *PackageName);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to save Laby UI: %s"), *PackageName);
		}
	}
}

class FMazeUIEditorModule : public IModuleInterface
{
	FTSTicker::FDelegateHandle CreationTicker;
	FMazeAutoLiveCoding AutoLiveCoding;

public:
	virtual void StartupModule() override
	{
		if (IsRunningCommandlet())
			return;

		AutoLiveCoding.Start();
		UToolsetRegistry::RegisterToolsetClass(UMazeDiagnosticsToolset::StaticClass());

		CreationTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		    [](float)
		    {
			    CreateAsset(TEXT("WBP_HUD"), true);
			    CreateAsset(TEXT("WBP_ExplorationMap"), false, false, false, true);
			    CreateAsset(TEXT("WBP_MainMenu"), false);
			    CreateAsset(TEXT("WBP_PauseMenu"), false, true);
			    CreateAsset(TEXT("WBP_Settings"), false, false, true);

			    return false;
		    }));
	}

	virtual void ShutdownModule() override
	{
		AutoLiveCoding.Stop();
		UToolsetRegistry::UnregisterToolsetClass(UMazeDiagnosticsToolset::StaticClass());
		FTSTicker::GetCoreTicker().RemoveTicker(CreationTicker);
	}
};

IMPLEMENT_MODULE(FMazeUIEditorModule, labyEditor)
