#include "UI/MazeItemPresentation.h"
#include "UI/MazeInterfaceStyle.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/UserWidget.h"
#include "Player/MazeCharacter.h"
#include "Player/MazeKeyBindings.h"
#include "Player/MazePlayerController.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

namespace
{
	TArray<FVector2D> RoundedRect(FVector2D Position, FVector2D Size, float Radius)
	{
		TArray<FVector2D> Points;
		const FVector2D Centers[] = {Position + FVector2D(Size.X - Radius, Radius),
		                             Position + Size - FVector2D(Radius, Radius),
		                             Position + FVector2D(Radius, Size.Y - Radius),
		                             Position + FVector2D(Radius, Radius)};

		for (int32 Corner = 0; Corner < 4; ++Corner)
			for (int32 Step = 0; Step <= 4; ++Step)
			{
				const float Angle = (Corner * 90.f - 90.f) + Step * 22.5f;

				Points.Add(Centers[Corner] + FVector2D(FMath::Cos(FMath::DegreesToRadians(Angle)),
				                                       FMath::Sin(FMath::DegreesToRadians(Angle))) *
				                                 Radius);
			}

		const FVector2D FirstPoint = Points[0];

		Points.Add(FirstPoint);

		return Points;
	}

	double Cross(FVector2D A, FVector2D B, FVector2D C)
	{
		return (B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X);
	}

	int32 GradientLeader(const FGeometry& Geometry,
	                     FSlateWindowElementList& Elements,
	                     int32 Layer,
	                     const TArray<FVector2D>& Points,
	                     const FLinearColor& StartColor,
	                     const FLinearColor& EndColor)
	{
		float TotalLength = 0.f;

		for (int32 Index = 1; Index < Points.Num(); ++Index)
			TotalLength += FVector2D::Distance(Points[Index - 1], Points[Index]);

		if (TotalLength <= UE_SMALL_NUMBER)
			return Layer;

		float Covered = 0.f;
		TArray<FVector2D> Segment;

		Segment.SetNum(2);

		for (int32 Index = 1; Index < Points.Num(); ++Index)
		{
			const FVector2D From = Points[Index - 1];
			const FVector2D To = Points[Index];
			const float Length = FVector2D::Distance(From, To);
			const int32 SegmentCount = FMath::Max(1, FMath::CeilToInt(Length / 8.f));

			for (int32 Step = 0; Step < SegmentCount; ++Step)
			{
				Segment[0] = FMath::Lerp(From, To, float(Step) / SegmentCount);
				Segment[1] = FMath::Lerp(From, To, float(Step + 1) / SegmentCount);

				const float Progress = (Covered + Length * (Step + .5f) / SegmentCount) / TotalLength;
				const FLinearColor Color = FMath::Lerp(StartColor, EndColor, Progress);

				FSlateDrawElement::MakeLines(
				    Elements, ++Layer, Geometry.ToPaintGeometry(), Segment, ESlateDrawEffect::None, Color, true, 2.f);
			}

			Covered += Length;
		}

		return Layer;
	}

	TArray<FVector2D> ConvexOutline(TArray<FVector2D> Points, FVector2D Center)
	{
		Points.Sort(
		    [](const FVector2D& A, const FVector2D& B)
		    {
			    return A.X == B.X ? A.Y < B.Y : A.X < B.X;
		    });

		TArray<FVector2D> Hull;

		for (const FVector2D Point : Points)
		{
			while (Hull.Num() >= 2 && Cross(Hull[Hull.Num() - 2], Hull.Last(), Point) <= 0.)
				Hull.Pop();

			Hull.Add(Point);
		}

		const int32 LowerCount = Hull.Num();

		for (int32 Index = Points.Num() - 2; Index >= 0; --Index)
		{
			const FVector2D Point = Points[Index];

			while (Hull.Num() > LowerCount && Cross(Hull[Hull.Num() - 2], Hull.Last(), Point) <= 0.)
				Hull.Pop();

			Hull.Add(Point);
		}

		if (Hull.Num() < 4)
			return {};

		for (FVector2D& Point : Hull)
			Point += (Point - Center).GetSafeNormal() * 5.f;

		const FVector2D FirstPoint = Hull[0];

		Hull.Add(FirstPoint);

		return Hull;
	}
}

int32 MazeItemPresentation::PaintFocus(const UUserWidget& Owner,
                                       const FGeometry& Geometry,
                                       FSlateWindowElementList& Elements,
                                       int32 Layer)
{
	const auto* Controller = Owner.GetOwningPlayer<AMazePlayerController>();
	const auto* Player = Controller ? Cast<AMazeCharacter>(Controller->GetPawn()) : nullptr;
	TArray<FVector> WorldOutline;
	FVector ItemLocation;

	if (!Player || Controller->IsMenuOpen() || Controller->IsMapOpen() ||
	    !Player->GetFocusedPickup(WorldOutline, ItemLocation))
		return Layer;

	FVector2D Minimum(TNumericLimits<double>::Max(), TNumericLimits<double>::Max());
	FVector2D Maximum(-TNumericLimits<double>::Max(), -TNumericLimits<double>::Max());
	TArray<FVector2D> ScreenOutline;
	FVector2D Center;

	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(Controller, ItemLocation, Center, true))
		return Layer;

	for (const FVector& Point : WorldOutline)
	{
		FVector2D Projected;

		if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(Controller, Point, Projected, true))
			return Layer;

		ScreenOutline.Add(Projected);
		Minimum.X = FMath::Min(Minimum.X, Projected.X);
		Minimum.Y = FMath::Min(Minimum.Y, Projected.Y);
		Maximum.X = FMath::Max(Maximum.X, Projected.X);
		Maximum.Y = FMath::Max(Maximum.Y, Projected.Y);
	}

	const TArray<FVector2D> Contour = ConvexOutline(MoveTemp(ScreenOutline), Center);

	if (Contour.IsEmpty())
		return Layer;

	Minimum -= FVector2D(7.f, 7.f);
	Maximum += FVector2D(7.f, 7.f);

	const FVector2D Screen = Geometry.GetLocalSize();
	const auto Palette = MazeInterfaceStyle::Palette();
	const FLinearColor Accent = Palette.Accent.CopyWithNewOpacity(.95f);
	const FText KeyText = MazeKeyBindings::GetKey(TEXT("Interact")).GetDisplayName();
	const auto KeyFont = MazeInterfaceStyle::Font(17, 0);
	const auto Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const float KeyWidth = FMath::Max(32.f, Measure->Measure(KeyText, KeyFont).X + 16.f);
	const FVector2D LabelSize(193.f + KeyWidth, 57.f);
	const FVector2D Label(FMath::Clamp(Maximum.X + 45.f, 12.f, Screen.X - LabelSize.X - 12.f),
	                      FMath::Clamp(Minimum.Y - 78.f, 12.f, Screen.Y - LabelSize.Y - 12.f));

	FSlateDrawElement::MakeLines(
	    Elements, ++Layer, Geometry.ToPaintGeometry(), Contour, ESlateDrawEffect::None, Accent, true, 2.f);

	const FVector2D Start(Maximum.X, (Minimum.Y + Maximum.Y) * .5f);
	const FVector2D Finish(Label.X, Label.Y + LabelSize.Y * .5f);
	const float Direction = Finish.X >= Start.X ? 1.f : -1.f;
	const FVector2D Bend(Start.X + Direction * 24.f, Start.Y);
	TArray<FVector2D> Leader = {Start, Bend - FVector2D(Direction * 5.f, 0.f)};

	for (int32 Step = 1; Step <= 4; ++Step)
	{
		const float T = Step / 4.f;

		Leader.Add(FMath::Square(1.f - T) * (Bend - FVector2D(Direction * 5.f, 0.f)) + 2.f * T * (1.f - T) * Bend +
		           FMath::Square(T) * (Bend + FVector2D(Direction * 5.f, 0.f)));
	}

	Leader.Add(Finish);
	Layer = GradientLeader(Geometry,
	                       Elements,
	                       Layer,
	                       Leader,
	                       Palette.Ink.CopyWithNewOpacity(.85f),
	                       Palette.Accent.CopyWithNewOpacity(.35f));

	const FVector2D ArrowDirection = (Leader[1] - Start).GetSafeNormal();
	const FVector2D ArrowNormal(-ArrowDirection.Y, ArrowDirection.X);
	const TArray<FVector2D> Arrow = {
	    Start + ArrowDirection * 10.f + ArrowNormal * 5.f, Start, Start + ArrowDirection * 10.f - ArrowNormal * 5.f};

	FSlateDrawElement::MakeLines(
	    Elements, ++Layer, Geometry.ToPaintGeometry(), Arrow, ESlateDrawEffect::None, Palette.Ink, true, 2.f);
	FSlateDrawElement::MakeBox(Elements,
	                           ++Layer,
	                           Geometry.ToPaintGeometry(LabelSize, FSlateLayoutTransform(Label)),
	                           FCoreStyle::Get().GetBrush("WhiteBrush"),
	                           ESlateDrawEffect::None,
	                           Palette.BackgroundMid.CopyWithNewOpacity(.95f));
	FSlateDrawElement::MakeLines(Elements,
	                             ++Layer,
	                             Geometry.ToPaintGeometry(),
	                             RoundedRect(Label, LabelSize, 8.f),
	                             ESlateDrawEffect::None,
	                             Accent,
	                             true,
	                             1.5f);
	FSlateDrawElement::MakeText(
	    Elements,
	    ++Layer,
	    Geometry.ToPaintGeometry(FVector2D(172.f, 28.f), FSlateLayoutTransform(Label + FVector2D(12.f, 17.f))),
	    NSLOCTEXT("Maze.Items", "Headlamp", "Налобный фонарик"),
	    MazeInterfaceStyle::Font(17, 0),
	    ESlateDrawEffect::None,
	    Palette.Ink);

	const FVector2D KeyPosition = Label + FVector2D(183.f, 11.f);

	FSlateDrawElement::MakeBox(Elements,
	                           ++Layer,
	                           Geometry.ToPaintGeometry(FVector2D(KeyWidth, 35.f), FSlateLayoutTransform(KeyPosition)),
	                           FCoreStyle::Get().GetBrush("WhiteBrush"),
	                           ESlateDrawEffect::None,
	                           Palette.HoverAccent);
	FSlateDrawElement::MakeText(Elements,
	                            ++Layer,
	                            Geometry.ToPaintGeometry(FVector2D(KeyWidth - 12.f, 25.f),
	                                                     FSlateLayoutTransform(KeyPosition + FVector2D(8.f, 5.f))),
	                            KeyText,
	                            KeyFont,
	                            ESlateDrawEffect::None,
	                            Palette.OnAccent);

	return Layer;
}

int32 MazeItemPresentation::PaintDoorFocus(const UUserWidget& Owner,
                                           const FGeometry& Geometry,
                                           FSlateWindowElementList& Elements,
                                           int32 Layer)
{
	const auto* Controller = Owner.GetOwningPlayer<AMazePlayerController>();
	const auto* Player = Controller ? Cast<AMazeCharacter>(Controller->GetPawn()) : nullptr;
	FVector Handle;
	bool bOpen = false;

	if (!Player || Controller->IsMenuOpen() || Controller->IsMapOpen() || !Player->GetFocusedDoor(Handle, bOpen))
		return Layer;

	FVector2D Anchor;

	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(Controller, Handle, Anchor, true))
		return Layer;

	const FVector2D Screen = Geometry.GetLocalSize();
	const FVector2D Size(206.f, 52.f);
	const FVector2D Position(FMath::Clamp(Anchor.X + 24.f, 12.f, Screen.X - Size.X - 12.f),
	                         FMath::Clamp(Anchor.Y - 70.f, 12.f, Screen.Y - Size.Y - 12.f));
	const auto Palette = MazeInterfaceStyle::Palette();
	const FLinearColor Accent = Palette.Accent.CopyWithNewOpacity(.95f);
	const FVector2D Elbow(Anchor.X + 15.f, Position.Y + Size.Y * .5f);
	const TArray<FVector2D> Leader = {Anchor, Elbow, Position + FVector2D(0.f, Size.Y * .5f)};

	FSlateDrawElement::MakeLines(
	    Elements, ++Layer, Geometry.ToPaintGeometry(), Leader, ESlateDrawEffect::None, Accent, true, 2.f);
	FSlateDrawElement::MakeBox(Elements,
	                           ++Layer,
	                           Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Position)),
	                           FCoreStyle::Get().GetBrush("WhiteBrush"),
	                           ESlateDrawEffect::None,
	                           Palette.BackgroundMid.CopyWithNewOpacity(.96f));
	FSlateDrawElement::MakeLines(Elements,
	                             ++Layer,
	                             Geometry.ToPaintGeometry(),
	                             RoundedRect(Position, Size, 8.f),
	                             ESlateDrawEffect::None,
	                             Accent,
	                             true,
	                             1.5f);
	FSlateDrawElement::MakeText(
	    Elements,
	    ++Layer,
	    Geometry.ToPaintGeometry(FVector2D(132.f, 28.f), FSlateLayoutTransform(Position + FVector2D(15.f, 16.f))),
	    bOpen ? NSLOCTEXT("Maze.Doors", "Close", "ЗАКРЫТЬ") : NSLOCTEXT("Maze.Doors", "Open", "ОТКРЫТЬ"),
	    MazeInterfaceStyle::Font(17, 0),
	    ESlateDrawEffect::None,
	    Palette.Ink);

	const FText KeyText = MazeKeyBindings::GetKey(TEXT("Interact")).GetDisplayName();
	const FVector2D KeyPosition = Position + FVector2D(155.f, 9.f);

	FSlateDrawElement::MakeBox(Elements,
	                           ++Layer,
	                           Geometry.ToPaintGeometry(FVector2D(40.f, 34.f), FSlateLayoutTransform(KeyPosition)),
	                           FCoreStyle::Get().GetBrush("WhiteBrush"),
	                           ESlateDrawEffect::None,
	                           Palette.HoverAccent);
	FSlateDrawElement::MakeText(
	    Elements,
	    ++Layer,
	    Geometry.ToPaintGeometry(FVector2D(32.f, 25.f), FSlateLayoutTransform(KeyPosition + FVector2D(8.f, 5.f))),
	    KeyText,
	    MazeInterfaceStyle::Font(17, 0),
	    ESlateDrawEffect::None,
	    Palette.OnAccent);

	return Layer;
}
