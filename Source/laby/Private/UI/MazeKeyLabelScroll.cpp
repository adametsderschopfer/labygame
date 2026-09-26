#include "UI/MazeWidgets.h"
#include "UI/MazeInterfaceStyle.h"
#include "Player/MazeKeyBindings.h"
#include "Components/InputKeySelector.h"
#include "Components/WidgetSwitcher.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"

namespace
{
	constexpr float LabelPadding = 12.f;
	constexpr float ScrollSpeed = 32.f;
	constexpr float EdgePause = 1.f;

	FText KeyLabel(const UInputKeySelector& Selector)
	{
		if (Selector.GetIsSelectingKey())
			return Selector.GetKeySelectionText();

		const FInputChord& Chord = Selector.GetSelectedKey();

		if (!Chord.Key.IsValid())
			return Selector.GetNoKeySpecifiedText();

		return Chord.Key.IsModifierKey() ? Chord.Key.GetDisplayName() : Chord.GetInputText();
	}

	bool ControlsVisible(const UUserWidget& Menu)
	{
		const auto* Pages = Cast<UWidgetSwitcher>(Menu.GetWidgetFromName(TEXT("SettingsPages")));

		return Pages && Pages->GetActiveWidgetIndex() == 1;
	}

	int32 PaintAmbientBackground(const FGeometry& Geometry,
	                             FSlateWindowElementList& Elements,
	                             int32 Layer,
	                             const FLinearColor& Tint)
	{
		const FVector2D Size = Geometry.GetLocalSize();

		if (Size.X <= 0.f || Size.Y <= 0.f)
			return Layer;

		const auto Resource =
		    FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FCoreStyle::Get().GetBrush("WhiteBrush"));
		TArray<FSlateVertex> Vertices;
		TArray<SlateIndex> Indices;

		Vertices.Reserve(128);
		Indices.Reserve(384);

		const auto Vertex = [&](FVector2D Position, FVector2D UV, FLinearColor Color)
		{
			return FSlateVertex::Make<ESlateVertexRounding::Disabled>(Geometry.GetAccumulatedRenderTransform(),
			                                                          FVector2f(Position),
			                                                          FVector2f(UV),
			                                                          (Color * Tint).ToFColorSRGB());
		};
		const auto Quad =
		    [&](FLinearColor TopLeft, FLinearColor TopRight, FLinearColor BottomRight, FLinearColor BottomLeft)
		{
			const SlateIndex Base = Vertices.Num();

			Vertices.Add(Vertex({0, 0}, {0, 0}, TopLeft));
			Vertices.Add(Vertex({Size.X, 0}, {1, 0}, TopRight));
			Vertices.Add(Vertex(Size, {1, 1}, BottomRight));
			Vertices.Add(Vertex({0, Size.Y}, {0, 1}, BottomLeft));
			Indices.Append(
			    {Base, SlateIndex(Base + 1), SlateIndex(Base + 2), Base, SlateIndex(Base + 2), SlateIndex(Base + 3)});
		};
		const auto Glow = [&](FVector2D Center, FVector2D Radius, FLinearColor Color)
		{
			constexpr int32 Segments = 48;
			const SlateIndex Base = Vertices.Num();

			Vertices.Add(Vertex(Center, {0.5f, 0.5f}, Color));

			for (int32 Segment = 0; Segment <= Segments; ++Segment)
			{
				const double Angle = 2.0 * PI * Segment / Segments;
				const FVector2D Unit(FMath::Cos(Angle), FMath::Sin(Angle));

				Vertices.Add(Vertex(Center + Unit * Radius, Unit * 0.5 + FVector2D(0.5), FLinearColor::Transparent));

				if (Segment > 0)
					Indices.Append({Base, SlateIndex(Base + Segment), SlateIndex(Base + Segment + 1)});
			}
		};

		const auto Palette = MazeInterfaceStyle::Palette();

		Quad(Palette.BackgroundMid, Palette.BackgroundHigh, Palette.Glass, Palette.BackgroundLow);

		const double Time = FPlatformTime::Seconds();
		const FVector2D UpperCenter(Size.X * (0.78 + FMath::Sin(Time * 0.035) * 0.035),
		                            Size.Y * (0.18 + FMath::Cos(Time * 0.028) * 0.04));
		const FVector2D LowerCenter(Size.X * (0.58 + FMath::Cos(Time * 0.024) * 0.045),
		                            Size.Y * (0.92 + FMath::Sin(Time * 0.03) * 0.035));
		const FVector2D MiddleCenter(Size.X * (0.50 + FMath::Sin(Time * 0.018) * 0.03), Size.Y * 0.52);

		Glow(UpperCenter, {Size.X * 0.53, Size.Y * 0.78}, Palette.Edge.CopyWithNewOpacity(0.10f));
		Glow(LowerCenter, {Size.X * 0.50, Size.Y * 0.58}, Palette.Line.CopyWithNewOpacity(0.08f));
		Glow(MiddleCenter, {Size.X * 0.34, Size.Y * 0.62}, Palette.HoverSurface.CopyWithNewOpacity(0.05f));

		FSlateDrawElement::MakeCustomVerts(Elements, Layer, Resource, Vertices, Indices, nullptr, 0, 0);

		return Layer + 1;
	}

	class SMazeAmbientBackground : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SMazeAmbientBackground)
		{
		}
		SLATE_END_ARGS()

		void Construct(const FArguments&)
		{
		}

		virtual FVector2D ComputeDesiredSize(float) const override
		{
			return FVector2D(1920.f, 1080.f);
		}

		virtual int32 OnPaint(const FPaintArgs&,
		                      const FGeometry& Geometry,
		                      const FSlateRect&,
		                      FSlateWindowElementList& Elements,
		                      int32 Layer,
		                      const FWidgetStyle& Style,
		                      bool) const override
		{
			return PaintAmbientBackground(Geometry, Elements, Layer, Style.GetColorAndOpacityTint());
		}
	};
}

TSharedRef<SWidget> UMazeMenuWidget::RebuildWidget()
{
	return SNew(SOverlay) + SOverlay::Slot()[SNew(SMazeAmbientBackground)] + SOverlay::Slot()[Super::RebuildWidget()];
}

void UMazeMenuWidget::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
	Super::NativeTick(Geometry, DeltaSeconds);

	if (const auto Widget = GetCachedWidget())
		Widget->Invalidate(EInvalidateWidgetReason::Paint);

	UpdateAudioSettings();

	if (!ControlsVisible(*this))
	{
		KeyLabelScroll.Reset();

		return;
	}

	for (const auto& Binding : MazeKeyBindings::Definitions())
		if (const auto* Selector = Cast<UInputKeySelector>(GetWidgetFromName(Binding.WidgetName())))
		{
			auto& Scroll = KeyLabelScroll.FindOrAdd(Binding.Id);
			const FString Text = KeyLabel(*Selector).ToString();
			const float Width = Selector->GetCachedGeometry().GetLocalSize().X;

			if (Scroll.Text != Text || !FMath::IsNearlyEqual(Scroll.Width, Width))
			{
				Scroll.Text = Text;
				Scroll.Width = Width;
				Scroll.Elapsed = 0.f;
			}
			else
				Scroll.Elapsed += DeltaSeconds;
		}
}

int32 UMazeMenuWidget::NativePaint(const FPaintArgs& Args,
                                   const FGeometry& Geometry,
                                   const FSlateRect& CullingRect,
                                   FSlateWindowElementList& Elements,
                                   int32 Layer,
                                   const FWidgetStyle& Style,
                                   bool bParentEnabled) const
{
	const int32 LabelLayer =
	    Super::NativePaint(Args, Geometry, CullingRect, Elements, Layer, Style, bParentEnabled) + 1;

	if (!ControlsVisible(*this))
		return LabelLayer;

	const auto Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	for (const auto& Binding : MazeKeyBindings::Definitions())
		if (const auto* Selector = Cast<UInputKeySelector>(GetWidgetFromName(Binding.WidgetName())))
		{
			// Use this paint pass, not last tick's desktop-space geometry (ScaleBox/DPI/captures).
			const FGeometry& KeyGeometry = Selector->GetPaintSpaceGeometry();
			const FVector2D Size = KeyGeometry.GetLocalSize();
			const float Width = Size.X - 2.f * LabelPadding;

			if (Width <= 0.f || Size.Y <= 0.f)
				continue;

			const FText Text = KeyLabel(*Selector);
			const FTextBlockStyle& TextStyle = Selector->GetTextStyle();
			const FVector2D TextSize = Measure->Measure(Text, TextStyle.Font);
			const float Overflow = FMath::Max(0.f, float(TextSize.X) - Width);
			float X = LabelPadding + (Width - TextSize.X) * 0.5f;

			if (Overflow > 0.f)
			{
				const auto* Scroll = KeyLabelScroll.Find(Binding.Id);
				const float TravelTime = Overflow / ScrollSpeed;
				const float Phase = FMath::Fmod(Scroll ? Scroll->Elapsed : 0.f, 2.f * (TravelTime + EdgePause));
				const float Offset =
				    Phase <= TravelTime + EdgePause
				        ? FMath::Clamp((Phase - EdgePause) * ScrollSpeed, 0.f, Overflow)
				        : Overflow - FMath::Clamp((Phase - TravelTime - 2.f * EdgePause) * ScrollSpeed, 0.f, Overflow);
				X = LabelPadding - Offset;
			}

			Elements.PushClip(FSlateClippingZone(
			    KeyGeometry.MakeChild(FVector2D(Width, Size.Y), FSlateLayoutTransform(FVector2D(LabelPadding, 0.f)))));
			FSlateDrawElement::MakeText(
			    Elements,
			    LabelLayer,
			    KeyGeometry.ToPaintGeometry(TextSize,
			                                FSlateLayoutTransform(FVector2D(X, (Size.Y - TextSize.Y) * 0.5f))),
			    Text,
			    TextStyle.Font,
			    ESlateDrawEffect::None,
			    TextStyle.ColorAndOpacity.GetColor(Style) * Style.GetColorAndOpacityTint());
			Elements.PopClip();
		}

	return LabelLayer;
}
