#include "UI/MazeExplorationMapWidget.h"
#include "ECS/MazeECSSubsystem.h"
#include "Player/MazeCharacter.h"
#include "Player/MazePlayerController.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Input/Reply.h"

void UMazeExplorationMapWidget::CenterOnPlayer()
{
	const auto* ECS = GetWorld()->GetSubsystem<UMazeECSSubsystem>();
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
	const auto* ECS = GetWorld()->GetSubsystem<UMazeECSSubsystem>();

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

	const auto* Controller = GetOwningPlayer<AMazePlayerController>();
	const auto* Player = Controller ? Cast<AMazeCharacter>(Controller->GetPawn()) : nullptr;
	const auto* ECS = GetWorld() ? GetWorld()->GetSubsystem<UMazeECSSubsystem>() : nullptr;

	if (!ECS || !Player || Controller->IsMenuOpen() || Player->GetVitals().Health <= 0)
		return Layer;

	const auto Session = ECS->ReadSession();
	const auto Maze = ECS->ReadMaze(Session.Maze);
	const auto* Exploration = ECS->ReadExploration(Player->GetPlayerEntity());

	if (!Session.bSessionStarted || !Maze.Data || Maze.Cell <= 0)
		return Layer;

	const auto& Layout = Maze.Data->Layout;
	const FVector Local = (Player->GetActorLocation() - Maze.Origin) / Maze.Cell;
	const FVector2D PlayerPosition(Local.X, Local.Y);
	const FVector2D ViewSize = Geometry.GetLocalSize();
	const bool bFull = Session.bMapOpen;
	const float Side = FMath::Min(272.f, float(FMath::Min(ViewSize.X, ViewSize.Y) - 48.f));

	if (Side <= 0)
		return Layer;

	FVector2D Size = bFull ? ViewSize : FVector2D(Side, Side);
	FVector2D Origin = bFull ? FVector2D::ZeroVector : ViewSize - Size - FVector2D(24, 24);

	// The Blueprint owns the minimap slot; native fallback supports missing assets.
	if (!bFull)
		if (const auto* Bounds = GetWidgetFromName(TEXT("MinimapBounds")))
		{
			const auto& BoundsGeometry = Bounds->GetCachedGeometry();

			if (BoundsGeometry.GetLocalSize().X > 0 && BoundsGeometry.GetLocalSize().Y > 0)
			{
				Origin = Geometry.AbsoluteToLocal(BoundsGeometry.LocalToAbsolute(FVector2D::ZeroVector));
				Size = Geometry.AbsoluteToLocal(BoundsGeometry.LocalToAbsolute(BoundsGeometry.GetLocalSize())) - Origin;
			}
		}

	const FVector2D MapCenter = bFull ? Center : PlayerPosition;
	const float Step = bFull ? Zoom : 20.f;
	const FVector2D Offset = Origin + Size * 0.5 - MapCenter * Step;
	auto Box = [&](FVector2D P, FVector2D Extent, FLinearColor Color)
	{
		FSlateDrawElement::MakeBox(Elements,
		                           Layer,
		                           Geometry.ToPaintGeometry(Extent, FSlateLayoutTransform(P)),
		                           FCoreStyle::Get().GetBrush("WhiteBrush"),
		                           ESlateDrawEffect::None,
		                           Color);
	};
	auto Line = [&](FVector2D A, FVector2D B, FLinearColor Color, float Width = 1.5f)
	{
		FSlateDrawElement::MakeLines(Elements,
		                             Layer,
		                             Geometry.ToPaintGeometry(),
		                             TArray<FVector2D>{A, B},
		                             ESlateDrawEffect::None,
		                             Color,
		                             true,
		                             Width);
	};
	auto Seen = [&](int32 Index)
	{
		return Exploration && Exploration->Seen.IsValidIndex(Index) && Exploration->Seen[Index] != 0;
	};

	++Layer;
	Box(Origin, Size, FLinearColor::Black);

	const FGeometry Clip = Geometry.MakeChild(Size, FSlateLayoutTransform(Origin));

	Elements.PushClip(FSlateClippingZone(Clip));
	++Layer;

	const int32 MinX = FMath::Clamp(FMath::FloorToInt((Origin.X - Offset.X) / Step), 0, Layout.Size - 1);
	const int32 MinY = FMath::Clamp(FMath::FloorToInt((Origin.Y - Offset.Y) / Step), 0, Layout.Size - 1);
	const int32 MaxX = FMath::Clamp(FMath::CeilToInt((Origin.X + Size.X - Offset.X) / Step), 0, Layout.Size - 1);
	const int32 MaxY = FMath::Clamp(FMath::CeilToInt((Origin.Y + Size.Y - Offset.Y) / Step), 0, Layout.Size - 1);

	for (int32 Y = MinY; Y <= MaxY; ++Y)
		for (int32 X = MinX; X <= MaxX; ++X)
		{
			const int32 Index = Y * Layout.Size + X;

			if (!Seen(Index))
				continue;

			const FVector2D P = Offset + FVector2D(X, Y) * Step;
			Box(P, FVector2D(Step, Step), FLinearColor(0.055f, 0.095f, 0.12f));
		}

	++Layer;

	for (int32 Y = MinY; Y <= MaxY; ++Y)
		for (int32 X = MinX; X <= MaxX; ++X)
		{
			const int32 Index = Y * Layout.Size + X;

			if (!Seen(Index))
				continue;

			const FVector2D P = Offset + FVector2D(X, Y) * Step;
			const uint8 Walls = Layout.Walls[Index];
			const FLinearColor Wall(0.5f, 0.65f, 0.72f);

			if (Walls & 1)
				Line(P, P + FVector2D(Step, 0), Wall);

			if (Walls & 2)
				Line(P + FVector2D(Step, 0), P + FVector2D(Step, Step), Wall);

			if (Walls & 4)
				Line(P + FVector2D(0, Step), P + FVector2D(Step, Step), Wall);

			if (Walls & 8)
				Line(P, P + FVector2D(0, Step), Wall);

			if (!Layout.HasFloor(Index))
			{
				Line(P + FVector2D(Step * 0.25, Step * 0.25),
				     P + FVector2D(Step * 0.75, Step * 0.75),
				     FLinearColor(1, 0.3f, 0.1f));
				Line(P + FVector2D(Step * 0.75, Step * 0.25),
				     P + FVector2D(Step * 0.25, Step * 0.75),
				     FLinearColor(1, 0.3f, 0.1f));
			}
		}

	++Layer;

	for (int32 Exit : Layout.Exits)
		if (Seen(Exit))
			Box(Offset + FVector2D(Exit % Layout.Size + 0.5, Exit / Layout.Size + 0.5) * Step - FVector2D(3, 3),
			    FVector2D(6, 6),
			    FLinearColor(0.2f, 1, 0.4f));

	const FVector2D Start(Maze.Data->Start.X / Maze.Cell, Maze.Data->Start.Y / Maze.Cell);
	const int32 StartIndex = FMath::FloorToInt(Start.Y) * Layout.Size + FMath::FloorToInt(Start.X);

	if (Seen(StartIndex))
		Box(Offset + Start * Step - FVector2D(3, 3), FVector2D(6, 6), FLinearColor(0.15f, 0.5f, 1));

	++Layer;

	const FVector2D P = Offset + PlayerPosition * Step;
	const float Angle = FMath::DegreesToRadians(Controller->GetControlRotation().Yaw);
	const FVector2D Direction(FMath::Cos(Angle), FMath::Sin(Angle)), Right(-Direction.Y, Direction.X);
	const FVector2D Tip = P + Direction * 8, A = P - Direction * 5 + Right * 4, B = P - Direction * 5 - Right * 4;

	Line(Tip, A, FLinearColor(1, 0.75f, 0.12f), 2);
	Line(Tip, B, FLinearColor(1, 0.75f, 0.12f), 2);
	Line(A, B, FLinearColor(1, 0.75f, 0.12f), 2);
	++Layer;
	Box(Origin, FVector2D(Size.X, 30), FLinearColor(0.015f, 0.025f, 0.035f));
	++Layer;
	FSlateDrawElement::MakeText(
	    Elements,
	    Layer,
	    Geometry.ToPaintGeometry(FVector2D(Size.X - 16, 24), FSlateLayoutTransform(Origin + FVector2D(8, 5))),
	    bFull ? NSLOCTEXT(
	                "Maze.Map", "Controls", "MAP  |  Drag to pan  |  Wheel to zoom  |  Home: player  |  M / Esc: close")
	          : NSLOCTEXT("Maze.Map", "Minimap", "MAP  [M]"),
	    FCoreStyle::GetDefaultFontStyle("Regular", bFull ? 16 : 13),
	    ESlateDrawEffect::None,
	    FLinearColor(0.8f, 0.9f, 0.95f));
	Elements.PopClip();

	return Layer;
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
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton)
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
	const FVector2D CursorOffset =
	    Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()) - Geometry.GetLocalSize() * 0.5;
	const FVector2D Anchor = Center + CursorOffset / Zoom;

	Zoom = FMath::Clamp(Zoom * FMath::Pow(1.2f, Event.GetWheelDelta()), 4.f, 80.f);
	Center = Anchor - CursorOffset / Zoom;
	ClampCenter();

	return FReply::Handled();
}
