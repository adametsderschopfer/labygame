#include "MazeMapPaint.h"
#include "Maze/MazeLayout.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

FSlateRect MazeMapContentRect(const FMazeMapPaintView& View)
{
	const FVector2D Inset = View.bFull ? FVector2D(36, 88) : FVector2D(28, 28);

	return FSlateRect(View.Origin + Inset, View.Origin + View.Size - Inset);
}

int32 PaintMazeMap(const FGeometry& Geometry,
                   FSlateWindowElementList& Elements,
                   int32 Layer,
                   const FLinearColor& Tint,
                   const FMazeMapPaintView& View)
{
	const FLinearColor Ink(0.7f, 0.73f, 0.73f), Muted(0.29f, 0.32f, 0.33f);
	const FLinearColor Accent(0.86f, 0.65f, 0.32f), Wall(0.42f, 0.46f, 0.47f);
	const FLinearColor StartColor(0.38f, 0.58f, 0.66f), ExitColor(0.5f, 0.7f, 0.52f);
	const FLinearColor Danger(0.74f, 0.34f, 0.22f);
	const FSlateRect Content = MazeMapContentRect(View);
	const FVector2D MapOrigin(Content.Left, Content.Top),
	    MapSize(Content.Right - Content.Left, Content.Bottom - Content.Top);
	const FVector2D MapMiddle = MapOrigin + MapSize * 0.5;
	const FVector2D Offset = MapMiddle - View.Center * View.Step;
	const auto Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	if (MapSize.X <= 0 || MapSize.Y <= 0 || View.Step <= 0)
		return Layer;

	auto Box = [&](FVector2D P, FVector2D Size, FLinearColor Color)
	{
		FSlateDrawElement::MakeBox(Elements,
		                           Layer,
		                           Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(P)),
		                           FCoreStyle::Get().GetBrush("WhiteBrush"),
		                           ESlateDrawEffect::None,
		                           Color * Tint);
	};
	auto Path = [&](const TArray<FVector2D>& Points, FLinearColor Color, float Width = 1.f)
	{
		FSlateDrawElement::MakeLines(
		    Elements, Layer, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color * Tint, true, Width);
	};
	auto Line = [&](FVector2D A, FVector2D B, FLinearColor Color, float Width = 1.f)
	{
		Path({A, B}, Color, Width);
	};
	auto Text = [&](const FText& Value,
	                FVector2D P,
	                int32 FontSize,
	                FLinearColor Color,
	                bool bCentered = false,
	                float MaxWidth = 0.f)
	{
		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", FontSize);
		FVector2D Extent = Measure->Measure(Value, Font);

		if (MaxWidth > 0 && Extent.X > MaxWidth)
		{
			Font.Size = FMath::Max(8, FMath::FloorToInt(FontSize * MaxWidth / Extent.X));
			Extent = Measure->Measure(Value, Font);
		}

		if (bCentered)
			P -= Extent * 0.5;

		FSlateDrawElement::MakeText(Elements,
		                            Layer,
		                            Geometry.ToPaintGeometry(Extent, FSlateLayoutTransform(P)),
		                            Value,
		                            Font,
		                            ESlateDrawEffect::None,
		                            Color * Tint);
	};
	auto Outline = [&](FVector2D P, FVector2D Size, FLinearColor Color, float Cut = 0.f)
	{
		Path({P + FVector2D(Cut, 0),
		      P + FVector2D(Size.X - Cut, 0),
		      P + FVector2D(Size.X, Cut),
		      P + FVector2D(Size.X, Size.Y - Cut),
		      P + FVector2D(Size.X - Cut, Size.Y),
		      P + FVector2D(Cut, Size.Y),
		      P + FVector2D(0, Size.Y - Cut),
		      P + FVector2D(0, Cut),
		      P + FVector2D(Cut, 0)},
		     Color);
	};

	++Layer;

	if (View.bFull)
		Box(FVector2D::ZeroVector, Geometry.GetLocalSize(), FLinearColor(0.004f, 0.006f, 0.008f, 0.94f));

	Box(View.Origin + FVector2D(4, 7), View.Size, FLinearColor(0, 0, 0, 0.65f));
	Box(View.Origin, View.Size, FLinearColor(0.028f, 0.032f, 0.035f, 0.98f));
	Box(View.Origin + FVector2D(2, 2), View.Size - FVector2D(4, 4), FLinearColor(0.013f, 0.016f, 0.019f, 0.98f));
	Outline(View.Origin, View.Size, Muted, 9);
	Line(
	    View.Origin + FVector2D(11, 1), View.Origin + FVector2D(View.Size.X - 11, 1), FLinearColor(0.46f, 0.49f, 0.5f));
	Box(MapOrigin, MapSize, FLinearColor(0.006f, 0.009f, 0.011f));

	++Layer;
	Elements.PushClip(FSlateClippingZone(Geometry.MakeChild(MapSize, FSlateLayoutTransform(MapOrigin))));

	// A drafting grid is presentation only; undiscovered maze topology is never drawn.
	const float Grid = View.Step * (View.bFull ? 4.f : 2.f);

	for (float X = MapOrigin.X + FMath::Fmod(FMath::Fmod(Offset.X - MapOrigin.X, Grid) + Grid, Grid); X < Content.Right;
	     X += Grid)
		Line({X, Content.Top}, {X, Content.Bottom}, FLinearColor(0.016f, 0.021f, 0.024f));

	for (float Y = MapOrigin.Y + FMath::Fmod(FMath::Fmod(Offset.Y - MapOrigin.Y, Grid) + Grid, Grid);
	     Y < Content.Bottom;
	     Y += Grid)
		Line({Content.Left, Y}, {Content.Right, Y}, FLinearColor(0.016f, 0.021f, 0.024f));

	const FMazeLayout* Layout = View.Layout;
	auto Seen = [&](int32 Index)
	{
		return View.Seen.IsValidIndex(Index) && View.Seen[Index] != 0;
	};

	if (Layout && Layout->Size > 0 && Layout->Walls.Num() == Layout->Size * Layout->Size)
	{
		const int32 MinX = FMath::Clamp(FMath::FloorToInt((Content.Left - Offset.X) / View.Step), 0, Layout->Size - 1);
		const int32 MinY = FMath::Clamp(FMath::FloorToInt((Content.Top - Offset.Y) / View.Step), 0, Layout->Size - 1);
		const int32 MaxX = FMath::Clamp(FMath::CeilToInt((Content.Right - Offset.X) / View.Step), 0, Layout->Size - 1);
		const int32 MaxY = FMath::Clamp(FMath::CeilToInt((Content.Bottom - Offset.Y) / View.Step), 0, Layout->Size - 1);

		++Layer;

		for (int32 Y = MinY; Y <= MaxY; ++Y)
			for (int32 X = MinX; X <= MaxX; ++X)
			{
				const int32 Index = Y * Layout->Size + X;

				if (!Seen(Index))
					continue;

				const FVector2D P = Offset + FVector2D(X, Y) * View.Step;
				Box(P, FVector2D(View.Step, View.Step), FLinearColor(0.04f, 0.05f, 0.054f));
			}

		++Layer;

		for (int32 Y = MinY; Y <= MaxY; ++Y)
			for (int32 X = MinX; X <= MaxX; ++X)
			{
				const int32 Index = Y * Layout->Size + X;

				if (!Seen(Index))
					continue;

				const FVector2D P = Offset + FVector2D(X, Y) * View.Step;
				const uint8 Walls = Layout->Walls[Index];
				const float Width = View.bFull ? FMath::Clamp(View.Step * 0.065f, 1.f, 2.5f) : 1.5f;

				if (Walls & 1)
					Line(P, P + FVector2D(View.Step, 0), Wall, Width);

				if (Walls & 2)
					Line(P + FVector2D(View.Step, 0), P + FVector2D(View.Step, View.Step), Wall, Width);

				if (Walls & 4)
					Line(P + FVector2D(0, View.Step), P + FVector2D(View.Step, View.Step), Wall, Width);

				if (Walls & 8)
					Line(P, P + FVector2D(0, View.Step), Wall, Width);

				if (!Layout->HasFloor(Index))
				{
					Line(P + FVector2D(0.28, 0.28) * View.Step, P + FVector2D(0.72, 0.72) * View.Step, Danger);
					Line(P + FVector2D(0.72, 0.28) * View.Step, P + FVector2D(0.28, 0.72) * View.Step, Danger);
				}
			}

		++Layer;

		for (int32 Exit : Layout->Exits)
			if (Seen(Exit))
			{
				const FVector2D P =
				    Offset + FVector2D(Exit % Layout->Size + 0.5, Exit / Layout->Size + 0.5) * View.Step;
				Outline(P - FVector2D(5, 5), {10, 10}, ExitColor);
				Box(P - FVector2D(2, 2), {4, 4}, ExitColor);
			}

		const int32 StartIndex = FMath::FloorToInt(View.Start.Y) * Layout->Size + FMath::FloorToInt(View.Start.X);

		if (Seen(StartIndex))
		{
			const FVector2D P = Offset + View.Start * View.Step;

			Path({P + FVector2D(0, -5),
			      P + FVector2D(5, 0),
			      P + FVector2D(0, 5),
			      P + FVector2D(-5, 0),
			      P + FVector2D(0, -5)},
			     StartColor,
			     1.5f);
		}
	}

	++Layer;

	const float Angle = FMath::DegreesToRadians(View.Yaw);
	const FVector2D Forward(FMath::Cos(Angle), FMath::Sin(Angle)), Right(-Forward.Y, Forward.X);
	const FVector2D Player = Offset + View.Player * View.Step;
	const TArray<FVector2D> Arrow{Player + Forward * 10,
	                              Player - Forward * 6 + Right * 5,
	                              Player - Forward * 3,
	                              Player - Forward * 6 - Right * 5,
	                              Player + Forward * 10};

	Path(Arrow, FLinearColor(0.01f, 0.012f, 0.015f), 5.f);
	++Layer;
	Path(Arrow, Accent, 2.f);
	Elements.PopClip();

	++Layer;
	Outline(MapOrigin - FVector2D(1, 1), MapSize + FVector2D(2, 2), Muted);

	for (int32 Corner = 0; Corner < 4; ++Corner)
	{
		const FVector2D Sign(Corner & 1 ? -1 : 1, Corner & 2 ? -1 : 1);
		const FVector2D P(Corner & 1 ? Content.Right : Content.Left, Corner & 2 ? Content.Bottom : Content.Top);

		Path({P + FVector2D(Sign.X * 12, 0), P, P + FVector2D(0, Sign.Y * 12)}, Ink, 2);
	}

	if (View.bCompass)
	{
		const FVector2D Positions[] = {{MapMiddle.X, Content.Top - 14},
		                               {Content.Right + 14, MapMiddle.Y},
		                               {MapMiddle.X, Content.Bottom + 14},
		                               {Content.Left - 14, MapMiddle.Y}};
		const TCHAR* Names[] = {TEXT("N"), TEXT("E"), TEXT("S"), TEXT("W")};

		for (int32 I = 0; I < 4; ++I)
			Text(FText::AsCultureInvariant(Names[I]), Positions[I], 13, I == 0 ? Accent : Ink, true);

		for (int32 I = 1; I < 10; ++I)
		{
			if (I == 5)
				continue;

			const double T = I / 10.0;

			Line({Content.Left + MapSize.X * T, Content.Top - 5},
			     {Content.Left + MapSize.X * T, Content.Top - 2},
			     Muted);
			Line({Content.Left + MapSize.X * T, Content.Bottom + 2},
			     {Content.Left + MapSize.X * T, Content.Bottom + 5},
			     Muted);
			Line({Content.Left - 5, Content.Top + MapSize.Y * T},
			     {Content.Left - 2, Content.Top + MapSize.Y * T},
			     Muted);
			Line({Content.Right + 2, Content.Top + MapSize.Y * T},
			     {Content.Right + 5, Content.Top + MapSize.Y * T},
			     Muted);
		}

		// Project camera bearing onto the rectangular rim, independent of panning and zooming.
		const double Reach =
		    1.0 / FMath::Max(FMath::Abs(Forward.X) / (MapSize.X * 0.5), FMath::Abs(Forward.Y) / (MapSize.Y * 0.5));
		const FVector2D Mark = MapMiddle + Forward * Reach;

		Path({Mark - Forward * 10 + Right * 5, Mark, Mark - Forward * 10 - Right * 5}, Accent, 2.f);
	}

	const FText Heading = FText::AsCultureInvariant(
	    FString::Printf(TEXT("%03d°"), FMath::RoundToInt(FRotator::ClampAxis(View.Yaw + 90.f)) % 360));

	if (View.bFull)
	{
		Text(NSLOCTEXT("Maze.Map", "Title", "КАРТА ЛАБИРИНТА"),
		     View.Origin + FVector2D(36, 22),
		     26,
		     Ink,
		     false,
		     View.Size.X - 170);
		Text(View.bPreview ? NSLOCTEXT("Maze.Map", "Preview", "ПРЕДПРОСМОТР ОФОРМЛЕНИЯ")
		                   : NSLOCTEXT("Maze.Map", "DiscoveredOnly", "ТОЛЬКО ИССЛЕДОВАННЫЕ УЧАСТКИ"),
		     View.Origin + FVector2D(36, 57),
		     11,
		     Muted,
		     false,
		     View.Size.X - 170);

		if (View.bCompass)
			Text(Heading, View.Origin + FVector2D(View.Size.X - 73, 42), 20, Accent, true);

		const float Bottom = View.Origin.Y + View.Size.Y - 50;

		Text(NSLOCTEXT("Maze.Map", "Legend", "▲ ВЫ     ◇ НАЧАЛО     □ ВЫХОД     × ПРОВАЛ"),
		     {View.Origin.X + 36, Bottom},
		     13,
		     Ink,
		     false,
		     View.Size.X - 72);
		Text(NSLOCTEXT("Maze.Map",
		               "Navigation",
		               "ЛКМ — перемещение     КОЛЕСО — масштаб     HOME — к игроку     M / ESC — закрыть"),
		     {View.Origin.X + 36, Bottom + 25},
		     12,
		     Muted,
		     false,
		     View.Size.X - 72);
	}
	else
	{
		Text(NSLOCTEXT("Maze.Map", "MinimapTitle", "НАВИГАЦИЯ"), View.Origin + FVector2D(1, -25), 12, Ink);

		if (View.bCompass)
			Text(Heading, View.Origin + FVector2D(View.Size.X - 52, -25), 12, Accent);

		Text(NSLOCTEXT("Maze.Map", "OpenMap", "M   /   ОТКРЫТЬ КАРТУ"),
		     View.Origin + FVector2D(1, View.Size.Y + 10),
		     11,
		     Muted);
	}

	return Layer;
}
