#include "Player/MazePlayerController.h"
#include "UI/MazeUIAssets.h"
#include "Components/InputComponent.h"
#include "Player/MazeCharacter.h"
#include "Player/MazeKeyBindings.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Engine/AssetManager.h"
#include "ECS/MazeECSSubsystem.h"
#include "UI/MazeWidgets.h"
#include "UI/MazeInterfaceStyle.h"
#include "UI/MazeInterfacePreferences.h"
#include "UI/MazeExplorationMapWidget.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Sound/SoundWave.h"
#include "World/MazeGameMode.h"
#include "World/MazeLocationSettings.h"
#include "World/MazeOnlineGameInstance.h"
#include "GameFramework/PlayerState.h"
#include "Engine/GameViewportClient.h"

namespace
{
	const FName MenuAmbientTag(TEXT("MazeMenuAmbient"));
	constexpr float MenuAmbientVolume = 0.55f;

	float CurrentMenuAmbientVolume()
	{
		return MenuAmbientVolume * FMazeMenuAudioPreferences::Read().MusicVolume;
	}

	UAudioComponent* FindMenuAmbient(AActor& Owner)
	{
		TInlineComponentArray<UAudioComponent*> Components;

		Owner.GetComponents(Components);

		for (UAudioComponent* Component : Components)
			if (Component && Component->ComponentHasTag(MenuAmbientTag))
				return Component;

		return nullptr;
	}

	void StartMenuAmbient(AMazePlayerController& Owner)
	{
		if (Owner.GetNetMode() == NM_DedicatedServer)
			return;

		UAudioComponent* Audio = FindMenuAmbient(Owner);

		if (!Audio)
		{
			const TSoftObjectPtr<USoundWave> AmbientSound = GetDefault<UMazeLocationSettings>()->MenuAmbientSound();
			auto* Sound = AmbientSound.Get();

			if (!Sound)
			{
				const TWeakObjectPtr<AMazePlayerController> WeakOwner(&Owner);

				UAssetManager::GetStreamableManager().RequestAsyncLoad(
				    AmbientSound.ToSoftObjectPath(),
				    FStreamableDelegate::CreateLambda(
				        [WeakOwner, AmbientSound]()
				        {
					        if (!AmbientSound.Get())
					        {
						        UE_LOG(LogTemp, Error, TEXT("Laby menu ambience could not be loaded"));

						        return;
					        }

					        if (AMazePlayerController* Controller = WeakOwner.Get();
					            Controller && Controller->IsMenuOpen())
						        StartMenuAmbient(*Controller);
				        }));

				return;
			}

			Audio = NewObject<UAudioComponent>(&Owner, TEXT("MenuAmbientAudio"));
			Audio->ComponentTags.Add(MenuAmbientTag);
			Audio->bAutoActivate = false;
			Audio->bAutoDestroy = false;
			Audio->bStopWhenOwnerDestroyed = true;
			Audio->bAllowSpatialization = false;
			Audio->bIsUISound = true;
			Audio->SetSound(Sound);
			Audio->RegisterComponent();
		}

		if (Audio->IsPlaying())
			Audio->AdjustVolume(2.5f, CurrentMenuAmbientVolume());
		else
			Audio->FadeIn(2.5f, CurrentMenuAmbientVolume());
	}

	void StopMenuAmbient(AActor& Owner, bool bDestroy)
	{
		if (UAudioComponent* Audio = FindMenuAmbient(Owner))
		{
			if (bDestroy)
			{
				Audio->Stop();
				Audio->DestroyComponent();
			}
			else if (Audio->IsPlaying())
				Audio->FadeOut(1.2f, 0.f);
		}
	}
}

void AMazePlayerController::RefreshMenuAmbientVolume()
{
	if (UAudioComponent* Audio = FindMenuAmbient(*this); Audio && Audio->IsPlaying())
		Audio->AdjustVolume(0.12f, CurrentMenuAmbientVolume());
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

	UClass* MapClass = MazeUIAssets::Map().LoadSynchronous();

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
	InputComponent->BindKey(EKeys::Backslash, IE_Pressed, this, &AMazePlayerController::ToggleDevelopmentMenu);
#endif
}

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
void AMazePlayerController::ToggleDevelopmentCamera()
{
	if (IsMenuOpen() || IsMapOpen() || IsDevelopmentMenuOpen() || !ReadSession().bSessionStarted)
		return;

	if (auto* MazePawn = Cast<AMazeCharacter>(GetPawn()))
		MazePawn->ToggleDevelopmentCamera();
}

#endif

void AMazePlayerController::ToggleMap()
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST

	if (IsDevelopmentMenuOpen())
		CloseDevelopmentMenu();

#endif

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
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	CloseDevelopmentMenu();
	DestroyDevelopmentPresentation();
#endif
	RemoveMenuWidget();
	StopMenuAmbient(*this, true);

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
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST

	if (IsDevelopmentMenuOpen())
	{
		CloseDevelopmentMenu();

		return;
	}

#endif

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
	StopMenuAmbient(*this, false);

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

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST

	if (IsDevelopmentMenuOpen())
	{
		const auto* DevelopmentPawn = Cast<AMazeCharacter>(GetPawn());

		if (!DevelopmentPawn || DevelopmentPawn->GetVitals().Health <= 0 || !ReadSession().bSessionStarted)
			CloseDevelopmentMenu();
	}

	RefreshDevelopmentPresentation(DeltaTime);

#endif

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
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	CloseDevelopmentMenu();
#endif

	// Room browsing remains hidden; all displayed layouts now belong to the editable Widget Blueprints.
	if (!IsLocalController() || !ECSSubsystem || !GetWorld()->GetGameViewport())
		return;

	const auto Asset = MazeUIAssets::Menu(bSettingsScreen, ReadSession().bSessionStarted);
	UClass* Class = Asset.LoadSynchronous();
	UMazeMenuWidget* NextMenu = Class ? CreateWidget<UMazeMenuWidget>(this, Class) : nullptr;

	if (!NextMenu)
	{
		UE_LOG(LogTemp, Error, TEXT("Laby menu asset is unavailable: %s"), *Asset.ToString());

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
	StartMenuAmbient(*this);

	FInputModeUIOnly Mode;

	Mode.SetWidgetToFocus(MenuWidget->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
}
