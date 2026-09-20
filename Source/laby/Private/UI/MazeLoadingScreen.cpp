#include "World/MazeOnlineGameInstance.h"
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

	// MoviePlayer owns its completion after LoadMap, including its minimum display time.
	if (LoadedWorld && bPlayingLoadingMovie)
	{
		bPlayingLoadingMovie = false;
		LoadingScreen.Reset();

		return;
	}

	ClearLoadingScreen();
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
}
