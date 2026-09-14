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
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
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
	if (auto* Character = Cast<AMazeCharacter>(GetPawn()))
		Character->ClearLocalInput();

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
	if (!GetWorld()->GetGameViewport())
		return;

	if (!Settings)
	{
		ShowNetworkMenu();

		return;
	}

	const TCHAR* Path = Settings                        ? TEXT("/Game/UI/WBP_Settings.WBP_Settings_C")
	                    : ReadSession().bSessionStarted ? TEXT("/Game/UI/WBP_PauseMenu.WBP_PauseMenu_C")
	                                                    : TEXT("/Game/UI/WBP_MainMenu.WBP_MainMenu_C");
	UClass* WidgetClass = LoadClass<UMazeMenuWidget>(nullptr, Path);

	if (!WidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("Missing maze menu: %s. Open the editor to create UI assets."), Path);

		return;
	}

	auto* NewWidget = CreateWidget<UMazeMenuWidget>(this, WidgetClass);

	if (!NewWidget)
		return;

	RemoveMenuWidget();

	if (ECSSubsystem)
		ECSSubsystem->SetMenu(true, Settings);

	SetPause(false);
	ResetIgnoreMoveInput();
	ResetIgnoreLookInput();
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
	FlushPressedKeys();
	bShowMouseCursor = true;

	MenuWidget = NewWidget;
	MenuWidget->AddToViewport(100);

	FInputModeUIOnly Mode;

	Mode.SetWidgetToFocus(MenuWidget->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
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

void AMazePlayerController::ShowNetworkMenu()
{
	auto* Viewport = GetWorld()->GetGameViewport();

	if (!Viewport || !ECSSubsystem)
		return;

	RemoveMenuWidget();
	ECSSubsystem->SetMenu(true);
	ResetIgnoreMoveInput();
	ResetIgnoreLookInput();
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
	FlushPressedKeys();
	bShowMouseCursor = true;

	const auto Room = ECSSubsystem->ReadRoom();
	auto* Online = GetGameInstance<UMazeOnlineGameInstance>();
	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);
	auto Label = [&Content](const FString& Value)
	{
		Content->AddSlot().AutoHeight().Padding(8)[SNew(STextBlock).Text(FText::FromString(Value)).AutoWrapText(true)];
	};
	auto Button = [&Content](const FString& Value, TFunction<void()> Action)
	{
		Content->AddSlot().AutoHeight().Padding(8)[SNew(SButton)
		                                               .HAlign(HAlign_Center)
		                                               .ContentPadding(FMargin(14))
		                                               .Text(FText::FromString(Value))
		                                               .OnClicked_Lambda(
		                                                   [Action]()
		                                                   {
			                                                   Action();

			                                                   return FReply::Handled();
		                                                   })];
	};

	Label(Room.bStarted ? TEXT("ПАУЗА") : Room.bActive ? TEXT("КОМНАТА ОЖИДАНИЯ") : TEXT("LABY • СЕТЕВАЯ ИГРА"));

	if (Room.bActive)
	{
		Content->AddSlot().AutoHeight().Padding(
		    8)[SNew(STextBlock)
		           .Text_Lambda(
		               [this]()
		               {
			               const auto Current = ECSSubsystem->ReadRoom();
			               FString List = FString::Printf(TEXT("Игроки: %d / 4\n\n"), Current.Members.Num());

			               for (const auto& Member : Current.Members)
				               List += FString::Printf(TEXT("%s%s\n"),
				                                       *Member.Name,
				                                       Member.Id == Current.HostId ? TEXT("  [ХОСТ]") : TEXT(""));

			               return FText::FromString(List);
		               })];

		if (HasAuthority() && !Room.bStarted)
		{
			Label(TEXT("Код комнаты: ") + (Online ? Online->RoomCode : FString()));
			Button(TEXT("Скопировать код"),
			       [Online]()
			       {
				       if (Online)
				       {
					       FPlatformApplicationMisc::ClipboardCopy(*Online->RoomCode);
					       Online->Status = TEXT("Код скопирован");
				       }
			       });
			Button(TEXT("Начать игру"),
			       [this]()
			       {
				       ServerStartRoom();
			       });
		}
		else if (!Room.bStarted)
			Label(TEXT("Ожидаем, пока хост начнёт игру…"));

		if (Room.bStarted)
			Button(TEXT("Продолжить"),
			       [this]()
			       {
				       CloseMenu();
			       });

		Button(TEXT("Выйти из комнаты"),
		       [Online]()
		       {
			       if (Online)
				       Online->Leave();
		       });
	}
	else
	{
		Button(TEXT("Создать комнату"),
		       [Online]()
		       {
			       if (Online)
				       Online->Host();
		       });

		TSharedRef<SEditableTextBox> Code = SNew(SEditableTextBox).HintText(FText::FromString(TEXT("Код комнаты")));

		Content->AddSlot().AutoHeight().Padding(8)[Code];
		Button(TEXT("Присоединиться"),
		       [Online, Code]()
		       {
			       if (Online)
				       Online->Join(Code->GetText().ToString());
		       });
	}

	Button(TEXT("Настройки"),
	       [this]()
	       {
		       ShowMenu(true);
	       });

	if (!Room.bActive)
		Button(TEXT("Выйти из игры"),
		       [this]()
		       {
			       ConsoleCommand(TEXT("quit"));
		       });

	Content->AddSlot().AutoHeight().Padding(8)[SNew(STextBlock)
	                                               .AutoWrapText(true)
	                                               .Text_Lambda(
	                                                   [Online]()
	                                                   {
		                                                   return FText::FromString(Online ? Online->Status : TEXT(""));
	                                                   })];

	TSharedRef<SWidget> Panel = SNew(SBorder)
	                                .BorderBackgroundColor(FLinearColor(0.015f, 0.025f, 0.04f, 1.f))
	                                .HAlign(HAlign_Center)
	                                .VAlign(VAlign_Center)[SNew(SBox).WidthOverride(520)[Content]];

	TSharedRef<SWidget> Root =
	    SNew(SMazeMenuRoot).OnEscape(FSimpleDelegate::CreateUObject(this, &ThisClass::ToggleMenu))[Panel];

	NetworkMenu = Root;
	Viewport->AddViewportWidgetContent(Root, 100);

	FInputModeUIOnly Mode;

	Mode.SetWidgetToFocus(Root);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
}
