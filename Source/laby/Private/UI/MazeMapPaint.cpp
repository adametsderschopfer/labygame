#include "MazeMapPaint.h"
#include "Player/MazeKeyBindings.h"
#include "Maze/MazeLayout.h"
#include "UI/MazeInterfaceStyle.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

namespace
{
	// Extend known cells to an open junction, including at the exploration frontier.
	// Only observed walls may cut it back; unseen cells are still never painted.
	// A pit changes its own fill, not neighbouring contours or wall topology.
	bool IsOpenJunction(const FMazeLayout& Layout, TConstArrayView<uint8> Seen, int32 X, int32 Y)
	{
		if (X <= 0 || Y <= 0 || X >= Layout.Size || Y >= Layout.Size)
			return false;

		const int32 Cells[] = {
		    (Y - 1) * Layout.Size + X - 1, (Y - 1) * Layout.Size + X, Y * Layout.Size + X, Y * Layout.Size + X - 1};
		const uint8 FacingWalls[] = {6, 12, 9, 3}; // SE, SW, NW, NE sides facing the junction.

		for (int32 I = 0; I < 4; ++I)
		{
			if (!Seen.IsValidIndex(Cells[I]) || !Seen[Cells[I]])
				continue;

			if (Layout.Walls[Cells[I]] & FacingWalls[I])
				return false;
		}

		return true;
	}

	// A schematic floor ribbon for one known cell. Open sides meet their neighbours
	// without internal seams; inset walls leave a readable dark gutter between rooms.
	void CorridorContour(uint8 Walls, uint8 JoinedCorners, float Step, TArray<FVector2D>& Points)
	{
		const double R = Step * 0.34;
		const double H = Step * 0.5;
		const double Bevel = FMath::Min(Step * 0.07, 3.0);
		TArray<FVector2D, TInlineAllocator<16>> Corners;

		const FVector2D Signs[] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};

		for (int32 I = 0; I < 4; ++I)
		{
			const FVector2D Sign = Signs[I];

			if (JoinedCorners & (1 << I))
			{
				Corners.Add(Sign * H);
				continue;
			}

			if (!(Walls & (1 << ((I + 3) % 4))))
				Corners.Add(Sign * (I % 2 == 0 ? FVector2D(H, R) : FVector2D(R, H)));

			Corners.Add(Sign * R);

			if (!(Walls & (1 << I)))
				Corners.Add(Sign * (I % 2 == 0 ? FVector2D(R, H) : FVector2D(H, R)));
		}

		Points.Reset();

		for (int32 I = 0; I < Corners.Num(); ++I)
		{
			const FVector2D P = Corners[I];

			if (FMath::IsNearlyEqual(FMath::Abs(P.X), H) || FMath::IsNearlyEqual(FMath::Abs(P.Y), H))
			{
				Points.Add(P);
				continue;
			}

			const FVector2D Before = (Corners[(I + Corners.Num() - 1) % Corners.Num()] - P).GetSafeNormal();
			const FVector2D After = (Corners[(I + 1) % Corners.Num()] - P).GetSafeNormal();

			if (FVector2D::DotProduct(Before, After) < -0.99)
				Points.Add(P);
			else
			{
				Points.Add(P + Before * Bevel);
				Points.Add(P + After * Bevel);
			}
		}
	}
}

FSlateRect MazeMapContentRect(const FMazeMapPaintView& View)
{
	if (!View.bFull)
		return FSlateRect(View.Origin + FVector2D(24, 24), View.Origin + View.Size - FVector2D(24, 24));

	const float Sidebar = FMath::Clamp(float(View.Size.X * 0.24), 190.f, 350.f);
	const float Width = FMath::Max(64.f, float(View.Size.X) - Sidebar - 100.f);
	const float Height = FMath::Max(64.f, float(View.Size.Y) - 170.f);
	const float Side = FMath::Min(Width, Height);
	const FVector2D Origin =
	    View.Origin + FVector2D(Sidebar + 50.f + (Width - Side) * 0.5f, 45.f + (Height - Side) * 0.5f);

	return FSlateRect(Origin, Origin + FVector2D(Side, Side));
}

int32 PaintMazeMap(const FGeometry& Geometry,
                   FSlateWindowElementList& Elements,
                   int32 Layer,
                   const FLinearColor& Tint,
                   const FMazeMapPaintView& View)
{
	using namespace MazeInterfaceStyle;

	const FLinearColor Wall = Ink.CopyWithNewOpacity(0.48f);
	const FLinearColor StartColor = Muted, ExitColor = Accent;
	const FLinearColor Danger = Ink;
	const FLinearColor RoomFill = FLinearColor::FromSRGBColor(FColor(112, 167, 173)).CopyWithNewOpacity(0.36f);

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
	auto Stroke = [&](FVector2D A, FVector2D B, FLinearColor Color, float Width = 1.f)
	{
		Path({A, B}, Color, Width);
	};
	const auto Resource =
	    FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FCoreStyle::Get().GetBrush("WhiteBrush"));
	TArray<FSlateVertex> Vertices;
	TArray<SlateIndex> Indices;

	Vertices.Reserve(4096);
	Indices.Reserve(12288);

	const auto FlushPolygons = [&](int32 DrawLayer)
	{
		if (Indices.IsEmpty())
			return;

		FSlateDrawElement::MakeCustomVerts(Elements, DrawLayer, Resource, Vertices, Indices, nullptr, 0, 0);
		Vertices.Reset();
		Indices.Reset();
	};
	// Every contour here is star-shaped about Pivot. Batch the triangle fans so a
	// zoomed-out map needs neither a draw element per cell nor a SceneCapture.
	const auto Polygon = [&](FVector2D Pivot,
	                         const TArray<FVector2D>& Points,
	                         FLinearColor Color,
	                         int32 DrawLayer,
	                         float EdgeOpacity = 1.f)
	{
		if (Vertices.Num() + Points.Num() + 1 > 60000)
			FlushPolygons(DrawLayer);

		const SlateIndex Base = Vertices.Num();
		const auto Vertex = [&](FVector2D P, float Opacity)
		{
			FLinearColor VertexColor = Color * Tint;
			VertexColor.A *= Opacity;

			return FSlateVertex::Make<ESlateVertexRounding::Disabled>(Geometry.GetAccumulatedRenderTransform(),
			                                                          FVector2f(P),
			                                                          FVector2f::ZeroVector,
			                                                          VertexColor.ToFColorSRGB());
		};
		Vertices.Add(Vertex(Pivot, 1.f));

		for (const FVector2D& P : Points)
			Vertices.Add(Vertex(P, EdgeOpacity));

		for (int32 I = 0; I < Points.Num(); ++I)
		{
			Indices.Add(Base);
			Indices.Add(Base + 1 + I);
			Indices.Add(Base + 1 + (I + 1) % Points.Num());
		}
	};
	auto Text = [&](const FText& Value,
	                FVector2D P,
	                int32 FontSize,
	                FLinearColor Color,
	                bool bCentered = false,
	                float MaxWidth = 0.f)
	{
		FSlateFontInfo Font = MazeInterfaceStyle::Font(FontSize, 80);
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
	{
		Box(FVector2D::ZeroVector, Geometry.GetLocalSize(), Glass.CopyWithNewOpacity(0.94f));
		Outline(View.Origin, View.Size, MazeInterfaceStyle::Line);
	}
	else
		Box(View.Origin, View.Size, Glass.CopyWithNewOpacity(0.8f));

	Outline(View.Origin, View.Size, MazeInterfaceStyle::Line, View.bFull ? 0 : 6);
	Box(MapOrigin, MapSize, Glass.CopyWithNewOpacity(0.7f));

	++Layer;
	Elements.PushClip(FSlateClippingZone(Geometry.MakeChild(MapSize, FSlateLayoutTransform(MapOrigin))));

	// A drafting grid is presentation only; undiscovered maze topology is never drawn.
	const float Grid = View.Step * (View.bFull ? 4.f : 2.f);

	for (float X = MapOrigin.X + FMath::Fmod(FMath::Fmod(Offset.X - MapOrigin.X, Grid) + Grid, Grid); X < Content.Right;
	     X += Grid)
		Stroke({X, Content.Top}, {X, Content.Bottom}, MazeInterfaceStyle::Line.CopyWithNewOpacity(0.24f));

	for (float Y = MapOrigin.Y + FMath::Fmod(FMath::Fmod(Offset.Y - MapOrigin.Y, Grid) + Grid, Grid);
	     Y < Content.Bottom;
	     Y += Grid)
		Stroke({Content.Left, Y}, {Content.Right, Y}, MazeInterfaceStyle::Line.CopyWithNewOpacity(0.24f));

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

		const int32 FloorLayer = ++Layer;

		++Layer;

		TArray<FVector2D> Contour;
		TArray<FVector2D> Floor;

		Contour.Reserve(24);
		Floor.Reserve(24);

		TArray<TPair<FVector2D, FVector2D>> WallEdges;
		// A transient presentation mask, derived from this immutable layout on each paint.
		// Seen remains the sole exploration authority; never fill unseen parts of a room.
		TSet<int32> RoomCells;

		for (const FIntRect& Room : Layout->Rooms)
			for (int32 Y = FMath::Max(MinY, Room.Min.Y); Y <= FMath::Min(MaxY, Room.Max.Y - 1); ++Y)
				for (int32 X = FMath::Max(MinX, Room.Min.X); X <= FMath::Min(MaxX, Room.Max.X - 1); ++X)
					RoomCells.Add(Y * Layout->Size + X);

		for (int32 Y = MinY; Y <= MaxY; ++Y)
			for (int32 X = MinX; X <= MaxX; ++X)
			{
				const int32 Index = Y * Layout->Size + X;

				if (!Seen(Index))
					continue;

				const FVector2D C = Offset + FVector2D(X + 0.5, Y + 0.5) * View.Step;
				const uint8 JoinedCorners = uint8(IsOpenJunction(*Layout, View.Seen, X, Y)) |
				                            (IsOpenJunction(*Layout, View.Seen, X + 1, Y) << 1) |
				                            (IsOpenJunction(*Layout, View.Seen, X + 1, Y + 1) << 2) |
				                            (IsOpenJunction(*Layout, View.Seen, X, Y + 1) << 3);

				CorridorContour(Layout->Walls[Index], JoinedCorners, View.Step, Contour);
				Floor.Reset();

				for (const FVector2D& P : Contour)
					Floor.Add(C + P);

				const FLinearColor FloorColor = Layout->HasFloor(Index) && RoomCells.Contains(Index)
				                                    ? RoomFill
				                                    : Muted.CopyWithNewOpacity(Layout->HasFloor(Index) ? 0.3f : 0.06f);
				Polygon(C, Floor, FloorColor, FloorLayer);

				for (int32 I = 0; I < Contour.Num(); ++I)
				{
					const FVector2D A = Contour[I], B = Contour[(I + 1) % Contour.Num()];
					const double H = View.Step * 0.5;
					const bool bVertical = FMath::IsNearlyEqual(A.X, B.X) && FMath::IsNearlyEqual(FMath::Abs(A.X), H);
					const bool bHorizontal = FMath::IsNearlyEqual(A.Y, B.Y) && FMath::IsNearlyEqual(FMath::Abs(A.Y), H);

					if (bVertical || bHorizontal)
					{
						const int32 NX = X + (bVertical ? FMath::Sign(A.X) : 0);
						const int32 NY = Y + (bHorizontal ? FMath::Sign(A.Y) : 0);

						if (NX >= 0 && NX < Layout->Size && NY >= 0 && NY < Layout->Size &&
						    Seen(NY * Layout->Size + NX))
							continue;

						// A faint dashed cut marks the limit of surveyed floor; never draw
						// the neighbouring unseen cell or pretend this frontier is a wall.
						for (int32 Dash = 0; Dash < 3; ++Dash)
							Stroke(C + FMath::Lerp(A, B, Dash / 3.0),
							       C + FMath::Lerp(A, B, (Dash + 0.45) / 3.0),
							       Muted.CopyWithNewOpacity(0.45f));

						continue;
					}

					WallEdges.Emplace(C + A, C + B);
				}

				if (!Layout->HasFloor(Index))
				{
					const double R = FMath::Clamp(View.Step * 0.18, 2.0, 6.0);
					Path({C + FVector2D(0, -R),
					      C + FVector2D(R, 0),
					      C + FVector2D(0, R),
					      C + FVector2D(-R, 0),
					      C + FVector2D(0, -R)},
					     Danger.CopyWithNewOpacity(0.7f));
					Stroke(C - FVector2D(R * 0.5, 0), C + FVector2D(R * 0.5, 0), Danger);
				}
			}

		FlushPolygons(FloorLayer);

		// Join across cell boundaries before drawing. A single continuous Slate path
		// avoids the repeated antialiased end caps of the old per-cell wall strokes.
		const auto Key = [&](FVector2D P)
		{
			const FVector2D Local = (P - Offset) / View.Step * 1024;

			return FIntPoint(FMath::RoundToInt(Local.X), FMath::RoundToInt(Local.Y));
		};
		TMap<FIntPoint, int32> Next;
		TSet<FIntPoint> Ends;
		TArray<bool> Used;

		Used.Init(false, WallEdges.Num());

		for (int32 I = 0; I < WallEdges.Num(); ++I)
		{
			Next.Add(Key(WallEdges[I].Key), I);
			Ends.Add(Key(WallEdges[I].Value));
		}

		TArray<FVector2D> Joined;

		for (int32 Pass = 0; Pass < 2; ++Pass)
			for (int32 First = 0; First < WallEdges.Num(); ++First)
			{
				if (Used[First] || (Pass == 0 && Ends.Contains(Key(WallEdges[First].Key))))
					continue;

				Joined.Reset();
				Joined.Add(WallEdges[First].Key);
				int32 Current = First;

				while (!Used[Current])
				{
					Used[Current] = true;
					const FVector2D P = WallEdges[Current].Value;

					if (Joined.Num() >= 2)
					{
						const FVector2D A = (Joined.Last() - Joined[Joined.Num() - 2]).GetSafeNormal();
						const FVector2D B = (P - Joined.Last()).GetSafeNormal();

						if (FVector2D::DotProduct(A, B) > 0.9999)
							Joined.Pop(EAllowShrinking::No);
					}

					Joined.Add(P);
					const int32* Following = Next.Find(Key(P));

					if (!Following)
						break;

					Current = *Following;
				}

				Path(Joined, Wall, FMath::Clamp(View.Step * 0.035f, 0.7f, 1.2f));
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

	for (const FMazeMapSignal& Signal : View.Signals)
	{
		const FVector2D Center = Offset + Signal.Position * View.Step;
		const FLinearColor SignalColor = Signal.bLocal ? Muted : Accent;
		const float SignalFade = 1.f - FMath::Clamp(Signal.Progress, 0.f, 1.f);

		for (int32 Ring = 0; Ring < 3; ++Ring)
		{
			const float Phase = FMath::Fmod(FMath::Clamp(Signal.Progress, 0.f, 1.f) * 1.5f + Ring / 3.f, 1.f);
			const float Radius = FMath::Lerp(5.f, View.bFull ? 44.f : 28.f, Phase);
			TArray<FVector2D> Circle;

			Circle.Reserve(25);

			for (int32 Segment = 0; Segment <= 24; ++Segment)
			{
				const float A = 2.f * PI * Segment / 24.f;

				Circle.Add(Center + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Radius);
			}

			Path(Circle, SignalColor.CopyWithNewOpacity((1.f - Phase) * SignalFade * 0.9f), Ring == 0 ? 2.f : 1.2f);
		}
	}

	++Layer;

	const float Angle = FMath::DegreesToRadians(View.Yaw);
	const FVector2D Forward(FMath::Cos(Angle), FMath::Sin(Angle)), Right(-Forward.Y, Forward.X);
	const FVector2D Player = Offset + View.Player * View.Step;
	const TArray<FVector2D> Arrow{
	    Player + Forward * 10, Player - Forward * 6 + Right * 5, Player - Forward * 6 - Right * 5};
	TArray<FVector2D> Fan;

	Fan.Add(Player);

	for (int32 I = -12; I <= 12; ++I)
	{
		const float A = Angle + FMath::DegreesToRadians(I * 2.5f);

		Fan.Add(Player + FVector2D(FMath::Cos(A), FMath::Sin(A)) * (View.bFull ? 64 : 38));
	}

	Polygon(Player, Fan, Accent.CopyWithNewOpacity(0.28f), Layer, 0.f);
	FlushPolygons(Layer);
	++Layer;
	Path({Arrow[0], Arrow[1], Arrow[2], Arrow[0]}, Glass, 3.f);
	++Layer;
	Polygon(Player, Arrow, Accent, Layer);
	FlushPolygons(Layer);

	if (View.bFull && Player.X > Content.Left + 20 && Player.X < Content.Right - 170 && Player.Y > Content.Top + 55 &&
	    Player.Y < Content.Bottom - 20)
	{
		Path({Player + FVector2D(10, -10), Player + FVector2D(36, -37), Player + FVector2D(151, -37)}, Muted);
		Text(NSLOCTEXT("Maze.Glass", "YouAreHere", "ВЫ ЗДЕСЬ"), Player + FVector2D(45, -62), 12, Ink);
	}

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
		const FVector2D Positions[] = {{MapMiddle.X, Content.Top - (View.bFull ? 32 : 14)},
		                               {Content.Right + (View.bFull ? 32 : 14), MapMiddle.Y},
		                               {MapMiddle.X, Content.Bottom + (View.bFull ? 32 : 14)},
		                               {Content.Left - (View.bFull ? 32 : 14), MapMiddle.Y}};
		const TCHAR* Names[] = {TEXT("N"), TEXT("E"), TEXT("S"), TEXT("W")};

		for (int32 I = 0; I < 4; ++I)
			Text(FText::AsCultureInvariant(Names[I]), Positions[I], 13, I == 0 ? Accent : Ink, true);

		for (int32 I = 1; I < 10; ++I)
		{
			if (I == 5)
				continue;

			const double T = I / 10.0;

			Stroke({Content.Left + MapSize.X * T, Content.Top - 5},
			       {Content.Left + MapSize.X * T, Content.Top - 2},
			       Muted);
			Stroke({Content.Left + MapSize.X * T, Content.Bottom + 2},
			       {Content.Left + MapSize.X * T, Content.Bottom + 5},
			       Muted);
			Stroke({Content.Left - 5, Content.Top + MapSize.Y * T},
			       {Content.Left - 2, Content.Top + MapSize.Y * T},
			       Muted);
			Stroke({Content.Right + 2, Content.Top + MapSize.Y * T},
			       {Content.Right + 5, Content.Top + MapSize.Y * T},
			       Muted);
		}

		// Project camera bearing onto the rectangular rim, independent of panning and zooming.
		const double Reach =
		    1.0 / FMath::Max(FMath::Abs(Forward.X) / (MapSize.X * 0.5), FMath::Abs(Forward.Y) / (MapSize.Y * 0.5));
		const FVector2D Mark = MapMiddle + Forward * Reach;

		// Three round bearing dots follow the same compass rim as the former chevron.
		for (int32 Dot = 0; Dot < 3; ++Dot)
		{
			const FVector2D Center = Mark - Forward * (Dot * 7.0);
			const double Radius = Dot == 0 ? 3.0 : 2.0;
			TArray<FVector2D> Circle;

			for (int32 Segment = 0; Segment < 16; ++Segment)
			{
				const double SegmentAngle = 2.0 * PI * Segment / 16.0;

				Circle.Add(Center + FVector2D(FMath::Cos(SegmentAngle), FMath::Sin(SegmentAngle)) * Radius);
			}

			Polygon(Center, Circle, Accent.CopyWithNewOpacity(1.f - Dot * 0.25f), Layer);
		}

		FlushPolygons(Layer);
	}

	if (View.bFull)
	{
		const float Sidebar = FMath::Clamp(float(View.Size.X * 0.24), 190.f, 350.f);
		const FVector2D Left = View.Origin + FVector2D(36, 38);
		const float TextWidth = Sidebar - 52;

		Text(NSLOCTEXT("Maze.Glass", "Brand", "LABY"), Left, 38, Ink);
		Text(NSLOCTEXT("Maze.Glass", "BrandSubtitle", "Л А Б И Р И Н Т"), Left + FVector2D(2, 61), 10, Muted);
		Text(NSLOCTEXT("Maze.Map", "Title", "КАРТА ЛАБИРИНТА"), Left + FVector2D(0, 139), 20, Ink, false, TextWidth);
		Stroke(Left + FVector2D(0, 181), Left + FVector2D(TextWidth, 181), MazeInterfaceStyle::Line);
		Text(NSLOCTEXT("Maze.Glass", "LegendTitle", "ОБОЗНАЧЕНИЯ"), Left + FVector2D(0, 229), 12, Muted);

		const FText Labels[] = {NSLOCTEXT("Maze.Glass", "YouAreHere", "ВЫ ЗДЕСЬ"),
		                        NSLOCTEXT("Maze.Glass", "Explored", "ИССЛЕДОВАНО"),
		                        NSLOCTEXT("Maze.Glass", "Unknown", "НЕИЗВЕСТНО"),
		                        NSLOCTEXT("Maze.Glass", "Start", "НАЧАЛО"),
		                        NSLOCTEXT("Maze.Glass", "Exit", "ВЫХОД"),
		                        NSLOCTEXT("Maze.Glass", "Gap", "ПРОВАЛ"),
		                        NSLOCTEXT("Maze.Map", "DiscoveredRoom", "КОМНАТА")};

		for (int32 I = 0; I < UE_ARRAY_COUNT(Labels); ++I)
		{
			const FVector2D P = Left + FVector2D(9, 291 + I * 47);

			if (I == 0)
			{
				Polygon(P, {P + FVector2D(7, 0), P + FVector2D(-5, 5), P + FVector2D(-5, -5)}, Accent, Layer);
				FlushPolygons(Layer);
			}
			else if (I == 1)
			{
				Box(P - FVector2D(9, 4), {18, 8}, Muted.CopyWithNewOpacity(0.3f));
				Stroke(P + FVector2D(-9, -4), P + FVector2D(9, -4), Wall);
				Stroke(P + FVector2D(-9, 4), P + FVector2D(9, 4), Wall);
			}
			else if (I == 2)
				for (int32 D = 0; D < 3; ++D)
					Stroke(P + FVector2D(-9 + D * 7, 0), P + FVector2D(-6 + D * 7, 0), Muted);

			else if (I == 4)
			{
				Outline(P - FVector2D(5, 5), {10, 10}, ExitColor);
				Box(P - FVector2D(2, 2), {4, 4}, ExitColor);
			}
			else if (I == 6)
			{
				Box(P - FVector2D(8, 6), {16, 12}, RoomFill);
				Outline(P - FVector2D(8, 6), {16, 12}, Wall);
			}
			else
			{
				Path({P + FVector2D(0, -5),
				      P + FVector2D(5, 0),
				      P + FVector2D(0, 5),
				      P + FVector2D(-5, 0),
				      P + FVector2D(0, -5)},
				     I == 5 ? Danger : StartColor);

				if (I == 5)
					Stroke(P - FVector2D(2.5, 0), P + FVector2D(2.5, 0), Danger);
			}

			Text(Labels[I], Left + FVector2D(38, 282 + I * 47), 12, Ink, false, TextWidth - 40);
		}

		if (View.bPreview)
			Text(NSLOCTEXT("Maze.Glass", "StaticPreview", "МАКЕТ / ПРИМЕР ДАННЫХ"),
			     Left + FVector2D(0, 650),
			     9,
			     Muted,
			     false,
			     TextWidth);

		// Coordinates describe the current world grid, not fabricated unexplored corridors.
		const int32 Stride = FMath::Max(1, FMath::CeilToInt(36.f / View.Step));
		const int32 FirstX = FMath::CeilToInt((Content.Left - Offset.X) / View.Step / Stride);
		const int32 FirstY = FMath::CeilToInt((Content.Top - Offset.Y) / View.Step / Stride);

		for (int32 I = FirstX; Offset.X + I * Stride * View.Step < Content.Right; ++I)
		{
			const float X = Offset.X + I * Stride * View.Step;

			Text(FText::AsNumber(I * Stride), {X, Content.Top - 12}, 8, Muted, true);
			Text(FText::AsNumber(I * Stride), {X, Content.Bottom + 12}, 8, Muted, true);
		}

		for (int32 I = FirstY; Offset.Y + I * Stride * View.Step < Content.Bottom; ++I)
		{
			const float Y = Offset.Y + I * Stride * View.Step;

			Text(FText::AsNumber(I * Stride), {Content.Left - 12, Y}, 8, Muted, true);
			Text(FText::AsNumber(I * Stride), {Content.Right + 12, Y}, 8, Muted, true);
		}
	}
	else
		Text(FText::Format(NSLOCTEXT("Maze.Glass", "MapHintKey", "{0}   КАРТА"),
		                   MazeKeyBindings::GetKey(TEXT("Map")).GetDisplayName()),
		     View.Origin + FVector2D(0, -26),
		     11,
		     Ink);

	return Layer;
}
