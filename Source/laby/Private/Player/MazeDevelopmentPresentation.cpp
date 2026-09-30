#include "Player/MazePlayerController.h"
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
#include "MazeDevelopmentUI.h"
#include "Player/MazeCharacter.h"
#include "ECS/MazeDevelopmentSystem.h"
#include "ECS/MazeECSSubsystem.h"
#include "UI/MazeRoomText.h"
#include "World/MazeWorld.h"
#include "World/MazeLocationSubsystem.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

FText MazeDevelopmentText(const TCHAR* En, const TCHAR* Ru, const TCHAR* Es)
{
	const FString Language = FInternationalization::Get().GetCurrentCulture()->GetTwoLetterISOLanguageName();

	return FText::AsCultureInvariant(Language == TEXT("ru") ? Ru : Language == TEXT("es") ? Es : En);
}

void AMazePlayerController::EnsureDevelopmentPresentation()
{
	if (DevelopmentPresentation)
		return;

	auto* Viewport = GetWorld()->GetGameViewport();

	if (!Viewport)
		return;

	DevelopmentPresentation = MakeShared<FMazeDevelopmentPresentation>();

	const TWeakObjectPtr<AMazePlayerController> WeakThis(this);
	const TWeakPtr<FMazeDevelopmentPresentation> WeakView(DevelopmentPresentation);

	DevelopmentPresentation->Overlay =
	    SNew(SConstraintCanvas) +
	    SConstraintCanvas::Slot()
	        .Anchors(FAnchors(0, 0))
	        .Offset(FMargin(24, 24, 380, 0))
	        .Alignment(FVector2D(0, 0))
	        .AutoSize(true)[SNew(SBox).WidthOverride(380).Visibility_Lambda(
	            [WeakThis, WeakView]()
	            {
		            const auto* PC = WeakThis.Get();
		            const auto View = WeakView.Pin();

		            return PC && View && PC->ReadSession().bSessionStarted && !PC->IsMenuOpen() &&
		                           (View->bShowPosition || View->bShowResources)
		                       ? EVisibility::HitTestInvisible
		                       : EVisibility::Collapsed;
	            })[SNew(SBorder).Padding(12).BorderBackgroundColor(FLinearColor(0.04f, 0.05f, 0.04f, 0.9f))
	                   [SNew(SVerticalBox) +
	                    SVerticalBox::Slot().AutoHeight()[SNew(STextBlock)
	                                                          .AutoWrapText(true)
	                                                          .Text_Lambda(
	                                                              [WeakView]()
	                                                              {
		                                                              const auto V = WeakView.Pin();

		                                                              return V ? V->Position : FText::GetEmpty();
	                                                              })
	                                                          .Visibility_Lambda(
	                                                              [WeakView]()
	                                                              {
		                                                              const auto V = WeakView.Pin();

		                                                              return V && V->bShowPosition
		                                                                         ? EVisibility::Visible
		                                                                         : EVisibility::Collapsed;
	                                                              })] +
	                    SVerticalBox::Slot().AutoHeight().Padding(
	                        0, 8, 0, 0)[SNew(STextBlock)
	                                        .AutoWrapText(true)
	                                        .Text_Lambda(
	                                            [WeakView]()
	                                            {
		                                            const auto V = WeakView.Pin();

		                                            return V ? V->Resources : FText::GetEmpty();
	                                            })
	                                        .Visibility_Lambda(
	                                            [WeakView]()
	                                            {
		                                            const auto V = WeakView.Pin();

		                                            return V && V->bShowResources ? EVisibility::Visible
		                                                                          : EVisibility::Collapsed;
	                                            })]]]];
	Viewport->AddViewportWidgetContent(DevelopmentPresentation->Overlay.ToSharedRef(), 140);
	RefreshDevelopmentPresentation(0);
}

void AMazePlayerController::RefreshDevelopmentPresentation(float DeltaTime)
{
	if (!DevelopmentPresentation || !ECSSubsystem)
		return;

	auto& View = *DevelopmentPresentation;

	if (!IsDevelopmentMenuOpen() && !View.bShowPosition && !View.bShowResources)
		return;

	View.RefreshWait -= DeltaTime;

	if (View.RefreshWait > 0)
		return;

	View.RefreshWait = 0.25f;

	const auto Maze = ECSSubsystem->ReadMaze(ReadSession().Maze);

	View.Seed =
	    Maze.Data
	        ? FText::Format(MazeDevelopmentText(TEXT("Seed: {0}"), TEXT("Seed: {0}"), TEXT("Semilla: {0}")),
	                        FText::AsCultureInvariant(LexToString(Maze.Seed)))
	        : MazeDevelopmentText(TEXT("Seed unavailable"), TEXT("Seed недоступен"), TEXT("Semilla no disponible"));

	if (const auto* MazePawn = GetPawn(); MazePawn && Maze.Data)
	{
		const auto Place = FMazeDevelopmentSystem::Locate(Maze, MazePawn->GetActorLocation());
		const FText Area =
		    Place.RoomIndex != INDEX_NONE ? MazeRoomLabel(Maze.Data->Layout, Place.RoomIndex, false)
		    : Place.bInside
		        ? MazeDevelopmentText(TEXT("Corridor"), TEXT("Коридор"), TEXT("Pasillo"))
		        : MazeDevelopmentText(TEXT("Outside the maze"), TEXT("Вне лабиринта"), TEXT("Fuera del laberinto"));

		View.Position = FText::Format(
		    MazeDevelopmentText(TEXT("Position (cm): {0}, {1}, {2}\nCell (zero based): {3}, {4}\nRoom: {5}"),
		                        TEXT("Координаты (см): {0}, {1}, {2}\nКлетка (с нуля): {3}, {4}\nПомещение: {5}"),
		                        TEXT("Posición (cm): {0}, {1}, {2}\nCelda (desde cero): {3}, {4}\nSala: {5}")),
		    FText::AsNumber(FMath::RoundToInt(Place.Position.X)),
		    FText::AsNumber(FMath::RoundToInt(Place.Position.Y)),
		    FText::AsNumber(FMath::RoundToInt(Place.Position.Z)),
		    FText::AsNumber(Place.Cell.X),
		    FText::AsNumber(Place.Cell.Y),
		    Area);
	}
	else
		View.Position = MazeDevelopmentText(
		    TEXT("No player/maze"), TEXT("Игрок или лабиринт недоступен"), TEXT("Jugador o laberinto no disponible"));

	const auto* Location = GetWorld()->GetSubsystem<UMazeLocationSubsystem>();

	if (View.bWaitingTeleport && Location && Location->IsReady())
	{
		View.bWaitingTeleport = false;
		View.Status = MazeDevelopmentText(
		    TEXT("Teleport complete"), TEXT("Телепорт завершён"), TEXT("Teletransporte completado"));
	}

	int32 Resident = 0, Pending = 0;
	int64 Bytes = 0;

	for (TActorIterator<AMazeWorld> It(GetWorld()); It; ++It)
		if (It->GetMazeEntity() == ReadSession().Maze)
		{
			Resident += It->GetResidentChunkCount();
			Pending += It->HasPendingChunk() ? 1 : 0;
			Bytes += It->GetResidentGeometryBytes();
		}

	const auto Preparation = Location ? Location->ReadPreparationStatus() : FMazePreparationStatus();
	const FText Ready = Location && Location->IsReady()
	                        ? MazeDevelopmentText(TEXT("Ready"), TEXT("Готово"), TEXT("Listo"))
	                        : MazeDevelopmentText(TEXT("Preparing"), TEXT("Подготовка"), TEXT("Preparando"));

	View.Resources = FText::Format(
	    MazeDevelopmentText(TEXT("Resources: {0}\nChunks: {1} loaded / {2} building\nPreparation: {3} / {4}\nPending "
	                             "shader PSOs: {5}\nSource geometry: {6} MiB"),
	                        TEXT("Ресурсы: {0}\nЧанки: {1} загружено / {2} строится\nПодготовка: {3} / {4}\nОжидают "
	                             "PSO шейдеров: {5}\nИсходная геометрия: {6} МиБ"),
	                        TEXT("Recursos: {0}\nBloques: {1} cargados / {2} en construcción\nPreparación: {3} / "
	                             "{4}\nPSO de sombreadores pendientes: {5}\nGeometría de origen: {6} MiB")),
	    Ready,
	    FText::AsNumber(Resident),
	    FText::AsNumber(Pending),
	    FText::AsNumber(Preparation.Completed),
	    FText::AsNumber(Preparation.Total),
	    FText::AsNumber(Location ? Location->GetPendingPSOs() : 0),
	    FText::AsNumber(double(Bytes) / (1024 * 1024)));

	if (Location && !Location->GetFailure().IsEmpty())
		View.Resources = FText::Format(
		    MazeDevelopmentText(TEXT("{0}\nFailure: {1}"), TEXT("{0}\nОшибка: {1}"), TEXT("{0}\nError: {1}")),
		    View.Resources,
		    FText::AsCultureInvariant(Location->GetFailure()));
}

void AMazePlayerController::DestroyDevelopmentPresentation()
{
	if (!DevelopmentPresentation)
		return;

	auto* Viewport = GetWorld()->GetGameViewport();

	if (Viewport && DevelopmentPresentation->Overlay)
		Viewport->RemoveViewportWidgetContent(DevelopmentPresentation->Overlay.ToSharedRef());

	if (auto* OriginalViewport = DevelopmentPresentation->Viewport.Get();
	    OriginalViewport && OriginalViewport->GetWorld() == GetWorld() && DevelopmentPresentation->bSavedCollision)
	{
		OriginalViewport->EngineShowFlags.SetCollision(DevelopmentPresentation->bOriginalCollision);
		OriginalViewport->ToggleShowCollision();
		OriginalViewport->EngineShowFlags.SetVolumes(DevelopmentPresentation->bOriginalVolumes);
		OriginalViewport->ToggleShowVolumes();
	}

	DevelopmentPresentation.Reset();
}

#endif
