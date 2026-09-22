#include "UI/MazeWidgets.h"
#include "Player/MazeKeyBindings.h"
#include "Components/InputKeySelector.h"
#include "Components/WidgetSwitcher.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"

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
}

void UMazeMenuWidget::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
	Super::NativeTick(Geometry, DeltaSeconds);

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

	if (const auto Widget = GetCachedWidget())
		Widget->Invalidate(EInvalidateWidgetReason::Paint);
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
			const FGeometry& KeyGeometry = Selector->GetCachedGeometry();
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
