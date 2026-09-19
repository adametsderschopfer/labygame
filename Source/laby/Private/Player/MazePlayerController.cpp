#include "Player/MazePlayerController.h"
#include "Player/MazeCharacter.h"
#include "ECS/MazeECSSubsystem.h"
#include "UI/MazeWidgets.h"
#include "UI/MazeExplorationMapWidget.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "World/MazeGameMode.h"
#include "World/MazeOnlineGameInstance.h"
#include "GameFramework/PlayerState.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SWeakWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	class SMazeMenuRoot : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SMazeMenuRoot)
		{
		}
		SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_EVENT(FSimpleDelegate, OnEscape)
		SLATE_END_ARGS()
		void Construct(const FArguments& Args)
		{
			Escape = Args._OnEscape;
			ChildSlot[Args._Content.Widget];
		}

		virtual bool SupportsKeyboardFocus() const override
		{
			return true;
		}

		virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override
		{
			if (Event.GetKey() == EKeys::Escape)
			{
				Escape.ExecuteIfBound();

				return FReply::Handled();
			}

			return SCompoundWidget::OnKeyDown(Geometry, Event);
		}

	private:
		FSimpleDelegate Escape;
	};
}

void UMazePreferences::SetSensitivity(float Value)
{
	MouseSensitivity = FMath::Clamp(Value, 0.1f, 3.f);
	SaveConfig(CPF_Config, *GGameUserSettingsIni);
}

void AMazePlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
		return;

	ECSSubsystem = GetWorld()->GetSubsystem<UMazeECSSubsystem>();
	check(ECSSubsystem);

	UClass* MapClass =
	    LoadClass<UMazeExplorationMapWidget>(nullptr, TEXT("/Game/UI/WBP_ExplorationMap.WBP_ExplorationMap_C"));

	ExplorationMap =
	    CreateWidget<UMazeExplorationMapWidget>(this, MapClass ? MapClass : UMazeExplorationMapWidget::StaticClass());

	if (ExplorationMap)
	{
		ExplorationMap->SetIsFocusable(true);
		ExplorationMap->ForceVolatile(true);
		ExplorationMap->SetVisibility(ESlateVisibility::HitTestInvisible);
		ExplorationMap->AddToViewport(20);
	}

	if (auto* Online = GetGameInstance<UMazeOnlineGameInstance>())
		Online->LocalRoomReady();

	if (const auto* State = GetWorld()->GetGameState<AMazeGameState>(); State && !HasAuthority())
		ECSSubsystem->ReceiveRoom(State->Room);

	if (ReadSession().bSessionStarted)
		CloseMenu();
	else
		ShowMenu();

	UE_LOG(LogTemp,
	       Display,
	       TEXT("Laby player ready: menu=%d session=%d sensitivity=%.2f"),
	       IsMenuOpen(),
	       ReadSession().bSessionStarted,
	       GetDefault<UMazePreferences>()->GetSensitivity());
}

FMazeSessionFragment AMazePlayerController::ReadSession() const
{
	return ECSSubsystem ? ECSSubsystem->ReadSession() : FMazeSessionFragment();
}

void AMazePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::M, IE_Pressed, this, &AMazePlayerController::ToggleMap);
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AMazePlayerController::ToggleMenu).bExecuteWhenPaused =
	    true;
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	InputComponent->BindKey(EKeys::F7, IE_Pressed, this, &AMazePlayerController::ToggleMinimap);
#endif
}

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
void AMazePlayerController::ToggleMinimap()
{
	if (ECSSubsystem)
		ECSSubsystem->ToggleMinimap();
}

#endif

void AMazePlayerController::ToggleMap()
{
	if (!ECSSubsystem || !ExplorationMap || IsMenuOpen() || !ReadSession().bSessionStarted)
		return;

	const auto* MapPlayer = Cast<AMazeCharacter>(GetPawn());

	if (!IsMapOpen() && (!MapPlayer || MapPlayer->GetVitals().Health <= 0))
		return;

	ECSSubsystem->SetMapOpen(!IsMapOpen());

	if (auto* MazePawn = Cast<AMazeCharacter>(GetPawn()))
		MazePawn->ClearLocalInput();

	FlushPressedKeys();
	ResetIgnoreMoveInput();
	ResetIgnoreLookInput();
	bShowMouseCursor = IsMapOpen();

	if (IsMapOpen())
	{
		SetIgnoreMoveInput(true);
		SetIgnoreLookInput(true);
		ExplorationMap->CenterOnPlayer();
		ExplorationMap->SetVisibility(ESlateVisibility::Visible);

		FInputModeUIOnly Mode;

		Mode.SetWidgetToFocus(ExplorationMap->TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
	}
	else
	{
		ExplorationMap->SetVisibility(ESlateVisibility::HitTestInvisible);
		SetInputMode(FInputModeGameOnly());
	}
}

void AMazePlayerController::RemoveMenuWidget()
{
	if (auto* MazePawn = Cast<AMazeCharacter>(GetPawn()))
		MazePawn->ClearLocalInput();

	if (NetworkMenu.IsValid() && GetWorld()->GetGameViewport())
		GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(NetworkMenu.ToSharedRef());

	NetworkMenu.Reset();

	if (ReadSession().bSettingsOpen)
	{
		auto* Preferences = GetMutableDefault<UMazePreferences>();

		Preferences->SetSensitivity(Preferences->GetSensitivity());
	}

	if (MenuWidget)
		MenuWidget->RemoveFromParent();

	MenuWidget = nullptr;
}

void AMazePlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	RemoveMenuWidget();

	if (ExplorationMap)
		ExplorationMap->RemoveFromParent();

	ExplorationMap = nullptr;
	ECSSubsystem = nullptr;
	Super::EndPlay(Reason);
}

void AMazePlayerController::ToggleMenu()
{
	if (IsMapOpen())
	{
		ToggleMap();

		return;
	}

	if (ReadSession().bSettingsOpen)
		ShowMenu();
	else if (IsMenuOpen() && ReadSession().bSessionStarted)
		CloseMenu();
	else
		ShowMenu();
}

void AMazePlayerController::CloseMenu()
{
	if (!ReadSession().bSessionStarted)
		return;

	RemoveMenuWidget();

	if (ECSSubsystem)
		ECSSubsystem->SetMenu(false);

	SetPause(false);
	ResetIgnoreMoveInput();
	ResetIgnoreLookInput();
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
	FlushPressedKeys();
}

void AMazePlayerController::StartNewGame()
{
	if (GetWorld()->GetNetMode() == NM_Standalone)
		UGameplayStatics::OpenLevel(this, TEXT("/Game/Maps/Maze"), true, TEXT("Room=1?QuickStart=1"));
}

void AMazePlayerController::ShowMenu(bool Settings)
{
	ShowNetworkMenu(false, Settings);
}

void AMazePlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (!IsLocalController() || !ECSSubsystem)
		return;

	if (IsMapOpen())
	{
		const auto* MapPlayer = Cast<AMazeCharacter>(GetPawn());

		if (!MapPlayer || MapPlayer->GetVitals().Health <= 0)
			ToggleMap();
	}

	const auto Room = ECSSubsystem->ReadRoom();

	if (Room.bActive != bDisplayedRoom || Room.bStarted != bDisplayedStarted)
	{
		bDisplayedRoom = Room.bActive;
		bDisplayedStarted = Room.bStarted;

		if (Room.bStarted)
			CloseMenu();
		else
			ShowNetworkMenu();
	}
}

void AMazePlayerController::ServerStartRoom_Implementation()
{
	if (auto* Mode = GetWorld()->GetAuthGameMode<AMazeGameMode>())
		Mode->StartRoom(this);
}

void AMazePlayerController::ShowNetworkMenu(bool bJoinScreen, bool bSettingsScreen, bool bLocalJoin)
{
	if (IsMapOpen())
		ToggleMap();

	auto* Viewport = GetWorld()->GetGameViewport();

	if (!Viewport || !ECSSubsystem)
		return;

	RemoveMenuWidget();
	ECSSubsystem->SetMenu(true, bSettingsScreen);
	ResetIgnoreMoveInput();
	ResetIgnoreLookInput();
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
	FlushPressedKeys();
	bShowMouseCursor = true;

	const auto Room = ECSSubsystem->ReadRoom();

	// Network room screens are temporarily hidden.
	bJoinScreen = false;

	auto* Online = GetGameInstance<UMazeOnlineGameInstance>();
	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);
	auto Label = [&Content](const FText& Value)
	{
		Content->AddSlot().AutoHeight().Padding(
		    10)[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 20)).Text(Value).AutoWrapText(true)];
	};
	auto Button = [&Content, Online](const FText& Value, TFunction<void()> Action)
	{
		Content->AddSlot().AutoHeight().Padding(
		    10)[SNew(SButton)
		            .HAlign(HAlign_Center)
		            .ContentPadding(FMargin(20))
		            .TextStyle(&FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
		            .IsEnabled_Lambda(
		                [Online]()
		                {
			                return !Online || !Online->bBusy;
		                })
		            .OnClicked_Lambda(
		                [Action]()
		                {
			                Action();

			                return FReply::Handled();
		                })[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 22)).Text(Value)]];
	};

	Content->AddSlot().AutoHeight().Padding(
	    10, 10, 10, 22)[SNew(STextBlock)
	                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 28))
	                        .AutoWrapText(true)
	                        .Text(bSettingsScreen ? NSLOCTEXT("Maze.Menu", "SettingsTitle", "SETTINGS")
	                              : Room.bStarted ? NSLOCTEXT("Maze.Menu", "GameMenu", "GAME MENU")
	                                              : NSLOCTEXT("Maze.Menu", "SimpleTitle", "LABY"))];

	if (bSettingsScreen)
	{
		Label(NSLOCTEXT("Maze.Widgets", "SensitivityLabel", "Mouse sensitivity"));
		Content->AddSlot().AutoHeight().Padding(
		    10)[SNew(STextBlock)
		            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 22))
		            .Text_Lambda(
		                []()
		                {
			                FNumberFormattingOptions Options;
			                Options.MinimumFractionalDigits = 2;
			                Options.MaximumFractionalDigits = 2;

			                return FText::Format(
			                    NSLOCTEXT("Maze.Settings", "SensitivityValue", "{Value} x"),
			                    FFormatNamedArguments{
			                        {TEXT("Value"),
			                         FText::AsNumber(GetDefault<UMazePreferences>()->GetSensitivity(), &Options)}});
		                })];
		Content->AddSlot().AutoHeight().Padding(
		    10,
		    18)[SNew(SBox).HeightOverride(36)[SNew(SSlider)
		                                          .MinValue(0.1f)
		                                          .MaxValue(3.f)
		                                          .StepSize(0.05f)
		                                          .Value_Lambda(
		                                              []()
		                                              {
			                                              return GetDefault<UMazePreferences>()->GetSensitivity();
		                                              })
		                                          .OnValueChanged_Lambda(
		                                              [](float Value)
		                                              {
			                                              GetMutableDefault<UMazePreferences>()->MouseSensitivity =
			                                                  FMath::Clamp(Value, 0.1f, 3.f);
		                                              })
		                                          .OnMouseCaptureEnd_Lambda(
		                                              []()
		                                              {
			                                              auto* Preferences = GetMutableDefault<UMazePreferences>();
			                                              Preferences->SetSensitivity(Preferences->GetSensitivity());
		                                              })
		                                          .OnControllerCaptureEnd_Lambda(
		                                              []()
		                                              {
			                                              auto* Preferences = GetMutableDefault<UMazePreferences>();
			                                              Preferences->SetSensitivity(Preferences->GetSensitivity());
		                                              })]];
		Label(NSLOCTEXT("Maze.Widgets", "SettingsHint", "0.10 - 3.00 - Saved automatically"));
		Button(NSLOCTEXT("Maze.Widgets", "ResetButtonLabel", "Reset to defaults"),
		       []()
		       {
			       GetMutableDefault<UMazePreferences>()->SetSensitivity(1.f);
		       });
		Button(NSLOCTEXT("Maze.Menu", "Back", "Back"),
		       [this]()
		       {
			       ShowNetworkMenu();
		       });
	}
	else if (Room.bStarted)
	{
		Button(NSLOCTEXT("Maze.Menu", "Resume", "Resume"),
		       [this]()
		       {
			       CloseMenu();
		       });
		Button(NSLOCTEXT("Maze.Menu", "ReturnToMenu", "Main menu"),
		       [Online]()
		       {
			       if (Online)
				       Online->Leave();
		       });
	}
	else
	{
		Button(NSLOCTEXT("Maze.Menu", "CreateAndStart", "Create room and start game"),
		       [this]()
		       {
			       StartNewGame();
		       });
	}

	if (!bJoinScreen && !bSettingsScreen)
		Button(NSLOCTEXT("Maze.Menu", "Settings", "Settings"),
		       [this]()
		       {
			       ShowMenu(true);
		       });

	if (!Room.bActive && !bJoinScreen && !bSettingsScreen)
		Button(NSLOCTEXT("Maze.Menu", "Quit", "Quit game"),
		       [this]()
		       {
			       ConsoleCommand(TEXT("quit"));
		       });

	TSharedRef<SWidget> Panel =
	    SNew(SBorder)
	        .BorderBackgroundColor(FLinearColor(0.015f, 0.025f, 0.04f, 1.f))
	        .HAlign(HAlign_Center)
	        .VAlign(VAlign_Center)[SNew(SScaleBox)
	                                   .Stretch(EStretch::ScaleToFit)
	                                   .StretchDirection(
	                                       EStretchDirection::DownOnly)[SNew(SBox).WidthOverride(640)[Content]]];

	TSharedRef<SWidget> Root = SNew(SMazeMenuRoot)
	                               .OnEscape(FSimpleDelegate::CreateLambda(
	                                   [this, bJoinScreen, bSettingsScreen, Online]()
	                                   {
		                                   if (bJoinScreen || bSettingsScreen)
		                                   {
			                                   if (!Online || !Online->bBusy)
				                                   ShowNetworkMenu();
		                                   }
		                                   else
			                                   ToggleMenu();
	                                   }))[Panel];

	NetworkMenu = Root;
	Viewport->AddViewportWidgetContent(Root, 100);

	FInputModeUIOnly Mode;

	Mode.SetWidgetToFocus(Root);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
}
