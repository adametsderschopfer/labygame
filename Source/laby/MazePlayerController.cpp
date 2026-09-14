#include "MazePlayerController.h"
#include "MazeECSSubsystem.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

class SMazeMenuRoot : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMazeMenuRoot) {} SLATE_DEFAULT_SLOT(FArguments, Content) SLATE_END_ARGS()
	TWeakObjectPtr<AMazePlayerController> Owner;
	void Construct(const FArguments& Args) { ChildSlot[Args._Content.Widget]; }
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override
	{
		if (Event.GetKey() == EKeys::Escape && Owner.IsValid()) { Owner->ToggleMenu(); return FReply::Handled(); }
		return SCompoundWidget::OnKeyDown(Geometry, Event);
	}
};

void UMazePreferences::SetSensitivity(float Value)
{
	MouseSensitivity = FMath::Clamp(Value, 0.1f, 3.f);
	SaveConfig(CPF_Config, *GGameUserSettingsIni);
}

void AMazePlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController()) return;
	ECSSubsystem = GetWorld()->GetSubsystem<UMazeECSSubsystem>();
	check(ECSSubsystem);
	const auto* Mode = GetWorld()->GetAuthGameMode();
	ECSSubsystem->SetSessionStarted(Mode && UGameplayStatics::HasOption(Mode->OptionsString, TEXT("StartGame")));
	if (ReadSession().bSessionStarted) CloseMenu(); else ShowMenu();
	UE_LOG(LogTemp, Display, TEXT("Laby player ready: menu=%d session=%d sensitivity=%.2f"), IsMenuOpen(), ReadSession().bSessionStarted, GetDefault<UMazePreferences>()->GetSensitivity());
}

FMazeSessionFragment AMazePlayerController::ReadSession() const
{
	return ECSSubsystem ? ECSSubsystem->ReadSession() : FMazeSessionFragment();
}

void AMazePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AMazePlayerController::ToggleMenu).bExecuteWhenPaused = true;
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	InputComponent->BindKey(EKeys::M, IE_Pressed, this, &AMazePlayerController::ToggleMinimap);
#endif
}

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
void AMazePlayerController::ToggleMinimap()
{
	if (ECSSubsystem) ECSSubsystem->ToggleMinimap();
}
#endif

void AMazePlayerController::RemoveMenuWidget()
{
	if (ReadSession().bSettingsOpen)
	{
		auto* Preferences = GetMutableDefault<UMazePreferences>();
		Preferences->SetSensitivity(Preferences->GetSensitivity());
	}
	if (MenuWidget.IsValid() && GetWorld() && GetWorld()->GetGameViewport())
		GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(MenuWidget.ToSharedRef());
	MenuWidget.Reset();
}

void AMazePlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	RemoveMenuWidget();
	ECSSubsystem = nullptr;
	Super::EndPlay(Reason);
}

void AMazePlayerController::ToggleMenu()
{
	if (ReadSession().bSettingsOpen) ShowMenu();
	else if (IsMenuOpen() && ReadSession().bSessionStarted) CloseMenu();
	else ShowMenu();
}

void AMazePlayerController::CloseMenu()
{
	RemoveMenuWidget();
	if (ECSSubsystem) ECSSubsystem->SetMenu(false);
	SetPause(false);
	ResetIgnoreMoveInput();
	ResetIgnoreLookInput();
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
	FlushPressedKeys();
}

void AMazePlayerController::StartNewGame()
{
	CloseMenu();
	UGameplayStatics::OpenLevel(this, TEXT("/Game/Maps/Maze"), true, TEXT("StartGame=1"));
}

void AMazePlayerController::ShowMenu(bool Settings)
{
	if (!GetWorld()->GetGameViewport()) return;
	RemoveMenuWidget();
	if (ECSSubsystem) ECSSubsystem->SetMenu(true, Settings);
	SetPause(true);
	ResetIgnoreMoveInput(); ResetIgnoreLookInput();
	SetIgnoreMoveInput(true); SetIgnoreLookInput(true);
	FlushPressedKeys();
	bShowMouseCursor = true;
	TSharedRef<SVerticalBox> Items = SNew(SVerticalBox);
	auto Text = [&Items](const FString& Value, int32 Size, FLinearColor Color)
	{
		Items->AddSlot().AutoHeight().Padding(0, 0, 0, 18)
		[SNew(STextBlock).Text(FText::FromString(Value)).Font(FCoreStyle::GetDefaultFontStyle("Regular", Size)).ColorAndOpacity(Color)];
	};
	auto Button = [&Items](const FString& Label, TFunction<void()> Action)
	{
		Items->AddSlot().AutoHeight().Padding(0, 5)
		[SNew(SButton).ContentPadding(FMargin(20, 15)).OnClicked_Lambda([Action]() { Action(); return FReply::Handled(); })
		[SNew(STextBlock).Text(FText::FromString(Label)).Font(FCoreStyle::GetDefaultFontStyle("Regular", 20))]];
	};
	Text(TEXT("L A B Y"), 44, FLinearColor(0.75f, 0.9f, 1));
	if (Settings)
	{
		Text(TEXT("Настройки / Управление"), 24, FLinearColor::White);
		Text(TEXT("Чувствительность мыши"), 18, FLinearColor(0.8f, 0.85f, 0.9f));
		Items->AddSlot().AutoHeight().Padding(0, 0, 0, 16)
		[SNew(STextBlock).Text_Lambda([]() { return FText::FromString(FString::Printf(TEXT("%.2f ×"), GetDefault<UMazePreferences>()->GetSensitivity())); })
		.Font(FCoreStyle::GetDefaultFontStyle("Regular", 22))];
		Items->AddSlot().AutoHeight().Padding(0, 8, 0, 24)
		[SNew(SSlider).MinValue(0.1f).MaxValue(3.f).StepSize(0.05f)
		.Value_Lambda([]() { return GetDefault<UMazePreferences>()->GetSensitivity(); })
		.OnValueChanged_Lambda([](float Value) { GetMutableDefault<UMazePreferences>()->MouseSensitivity = FMath::Clamp(Value, 0.1f, 3.f); })
		.OnMouseCaptureEnd_Lambda([]() { auto* P = GetMutableDefault<UMazePreferences>(); P->SetSensitivity(P->GetSensitivity()); })
		.OnControllerCaptureEnd_Lambda([]() { auto* P = GetMutableDefault<UMazePreferences>(); P->SetSensitivity(P->GetSensitivity()); })];
		Text(TEXT("0.10 — 3.00  •  Сохраняется автоматически"), 14, FLinearColor(0.65f, 0.75f, 0.8f));
		Button(TEXT("По умолчанию"), []() { GetMutableDefault<UMazePreferences>()->SetSensitivity(1.f); });
		Button(TEXT("Назад"), [this]() { ShowMenu(); });
	}
	else
	{
		Text(ReadSession().bSessionStarted ? TEXT("Игра приостановлена") : TEXT("Один лабиринт. Три выхода."), 18, FLinearColor(0.7f, 0.8f, 0.85f));
		if (ReadSession().bSessionStarted) Button(TEXT("Продолжить"), [this]() { CloseMenu(); });
		Button(TEXT("Новая игра"), [this]() { StartNewGame(); });
		Button(TEXT("Настройки"), [this]() { ShowMenu(true); });
		Button(TEXT("Выйти на рабочий стол"), [this]() { UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false); });
	}
	TSharedRef<SMazeMenuRoot> Root = SNew(SMazeMenuRoot)
	[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.015f, 0.025f, 0.04f, 1))
	.HAlign(HAlign_Center).VAlign(VAlign_Center)
	[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
	[SNew(SBox).WidthOverride(460).Padding(30)[Items]]]];
	Root->Owner = this;
	MenuWidget = Root;
	GetWorld()->GetGameViewport()->AddViewportWidgetContent(Root, 100);
	FInputModeGameAndUI Mode;
	Mode.SetWidgetToFocus(Root); Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
}
