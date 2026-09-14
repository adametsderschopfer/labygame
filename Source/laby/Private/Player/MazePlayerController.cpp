#include "Player/MazePlayerController.h"
#include "Player/MazeCharacter.h"
#include "ECS/MazeECSSubsystem.h"
#include "UI/MazeWidgets.h"
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
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "HAL/PlatformApplicationMisc.h"

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
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AMazePlayerController::ToggleMenu).bExecuteWhenPaused =
	    true;
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	InputComponent->BindKey(EKeys::M, IE_Pressed, this, &AMazePlayerController::ToggleMinimap);
#endif
}

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
void AMazePlayerController::ToggleMinimap()
{
	if (ECSSubsystem)
		ECSSubsystem->ToggleMinimap();
}

#endif

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
	ECSSubsystem = nullptr;
	Super::EndPlay(Reason);
}

void AMazePlayerController::ToggleMenu()
{
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
	if (auto* Online = GetGameInstance<UMazeOnlineGameInstance>())
		Online->Host();
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

	bJoinScreen &= !Room.bActive;

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
	    10,
	    10,
	    10,
	    22)[SNew(STextBlock)
	            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 28))
	            .AutoWrapText(true)
	            .Text(bSettingsScreen ? NSLOCTEXT("Maze.Menu", "SettingsTitle", "SETTINGS")
	                  : bJoinScreen   ? (bLocalJoin ? NSLOCTEXT("Maze.Local", "JoinTitle", "JOIN LOCAL SERVER")
	                                                : NSLOCTEXT("Maze.Menu", "JoinTitle", "JOIN A ROOM"))
	                  : Room.bStarted ? NSLOCTEXT("Maze.Menu", "GameMenu", "GAME MENU")
	                  : Room.bActive  ? NSLOCTEXT("Maze.Menu", "Lobby", "WAITING ROOM")
	                                  : NSLOCTEXT("Maze.Menu", "Title", "LABY - MULTIPLAYER"))];

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
	else if (Room.bActive)
	{
		Content->AddSlot().AutoHeight().Padding(
		    8)[SNew(STextBlock)
		           .Font(FCoreStyle::GetDefaultFontStyle("Regular", 20))
		           .Text_Lambda(
		               [this]()
		               {
			               const auto Current = ECSSubsystem->ReadRoom();
			               TArray<FText> Lines;
			               Lines.Add(FText::Format(NSLOCTEXT("Maze.Menu", "PlayerCount", "Players: {Count} / 4\n"),
			                                       FFormatNamedArguments{{TEXT("Count"), Current.Members.Num()}}));

			               for (const auto& Member : Current.Members)
			               {
				               const FText Name = FText::AsCultureInvariant(Member.Name);
				               Lines.Add(Member.Id == Current.HostId
				                             ? FText::Format(NSLOCTEXT("Maze.Menu", "HostPlayer", "{Name}  [HOST]"),
				                                             FFormatNamedArguments{{TEXT("Name"), Name}})
				                             : Name);
			               }

			               return FText::Join(FText::AsCultureInvariant(TEXT("\n")), Lines);
		               })];

		if (HasAuthority() && !Room.bStarted)
		{
			const bool bLocal = Online && Online->IsLocalRoom();

			Label(FText::Format(bLocal ? NSLOCTEXT("Maze.Local", "Address", "Server address: {Code}")
			                           : NSLOCTEXT("Maze.Menu", "RoomCode", "Room code: {Code}"),
			                    FFormatNamedArguments{
			                        {TEXT("Code"),
			                         FText::AsCultureInvariant(
			                             Online ? (bLocal ? Online->LocalAddress() : Online->RoomCode) : FString())}}));

			if (bLocal)
				Label(NSLOCTEXT("Maze.Local",
				                "HostHint",
				                "On this PC, connect to 127.0.0.1 using the port above. On another PC, use this PC's "
				                "LAN IPv4 address."));

			Button(bLocal ? NSLOCTEXT("Maze.Local", "CopyAddress", "Copy address")
			              : NSLOCTEXT("Maze.Menu", "CopyCode", "Copy code"),
			       [Online, bLocal]()
			       {
				       if (!Online)
					       return;

				       FPlatformApplicationMisc::ClipboardCopy(*(bLocal ? Online->LocalAddress() : Online->RoomCode));
				       Online->Status = bLocal ? NSLOCTEXT("Maze.Local", "AddressCopied", "Address copied.")
				                               : NSLOCTEXT("Maze.Menu", "CodeCopied", "Code copied.");
			       });
			Button(NSLOCTEXT("Maze.Menu", "StartGame", "Start game"),
			       [this]()
			       {
				       ServerStartRoom();
			       });
		}
		else if (!Room.bStarted)
			Label(NSLOCTEXT("Maze.Menu", "Waiting", "Waiting for the host to start the game..."));

		if (Room.bStarted)
			Button(NSLOCTEXT("Maze.Menu", "Resume", "Resume"),
			       [this]()
			       {
				       CloseMenu();
			       });

		Button(NSLOCTEXT("Maze.Menu", "LeaveRoom", "Leave room"),
		       [Online]()
		       {
			       if (Online)
				       Online->Leave();
		       });
	}
	else if (bJoinScreen)
	{
		Label(bLocalJoin
		          ? NSLOCTEXT("Maze.Local",
		                      "JoinHint",
		                      "Use 127.0.0.1:7777 on the same PC, or the host LAN IPv4 address on another PC.")
		          : NSLOCTEXT("Maze.Menu", "JoinInstructions", "Enter the 10-character code shared by the host."));

		TSharedRef<SEditableTextBox> Code =
		    SNew(SEditableTextBox)
		        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 24))
		        .Padding(FMargin(16))
		        .IsEnabled_Lambda(
		            [Online]()
		            {
			            return !Online || !Online->bBusy;
		            })
		        .Text(bLocalJoin ? FText::AsCultureInvariant(TEXT("127.0.0.1:7777")) : FText::GetEmpty())
		        .HintText(bLocalJoin ? NSLOCTEXT("Maze.Local", "AddressHint", "Host IPv4:port")
		                             : NSLOCTEXT("Maze.Menu", "RoomCodeHint", "Room code"));

		Content->AddSlot().AutoHeight().Padding(8)[Code];
		Button(NSLOCTEXT("Maze.Menu", "JoinRoom", "Join room"),
		       [Online, Code, bLocalJoin]()
		       {
			       if (Online)
			       {
				       if (bLocalJoin)
					       Online->JoinLocal(Code->GetText().ToString());
				       else
					       Online->Join(Code->GetText().ToString());
			       }
		       });
		Button(NSLOCTEXT("Maze.Menu", "Back", "Back"),
		       [this]()
		       {
			       ShowNetworkMenu();
		       });
	}
	else
	{
		Button(NSLOCTEXT("Maze.Local", "Create", "Create local room"),
		       [Online]()
		       {
			       if (Online)
				       Online->HostLocal();
		       });
		Button(NSLOCTEXT("Maze.Local", "Join", "Join local room"),
		       [this, Online]()
		       {
			       if (Online)
				       Online->Status = FText::GetEmpty();

			       ShowNetworkMenu(true, false, true);
		       });
		Button(NSLOCTEXT("Maze.Menu", "CreateRoom", "Create room"),
		       [Online]()
		       {
			       if (Online)
				       Online->Host();
		       });
		Button(NSLOCTEXT("Maze.Menu", "JoinRoom", "Join room"),
		       [this, Online]()
		       {
			       if (Online)
				       Online->Status = FText::GetEmpty();

			       ShowNetworkMenu(true);
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

	Content->AddSlot().AutoHeight().Padding(8)[SNew(STextBlock)
	                                               .Font(FCoreStyle::GetDefaultFontStyle("Regular", 18))
	                                               .AutoWrapText(true)
	                                               .Text_Lambda(
	                                                   [Online]()
	                                                   {
		                                                   return Online ? Online->Status : FText::GetEmpty();
	                                                   })];

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
