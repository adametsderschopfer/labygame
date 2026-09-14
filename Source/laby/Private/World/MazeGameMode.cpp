#include "World/MazeGameMode.h"
#include "Player/MazePlayerController.h"
#include "Player/MazeCharacter.h"
#include "UI/MazeWidgets.h"
#include "World/MazeWorld.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "EngineUtils.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"

AMazeGameMode::AMazeGameMode()
{
	DefaultPawnClass = AMazeCharacter::StaticClass();
	HUDClass = AMazeHUD::StaticClass();
	PlayerControllerClass = AMazePlayerController::StaticClass();
}

void AMazeGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	auto* Maze = GetWorld()->SpawnActor<AMazeWorld>();

	Maze->InitializeMaze();
	GetWorld()->SpawnActor<APlayerStart>(Maze->StartLocation(), FRotator::ZeroRotator);

	auto* Sun = GetWorld()->SpawnActor<ADirectionalLight>(FVector(0, 0, 2000), FRotator(-55, -35, 0));

	Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable);
	Sun->GetLightComponent()->SetIntensity(5.f);
	CastChecked<UDirectionalLightComponent>(Sun->GetLightComponent())->SetAtmosphereSunLight(true);

	auto* Sky = GetWorld()->SpawnActor<ASkyLight>();

	Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
	Sky->GetLightComponent()->SetIntensity(1.2f);
	Sky->GetLightComponent()->SetRealTimeCaptureEnabled(true);

	auto* AtmosphereActor = GetWorld()->SpawnActor<AActor>();
	auto* Atmosphere = NewObject<USkyAtmosphereComponent>(AtmosphereActor);

	AtmosphereActor->SetRootComponent(Atmosphere);
	Atmosphere->RegisterComponent();
}

void AMazeHUD::BeginPlay()
{
	Super::BeginPlay();

	if (!PlayerOwner || !PlayerOwner->IsLocalController())
		return;

	UClass* WidgetClass = LoadClass<UMazeHUDWidget>(nullptr, TEXT("/Game/UI/WBP_HUD.WBP_HUD_C"));

	if (WidgetClass)
	{
		HUDWidget = CreateWidget<UMazeHUDWidget>(PlayerOwner, WidgetClass);

		if (HUDWidget)
			HUDWidget->AddToViewport();
	}
	else
		UE_LOG(LogTemp, Error, TEXT("Missing /Game/UI/WBP_HUD. Open the editor to create UI assets."));
}

void AMazeHUD::EndPlay(const EEndPlayReason::Type Reason)
{
	if (HUDWidget)
		HUDWidget->RemoveFromParent();

	HUDWidget = nullptr;
	Super::EndPlay(Reason);
}
