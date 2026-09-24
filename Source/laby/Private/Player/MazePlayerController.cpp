#include "Player/MazePlayerController.h"
#include "Components/InputComponent.h"
#include "Player/MazeCharacter.h"
#include "Player/MazeKeyBindings.h"
#include "Camera/PlayerCameraManager.h"
#include "ECS/MazeECSSubsystem.h"
#include "UI/MazeWidgets.h"
#include "UI/MazeInterfaceStyle.h"
#include "UI/MazeExplorationMapWidget.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "World/MazeGameMode.h"
#include "World/MazeOnlineGameInstance.h"
#include "GameFramework/PlayerState.h"
#include "Engine/GameViewportClient.h"

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

	// Keep the arms in view without letting the camera look vertically into the body.
	// CameraManager clamps ControlRotation itself, keeping view and aiming aligned.
	if (PlayerCameraManager)
	{
		PlayerCameraManager->ViewPitchMin = -55.f;
		PlayerCameraManager->ViewPitchMax = 65.f;
	}

	if (auto* Viewport = GetWorld()->GetGameViewport();
	    Viewport && GetLocalPlayer() == GetGameInstance()->GetFirstGamePlayer())
	{
		const TSharedRef<SWidget> Cursor = MazeInterfaceStyle::MakeCursor();

		Viewport->SetSoftwareCursorWidget(EMouseCursor::Default, Cursor);
		Viewport->SetSoftwareCursorWidget(EMouseCursor::Hand, Cursor);
	}

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

	if (IsLocalController())
		MazeKeyBindings::EnsureDefaults();

	InputComponent->BindAction(TEXT("Map"), IE_Pressed, this, &AMazePlayerController::ToggleMap);
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AMazePlayerController::ToggleMenu).bExecuteWhenPaused =
	    true;
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	InputComponent->BindKey(EKeys::F6, IE_Pressed, this, &AMazePlayerController::ToggleDevelopmentCamera);
#endif
}

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
void AMazePlayerController::ToggleDevelopmentCamera()
{
	if (IsMenuOpen() || IsMapOpen() || !ReadSession().bSessionStarted)
		return;

	if (auto* MazePawn = Cast<AMazeCharacter>(GetPawn()))
		MazePawn->ToggleDevelopmentCamera();
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

	if (IsLocalController() && GetLocalPlayer() == GetGameInstance()->GetFirstGamePlayer())
		if (auto* Viewport = GetWorld()->GetGameViewport())
		{
			Viewport->SetSoftwareCursorWidget(EMouseCursor::Default, TSharedPtr<SWidget>());
			Viewport->SetSoftwareCursorWidget(EMouseCursor::Hand, TSharedPtr<SWidget>());
		}

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
	// Room browsing remains hidden; all displayed layouts now belong to the editable Widget Blueprints.
	if (!IsLocalController() || !ECSSubsystem || !GetWorld()->GetGameViewport())
		return;

	const TCHAR* Path = bSettingsScreen                 ? TEXT("/Game/UI/WBP_Settings.WBP_Settings_C")
	                    : ReadSession().bSessionStarted ? TEXT("/Game/UI/WBP_PauseMenu.WBP_PauseMenu_C")
	                                                    : TEXT("/Game/UI/WBP_MainMenu.WBP_MainMenu_C");
	UClass* Class = LoadClass<UMazeMenuWidget>(nullptr, Path);
	UMazeMenuWidget* NextMenu = Class ? CreateWidget<UMazeMenuWidget>(this, Class) : nullptr;

	if (!NextMenu)
	{
		UE_LOG(LogTemp, Error, TEXT("Laby menu asset is unavailable: %s"), Path);

		return;
	}

	if (IsMapOpen())
		ToggleMap();

	RemoveMenuWidget();
	MenuWidget = NextMenu;
	ECSSubsystem->SetMenu(true, bSettingsScreen);
	ResetIgnoreMoveInput();
	ResetIgnoreLookInput();
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
	FlushPressedKeys();
	bShowMouseCursor = true;
	MenuWidget->AddToPlayerScreen(100);

	FInputModeUIOnly Mode;

	Mode.SetWidgetToFocus(MenuWidget->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
}
