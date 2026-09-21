#include "World/MazeOnlineGameInstance.h"
#include "World/MazeLocationSubsystem.h"
#include "UI/MazeInterfaceStyle.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "MoviePlayer.h"
#include "RenderingThread.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void UMazeOnlineGameInstance::BeginLoadingScreen(const FWorldContext& LoadingWorldContext, const FString& MapName)
{
	if (LoadingWorldContext.OwningGameInstance != this || IsDedicatedServerInstance() ||
	    !FSlateApplication::IsInitialized())
		return;

	ClearLoadingScreen();
	LoadingScreen = MazeInterfaceStyle::MakeLoadingScreen();

	if (LoadingWorldContext.WorldType != EWorldType::PIE && IsMoviePlayerEnabled() && GetMoviePlayer()->IsInitialized())
	{
		FLoadingScreenAttributes Attributes;

		Attributes.WidgetLoadingScreen = LoadingScreen;
		Attributes.MinimumLoadingScreenDisplayTime = 0.5f;
		Attributes.bAutoCompleteWhenLoadingCompletes = true;
		Attributes.bMoviesAreSkippable = false;
		GetMoviePlayer()->SetupLoadingScreen(Attributes);
		// PreLoadMap follows PreLoadMapWithContext and starts MoviePlayer before the blocking load.
		bPlayingLoadingMovie = true;

		return;
	}

	// PIE does not use MoviePlayer. Present one opaque frame before synchronous world generation.
	if (auto* Viewport = GetGameViewportClient())
	{
		LoadingViewport = Viewport;
		Viewport->AddViewportWidgetContent(LoadingScreen.ToSharedRef(), MAX_int32);
		FSlateApplication::Get().Tick();
		FSlateApplication::Get().GetRenderer()->Sync();
		FlushRenderingCommands();
	}
}

void UMazeOnlineGameInstance::EndLoadingScreen(UWorld* LoadedWorld)
{
	if (LoadedWorld && LoadedWorld->GetGameInstance() != this)
		return;

	// The loading movie covers blocking LoadMap; viewport overlay covers subsequent asynchronous work.
	bPlayingLoadingMovie = false;

	if (LoadedWorld)
	{
		const auto* Location = LoadedWorld->GetSubsystem<UMazeLocationSubsystem>();

		UpdatePreparationScreen(
		    LoadedWorld, Location && !Location->IsReady(), Location ? Location->GetFailure() : FString());

		if (Location && !Location->IsReady() && Location->GetFailure().IsEmpty())
			UpdateLoadingStatus(LoadedWorld, Location->ReadPreparationStatus());
	}
	else
		ClearLoadingScreen();
}

void UMazeOnlineGameInstance::UpdateLoadingStatus(UWorld* World, const FMazePreparationStatus& Preparation)
{
	if (World && World->GetGameInstance() == this && LoadingScreen && !bShowingPreparationError)
		MazeInterfaceStyle::UpdateLoadingScreen(LoadingScreen.ToSharedRef(), Preparation);
}

void UMazeOnlineGameInstance::ClearLoadingScreen()
{
	if (bPlayingLoadingMovie && IsMoviePlayerEnabled())
		GetMoviePlayer()->StopMovie();

	bPlayingLoadingMovie = false;

	if (LoadingScreen && LoadingViewport.IsValid())
		LoadingViewport->RemoveViewportWidgetContent(LoadingScreen.ToSharedRef());

	LoadingScreen.Reset();
	LoadingViewport.Reset();
	bShowingPreparationError = false;
}

void UMazeOnlineGameInstance::UpdatePreparationScreen(UWorld* World, bool bPreparing, const FString& Failure)
{
	if (!World || World != GetWorld() || World->GetNetMode() == NM_DedicatedServer ||
	    !FSlateApplication::IsInitialized())
		return;

	if (!bPreparing)
	{
		if (LoadingViewport.IsValid())
			ClearLoadingScreen();

		return;
	}

	auto* Viewport = GetGameViewportClient();

	if (!Viewport)
		return;

	if (LoadingViewport.IsValid() && (Failure.IsEmpty() || bShowingPreparationError))
		return;

	if (LoadingViewport.IsValid() && LoadingScreen)
		LoadingViewport->RemoveViewportWidgetContent(LoadingScreen.ToSharedRef());

	if (Failure.IsEmpty())
	{
		// Preserve elapsed time, but use a separate widget while MoviePlayer releases its Slate tree.
		LoadingScreen = MazeInterfaceStyle::MakeLoadingScreen(bShowingPreparationError ? nullptr : LoadingScreen);

		bShowingPreparationError = false;
	}
	else
	{
		// Details are in the log; never expose internal resource paths as product UI.
		LoadingScreen =
		    SNew(SBorder)
		        .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		        .BorderBackgroundColor(FLinearColor::Black)
		        .HAlign(HAlign_Center)
		        .VAlign(
		            VAlign_Center)[SNew(STextBlock)
		                               .Text(NSLOCTEXT(
		                                   "Maze.Loading",
		                                   "PreparationFailed",
		                                   "Не удалось подготовить локацию. Проверьте файлы игры и повторите запуск."))
		                               .AutoWrapText(true)
		                               .ColorAndOpacity(FLinearColor::White)];
		bShowingPreparationError = true;
	}

	LoadingViewport = Viewport;
	Viewport->AddViewportWidgetContent(LoadingScreen.ToSharedRef(), MAX_int32);
}
