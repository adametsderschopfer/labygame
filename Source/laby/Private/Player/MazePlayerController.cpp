#include "Player/MazePlayerController.h"
#include "ECS/MazeECSSubsystem.h"
#include "UI/MazeWidgets.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"

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

	const auto* Mode = GetWorld()->GetAuthGameMode();

	ECSSubsystem->SetSessionStarted(Mode && UGameplayStatics::HasOption(Mode->OptionsString, TEXT("StartGame")));

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
	CloseMenu();
	UGameplayStatics::OpenLevel(this, TEXT("/Game/Maps/Maze"), true, TEXT("StartGame=1"));
}

void AMazePlayerController::ShowMenu(bool Settings)
{
	if (!GetWorld()->GetGameViewport())
		return;

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

	SetPause(true);
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
