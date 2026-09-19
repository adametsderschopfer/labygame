#include "World/MazeOnlineGameInstance.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "MoviePlayer.h"
#include "RenderingThread.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SThrobber.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	TSharedRef<SWidget> MakeLoadingScreen()
	{
		// MoviePlayer renders while the game thread is blocked. Keep this tree free of UObjects/UMG bindings.
		return SNew(SBorder)
		    .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		    .BorderBackgroundColor(FLinearColor(0.015f, 0.025f, 0.04f, 1.f))
		    .HAlign(HAlign_Center)
		    .VAlign(
		        VAlign_Center)[SNew(SVerticalBox) +
		                       SVerticalBox::Slot()
		                           .AutoHeight()
		                           .HAlign(HAlign_Center)
		                           .Padding(0, 0, 0, 28)[SNew(STextBlock)
		                                                     .Text(NSLOCTEXT("Maze.Widgets", "Title", "L A B Y"))
		                                                     .Font(FCoreStyle::GetDefaultFontStyle("Bold", 44))
		                                                     .ColorAndOpacity(FLinearColor(0.35f, 0.85f, 0.9f))] +
		                       SVerticalBox::Slot()
		                           .AutoHeight()
		                           .HAlign(HAlign_Center)
		                           .Padding(0, 0, 0, 18)[SNew(STextBlock)
		                                                     .Text(NSLOCTEXT("Maze.Loading", "Loading", "Loading..."))
		                                                     .Font(FCoreStyle::GetDefaultFontStyle("Regular", 22))] +
		                       SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SNew(SThrobber)]];
	}
}

void UMazeOnlineGameInstance::BeginLoadingScreen(const FWorldContext& WorldContext, const FString& MapName)
{
	if (WorldContext.OwningGameInstance != this || IsDedicatedServerInstance() || !FSlateApplication::IsInitialized())
		return;

	ClearLoadingScreen();
	LoadingScreen = MakeLoadingScreen();

	if (WorldContext.WorldType != EWorldType::PIE && IsMoviePlayerEnabled() && GetMoviePlayer()->IsInitialized())
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
