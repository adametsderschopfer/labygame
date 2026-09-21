#include "UI/MazeInterfaceStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "Rendering/DrawElements.h"
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
			return FVector2D(28, 28);
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
			const FVector2D Center = Geometry.GetLocalSize() * 0.5;
			auto Arc = [&](float Amount, float Radius, FLinearColor Color, float Width)
			{
				TArray<FVector2D> Points;
				const int32 Count = FMath::Max(2, FMath::CeilToInt(48 * Amount));

				for (int32 I = 0; I <= Count; ++I)
				{
					const float Angle = -HALF_PI + 2 * PI * Amount * I / Count;
					Points.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
				}

				FSlateDrawElement::MakeLines(Elements,
				                             ++Layer,
				                             Geometry.ToPaintGeometry(),
				                             Points,
				                             ESlateDrawEffect::None,
				                             Color * Style.GetColorAndOpacityTint(),
				                             true,
				                             Width);
			};

			Arc(1, 10, FLinearColor(0, 0, 0, 0.8f), 4);
			Arc(1, 10, MazeInterfaceStyle::Muted, 1);
			Arc(0.16f + Hover * 0.84f, 10, MazeInterfaceStyle::Accent, 2);

			if (Hover > 0.01f)
				Arc(1, 3 * Hover, MazeInterfaceStyle::Accent.CopyWithNewOpacity(Hover * 0.7f), 2);

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
