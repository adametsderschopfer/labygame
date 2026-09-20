#include "UI/MazeExplorationMapWidget.h"
#include "ECS/MazeECSSubsystem.h"
#include "Player/MazeCharacter.h"
#include "Player/MazePlayerController.h"
#include "MazeMapPaint.h"
#include "Maze/MazeLayout.h"
#include "UI/MazeInterfacePreferences.h"
#include "Input/Reply.h"

void UMazeExplorationMapWidget::CenterOnPlayer()
{
	const auto* ECS = GetWorld() ? GetWorld()->GetSubsystem<UMazeECSSubsystem>() : nullptr;
	const auto* Player = GetOwningPlayerPawn();

	if (!ECS || !Player)
		return;

	const auto Maze = ECS->ReadMaze(ECS->ReadSession().Maze);

	if (Maze.Cell <= 0)
		return;

	const FVector Local = (Player->GetActorLocation() - Maze.Origin) / Maze.Cell;

	Center = FVector2D(Local.X, Local.Y);
	ClampCenter();
}

void UMazeExplorationMapWidget::ClampCenter()
{
	const auto* ECS = GetWorld() ? GetWorld()->GetSubsystem<UMazeECSSubsystem>() : nullptr;

	if (!ECS)
		return;

	const auto Maze = ECS->ReadMaze(ECS->ReadSession().Maze);

	if (!Maze.Data)
		return;

	Center.X = FMath::Clamp(Center.X, 0.0, double(Maze.Data->Layout.Size));
	Center.Y = FMath::Clamp(Center.Y, 0.0, double(Maze.Data->Layout.Size));
}

int32 UMazeExplorationMapWidget::NativePaint(const FPaintArgs& Args,
                                             const FGeometry& Geometry,
                                             const FSlateRect& CullingRect,
                                             FSlateWindowElementList& Elements,
                                             int32 Layer,
                                             const FWidgetStyle& Style,
                                             bool bParentEnabled) const
{
	Layer = Super::NativePaint(Args, Geometry, CullingRect, Elements, Layer, Style, bParentEnabled);

	const FVector2D ViewSize = Geometry.GetLocalSize();
	FMazeMapPaintView View;

	View.bPreview = IsDesignTime();

	const auto* Controller = GetOwningPlayer<AMazePlayerController>();
	const auto* Player = Controller ? Cast<AMazeCharacter>(Controller->GetPawn()) : nullptr;
	const auto* ECS = GetWorld() ? GetWorld()->GetSubsystem<UMazeECSSubsystem>() : nullptr;

	// Designer previews are static: they neither create a maze nor touch gameplay entities.
	if (View.bPreview)
	{
		View.bFull = ViewSize.X >= 900;
		View.Player = View.Center = FVector2D(8, 8);
		View.Yaw = -65.f;
	}
	else
	{
		if (!ECS || !Player || Controller->IsMenuOpen() || Player->GetVitals().Health <= 0)
			return Layer;

		View.bFull = ECS->ReadSession().bMapOpen;
		View.bCompass = FMazeInterfacePreferences::Read().bShowCompass;
	}

	const float Side = FMath::Min(312.f, float(FMath::Min(ViewSize.X, ViewSize.Y) - 112.f));

	if (Side <= 64)
		return Layer;

	View.Size = View.bFull ? FVector2D(ViewSize.X - 52, ViewSize.Y - 52) : FVector2D(Side, Side);
	View.Origin = View.bFull ? (ViewSize - View.Size) * 0.5 : ViewSize - View.Size - FVector2D(36, 64);

	// Both panels retain editable layout bounds in the Widget Blueprint.
	if (!View.bPreview)
		if (const auto* Bounds = GetWidgetFromName(View.bFull ? TEXT("FullMapBounds") : TEXT("MinimapBounds")))
		{
			const auto& BoundsGeometry = Bounds->GetPaintSpaceGeometry();
			const FVector2D BoundOrigin =
			    Geometry.AbsoluteToLocal(BoundsGeometry.LocalToAbsolute(FVector2D::ZeroVector));
			const FVector2D BoundSize =
			    Geometry.AbsoluteToLocal(BoundsGeometry.LocalToAbsolute(BoundsGeometry.GetLocalSize())) - BoundOrigin;

			if (BoundSize.X > 64 && BoundSize.Y > 176)
			{
				View.Origin = BoundOrigin;
				View.Size = BoundSize;
			}
		}

	View.Step = View.bFull ? Zoom : 20.f;

	if (View.bPreview)
	{
		// A fixed designer illustration, never produced by gameplay generation or stored in ECS.
		FMazeLayout Illustration;

		Illustration.Size = 20;
		Illustration.Walls.Init(15, 400);
		Illustration.Holes.Init(0, 400);

		TArray<uint8> Seen;

		Seen.Init(0, 400);

		const FIntPoint Route[] = {{10, 10},
		                           {6, 10},
		                           {6, 6},
		                           {10, 6},
		                           {10, 3},
		                           {13, 3},
		                           {13, 8},
		                           {16, 8},
		                           {16, 13},
		                           {12, 13},
		                           {12, 16},
		                           {8, 16},
		                           {8, 13},
		                           {4, 13},
		                           {4, 10},
		                           {6, 10}};

		for (int32 I = 1; I < UE_ARRAY_COUNT(Route); ++I)
		{
			FIntPoint A = Route[I - 1];
			const FIntPoint End = Route[I];

			Seen[A.Y * 20 + A.X] = 1;

			while (A != End)
			{
				const FIntPoint D(FMath::Sign(End.X - A.X), FMath::Sign(End.Y - A.Y));
				const FIntPoint B = A + D;
				const int32 Direction = D.X > 0 ? 1 : D.X < 0 ? 3 : D.Y > 0 ? 2 : 0;

				Illustration.Walls[A.Y * 20 + A.X] &= ~(1 << Direction);
				Illustration.Walls[B.Y * 20 + B.X] &= ~(1 << ((Direction + 2) % 4));
				Seen[B.Y * 20 + B.X] = 1;
				A = B;
			}
		}

		View.Player = View.Center = FVector2D(10.5, 10.5);
		View.Start = FVector2D(8.5, 16.5);
		View.Layout = &Illustration;
		View.Seen = Seen;

		const FSlateRect Content = MazeMapContentRect(View);

		View.Step = View.bFull ? (Content.Right - Content.Left) / 20.f : 20.f;

		return PaintMazeMap(Geometry, Elements, Layer, Style.GetColorAndOpacityTint(), View);
	}

	const auto Session = ECS->ReadSession();
	const auto Maze = ECS->ReadMaze(Session.Maze);

	if (!Session.bSessionStarted || !Maze.Data || Maze.Cell <= 0)
		return Layer;

	const FVector Local = (Player->GetActorLocation() - Maze.Origin) / Maze.Cell;

	View.Player = FVector2D(Local.X, Local.Y);
	View.Center = View.bFull ? Center : View.Player;
	View.Start = FVector2D(Maze.Data->Start.X, Maze.Data->Start.Y) / Maze.Cell;
	View.Yaw = Controller->GetControlRotation().Yaw;
	View.Layout = &Maze.Data->Layout;

	// Borrow only for this synchronous paint; stale discovery never reveals a regenerated maze.
	if (const auto* Exploration = ECS->ReadExploration(Player->GetPlayerEntity());
	    Exploration && Exploration->Maze == Session.Maze && Exploration->Revision == Maze.Revision)
		View.Seen = Exploration->Seen;

	return PaintMazeMap(Geometry, Elements, Layer, Style.GetColorAndOpacityTint(), View);
}

FReply UMazeExplorationMapWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (auto* Controller = GetOwningPlayer<AMazePlayerController>(); Controller && Controller->IsMapOpen())
	{
		if (Event.GetKey() == EKeys::M || Event.GetKey() == EKeys::Escape)
		{
			if (!Event.IsRepeat())
				Controller->ToggleMap();

			return FReply::Handled().ReleaseMouseCapture();
		}

		if (Event.GetKey() == EKeys::Home)
		{
			CenterOnPlayer();

			return FReply::Handled();
		}
	}

	return Super::NativeOnKeyDown(Geometry, Event);
}

FReply UMazeExplorationMapWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (const auto* Controller = GetOwningPlayer<AMazePlayerController>();
	    Controller && Controller->IsMapOpen() && Event.GetEffectingButton() == EKeys::LeftMouseButton)
		return FReply::Handled().CaptureMouse(TakeWidget());

	return Super::NativeOnMouseButtonDown(Geometry, Event);
}

FReply UMazeExplorationMapWidget::NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton)
		return FReply::Handled().ReleaseMouseCapture();

	return Super::NativeOnMouseButtonUp(Geometry, Event);
}

FReply UMazeExplorationMapWidget::NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (HasMouseCapture() && Event.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		Center -= (Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()) -
		           Geometry.AbsoluteToLocal(Event.GetLastScreenSpacePosition())) /
		          Zoom;
		ClampCenter();

		return FReply::Handled();
	}

	return Super::NativeOnMouseMove(Geometry, Event);
}

FReply UMazeExplorationMapWidget::NativeOnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const auto* Controller = GetOwningPlayer<AMazePlayerController>();

	if (!Controller || !Controller->IsMapOpen())
		return Super::NativeOnMouseWheel(Geometry, Event);

	FMazeMapPaintView View;

	View.bFull = true;
	View.Origin = FVector2D(26, 26);
	View.Size = Geometry.GetLocalSize() - FVector2D(52, 52);

	if (const auto* Bounds = GetWidgetFromName(TEXT("FullMapBounds")))
	{
		const auto& BoundsGeometry = Bounds->GetPaintSpaceGeometry();

		View.Origin = Geometry.AbsoluteToLocal(BoundsGeometry.LocalToAbsolute(FVector2D::ZeroVector));
		View.Size =
		    Geometry.AbsoluteToLocal(BoundsGeometry.LocalToAbsolute(BoundsGeometry.GetLocalSize())) - View.Origin;
	}

	const FSlateRect Content = MazeMapContentRect(View);
	const FVector2D MapMiddle((Content.Left + Content.Right) * 0.5, (Content.Top + Content.Bottom) * 0.5);

	const FVector2D CursorOffset = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()) - MapMiddle;
	const FVector2D Anchor = Center + CursorOffset / Zoom;

	Zoom = FMath::Clamp(Zoom * FMath::Pow(1.2f, Event.GetWheelDelta()), 4.f, 80.f);
	Center = Anchor - CursorOffset / Zoom;
	ClampCenter();

	return FReply::Handled();
}
