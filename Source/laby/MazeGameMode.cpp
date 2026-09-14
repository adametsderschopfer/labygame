#include "MazeGameMode.h"
#include "MazePlayerController.h"
#include "MazeCharacter.h"
#include "MazeVitalsSystem.h"
#include "MazeWorld.h"
#include "Engine/Canvas.h"
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

void AMazeHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
		return;

	if (const auto* PC = Cast<AMazePlayerController>(PlayerOwner); PC && PC->IsMenuOpen())
		return;

	DrawRect(FLinearColor(0.015f, 0.025f, 0.04f, 0.8f), 20, 20, 470, 100);
	DrawText(TEXT("LABY / FIND ONE OF THE THREE EXITS"), FLinearColor::White, 36, 32, nullptr, 1.2f);
	DrawText(TEXT("WASD  Move    Mouse  Look    Space  Jump"), FLinearColor(0.8f, 0.85f, 0.9f), 36, 60);
	DrawText(TEXT("Shift  Run    R  New maze    Esc  Menu"), FLinearColor(0.8f, 0.85f, 0.9f), 36, 82);
	DrawRect(FLinearColor::White, Canvas->ClipX / 2 - 2, Canvas->ClipY / 2 - 2, 4, 4);

	if (const auto* Character = PlayerOwner ? Cast<AMazeCharacter>(PlayerOwner->GetPawn()) : nullptr)
	{
		const FMazeVitals Vitals = Character->GetVitals();
		const float Top = Canvas->ClipY - 190.f;

		DrawRect(FLinearColor(0.015f, 0.025f, 0.04f, 0.85f), 20, Top, 300, 116);

		auto Bar = [&](const TCHAR* Label, float Value, float Y, FLinearColor Color)
		{
			DrawText(FString::Printf(TEXT("%s  %d / 100"), Label, FMath::CeilToInt(Value)), FLinearColor::White, 36, Y);
			DrawRect(FLinearColor(0.12f, 0.15f, 0.18f), 36, Y + 22, 268, 12);
			DrawRect(Color, 36, Y + 22, 268 * FMath::Clamp(Value / FMazeVitals::Maximum, 0.f, 1.f), 12);
		};

		Bar(TEXT("HEALTH"), Vitals.Health, Top + 12, FLinearColor(0.9f, 0.22f, 0.25f));
		Bar(Vitals.bExhausted ? TEXT("STAMINA / RECOVERING") : TEXT("STAMINA"),
		    Vitals.Stamina,
		    Top + 62,
		    Vitals.bExhausted ? FLinearColor(1.f, 0.55f, 0.12f) : FLinearColor(0.2f, 0.85f, 0.65f));

		if (!FMazeVitalsSystem::IsAlive(Vitals))
		{
			DrawRect(
			    FLinearColor(0.08f, 0.01f, 0.02f, 0.94f), Canvas->ClipX / 2 - 230, Canvas->ClipY / 2 - 70, 460, 110);
			DrawText(TEXT("YOU DIED"),
			         FLinearColor(1.f, 0.3f, 0.3f),
			         Canvas->ClipX / 2 - 180,
			         Canvas->ClipY / 2 - 50,
			         nullptr,
			         2);
			DrawText(TEXT("Press R to start a new maze"),
			         FLinearColor::White,
			         Canvas->ClipX / 2 - 180,
			         Canvas->ClipY / 2 + 4);

			return;
		}
	}

	for (TActorIterator<AMazeWorld> It(GetWorld()); It; ++It)
	{
		const auto* PC = Cast<AMazePlayerController>(PlayerOwner);

		if (!PC || PC->IsMinimapVisible())
			DrawMinimap(**It);

		if (const auto Data = It->GetGeneratedData())
			DrawText(
			    FString::Printf(TEXT("SESSION %d | %d x %d | START A"), It->Seed, Data->Layout.Size, Data->Layout.Size),
			    FLinearColor(0.7f, 0.8f, 0.85f),
			    24,
			    Canvas->ClipY - 40);

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
		DrawText(TEXT("DEV: M  Toggle map"), FLinearColor(0.7f, 0.8f, 0.85f), 24, Canvas->ClipY - 62);
#endif
		break;
	}

	const auto* Player = PlayerOwner ? Cast<AMazeCharacter>(PlayerOwner->GetPawn()) : nullptr;
	const int32 ReachedExit = Player ? Player->GetReachedExit() : 0;

	if (ReachedExit)
	{
		DrawRect(FLinearColor(0.01f, 0.06f, 0.04f, 0.9f), Canvas->ClipX / 2 - 230, Canvas->ClipY / 2 - 70, 460, 110);
		DrawText(FString::Printf(TEXT("EXIT %d REACHED"), ReachedExit),
		         FLinearColor(0.3f, 1.f, 0.6f),
		         Canvas->ClipX / 2 - 180,
		         Canvas->ClipY / 2 - 50,
		         nullptr,
		         2);
		DrawText(TEXT("Press R to generate a new maze"),
		         FLinearColor::White,
		         Canvas->ClipX / 2 - 180,
		         Canvas->ClipY / 2 + 4);
	}
}

void AMazeHUD::DrawMinimap(const AMazeWorld& Maze)
{
	const auto Data = Maze.GetGeneratedData();

	if (!Data)
		return;

	const FMazeLayout& Layout = Data->Layout;

	if (Layout.Walls.Num() != Layout.Size * Layout.Size)
		return;

	const float Size = FMath::Min(300.f, FMath::Min(Canvas->ClipX * 0.30f, Canvas->ClipY * 0.42f));
	const float Left = Canvas->ClipX - Size - 32.f, Top = 54.f;
	const float Step = Size / Layout.Size;

	DrawRect(FLinearColor(0.01f, 0.02f, 0.03f, 0.94f), Left - 12, Top - 34, Size + 24, Size + 66);
	DrawText(TEXT("MAP"), FLinearColor::White, Left, Top - 26);

	const FLinearColor WallColor(0.65f, 0.71f, 0.76f, 1);

	for (int32 Y = 0; Y < Layout.Size; ++Y)
		for (int32 X = 0; X < Layout.Size; ++X)
		{
			uint8 W = Layout.Walls[Y * Layout.Size + X];
			float PX = Left + X * Step, PY = Top + Y * Step;

			if (W & 1)
				DrawLine(PX, PY, PX + Step, PY, WallColor);

			if (W & 8)
				DrawLine(PX, PY, PX, PY + Step, WallColor);

			if (X == Layout.Size - 1 && (W & 2))
				DrawLine(PX + Step, PY, PX + Step, PY + Step, WallColor);

			if (Y == Layout.Size - 1 && (W & 4))
				DrawLine(PX, PY + Step, PX + Step, PY + Step, WallColor);
		}

	auto Project = [&](const FVector& World)
	{
		FVector Local = World - Maze.GetActorLocation();

		return FVector2D(Left + FMath::Clamp(Local.X / Maze.GetCellSize() * Step, 0.f, Size),
		                 Top + FMath::Clamp(Local.Y / Maze.GetCellSize() * Step, 0.f, Size));
	};
	FVector2D Start = Project(Maze.StartLocation());

	DrawRect(FLinearColor(0.15f, 0.5f, 1), Start.X - 3, Start.Y - 3, 6, 6);

	for (int32 I = 0; I < Layout.Exits.Num(); ++I)
	{
		int32 C = Layout.Exits[I];
		float X = Left + (C % Layout.Size + 0.5f) * Step;
		float Y = Top + (C / Layout.Size + 0.5f) * Step;

		if (I == 0)
			Y = Top;

		if (I == 1)
			X = Left + Size;

		if (I == 2)
			Y = Top + Size;

		DrawRect(FLinearColor(0.2f, 1, 0.4f), X - 3, Y - 3, 6, 6);
	}

	if (PlayerOwner && PlayerOwner->GetPawn())
	{
		FVector2D P = Project(PlayerOwner->GetPawn()->GetActorLocation());
		float Angle = FMath::DegreesToRadians(PlayerOwner->GetControlRotation().Yaw);
		FVector2D Direction(FMath::Cos(Angle), FMath::Sin(Angle));
		FVector2D Side(-Direction.Y, Direction.X);
		FVector2D Tip = P + Direction * 8, A = P - Direction * 5 + Side * 4, B = P - Direction * 5 - Side * 4;
		const FLinearColor PlayerColor(1, 0.75f, 0.12f);

		DrawLine(Tip.X, Tip.Y, A.X, A.Y, PlayerColor, 2);
		DrawLine(Tip.X, Tip.Y, B.X, B.Y, PlayerColor, 2);
		DrawLine(A.X, A.Y, B.X, B.Y, PlayerColor, 2);
	}

	DrawText(TEXT("YOU"), FLinearColor(1, 0.75f, 0.12f), Left, Top + Size + 10);
	DrawText(TEXT("A"), FLinearColor(0.15f, 0.5f, 1), Left + Size * 0.4f, Top + Size + 10);
	DrawText(TEXT("EXITS"), FLinearColor(0.2f, 1, 0.4f), Left + Size * 0.65f, Top + Size + 10);
}
