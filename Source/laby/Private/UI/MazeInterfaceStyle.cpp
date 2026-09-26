#include "UI/MazeInterfaceStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	// Owned by the game viewport; never retains widgets, a world or gameplay state.
	class SMazeCursor final : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SMazeCursor)
		{
		}
		SLATE_END_ARGS()
		void Construct(const FArguments&)
		{
			SetVisibility(EVisibility::HitTestInvisible);
		}

		virtual FVector2D ComputeDesiredSize(float) const override
		{
			return FVector2D(48, 48);
		}

		virtual void Tick(const FGeometry& Geometry, double Time, float Delta) override
		{
			SLeafWidget::Tick(Geometry, Time, Delta);

			auto& App = FSlateApplication::Get();
			const FWidgetPath Path =
			    App.LocateWindowUnderMouse(App.GetCursorPos(), App.GetInteractiveTopLevelWindows());
			bool bInteractive = false;

			for (int32 I = 0; I < Path.Widgets.Num(); ++I)
			{
				const auto& Entry = Path.Widgets[I];
				const FName Type = Entry.Widget->GetType();

				bInteractive |= Entry.Widget->IsEnabled() && (Type == TEXT("SButton") || Type == TEXT("SCheckBox") ||
				                                              Type == TEXT("SSlider") || Type == TEXT("SComboButton"));
			}

			Hover = FMath::FInterpTo(Hover, bInteractive ? 1.f : 0.f, Delta, 12.f);
		}

		virtual int32 OnPaint(const FPaintArgs&,
		                      const FGeometry& Geometry,
		                      const FSlateRect&,
		                      FSlateWindowElementList& Elements,
		                      int32 Layer,
		                      const FWidgetStyle& Style,
		                      bool) const override
		{
			// UE SlateUser centers software widgets at the pointer. Put the triangle's
			// tip at that center so the visible point, not its middle, is the click hotspot.
			const FVector2D Hotspot = Geometry.GetLocalSize() * 0.5;
			const FVector2D Corners[] = {Hotspot, Hotspot + FVector2D(3, 22), Hotspot + FVector2D(18, 14)};
			TArray<FVector2D> Outline;

			for (int32 I = 0; I < 3; ++I)
			{
				const FVector2D Corner = Corners[I];
				const FVector2D Before = Corner + (Corners[(I + 2) % 3] - Corner).GetSafeNormal() * 2.f;
				const FVector2D After = Corner + (Corners[(I + 1) % 3] - Corner).GetSafeNormal() * 2.f;

				for (int32 Step = 0; Step <= 6; ++Step)
				{
					const double T = Step / 6.;

					Outline.Add(Before * FMath::Square(1 - T) + Corner * (2 * (1 - T) * T) + After * T * T);
				}
			}

			const auto Palette = MazeInterfaceStyle::Palette();
			const FLinearColor Fill = FMath::Lerp(Palette.Ink, Palette.Accent, Hover);
			TArray<FSlateVertex> Vertices;
			TArray<SlateIndex> Indices;
			const auto AddVertex = [&](FVector2D P)
			{
				Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
				    Geometry.GetAccumulatedRenderTransform(),
				    FVector2f(P),
				    FVector2f::ZeroVector,
				    (Fill * Style.GetColorAndOpacityTint()).ToFColorSRGB()));
			};

			AddVertex((Corners[0] + Corners[1] + Corners[2]) / 3.);

			for (int32 I = 0; I < Outline.Num(); ++I)
			{
				AddVertex(Outline[I]);
				Indices.Append({0, SlateIndex(I + 1), SlateIndex((I + 1) % Outline.Num() + 1)});
			}

			const auto Resource =
			    FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FCoreStyle::Get().GetBrush("WhiteBrush"));

			const FVector2D FirstPoint = Outline[0];

			Outline.Add(FirstPoint);
			FSlateDrawElement::MakeLines(Elements,
			                             ++Layer,
			                             Geometry.ToPaintGeometry(),
			                             Outline,
			                             ESlateDrawEffect::None,
			                             Palette.OnAccent.CopyWithNewOpacity(.9f),
			                             true,
			                             3.f);
			FSlateDrawElement::MakeCustomVerts(Elements, ++Layer, Resource, Vertices, Indices, nullptr, 0, 0);
			FSlateDrawElement::MakeLines(Elements,
			                             ++Layer,
			                             Geometry.ToPaintGeometry(),
			                             Outline,
			                             ESlateDrawEffect::None,
			                             Fill.CopyWithNewOpacity(.8f),
			                             true,
			                             1.f);

			return Layer;
		}

	private:
		float Hover = 0;
	};
}

FSlateFontInfo MazeInterfaceStyle::Font(int32 Size, int32 LetterSpacing)
{
	FSlateFontInfo Result = FCoreStyle::GetDefaultFontStyle("Light", Size);

	Result.LetterSpacing = LetterSpacing;

	return Result;
}

TSharedRef<SWidget> MazeInterfaceStyle::MakeCursor()
{
	return SNew(SMazeCursor);
}
