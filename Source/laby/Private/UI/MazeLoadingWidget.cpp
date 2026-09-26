#include "UI/MazeInterfaceStyle.h"
#include "World/MazePreparationStatus.h"
#include "Misc/ScopeLock.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	FText StageTitle(EMazePreparationStage Stage)
	{
		switch (Stage)
		{
		case EMazePreparationStage::Map:
			return NSLOCTEXT("Maze.Loading", "MapStage", "Загрузка уровня…");

		case EMazePreparationStage::Topology:
			return NSLOCTEXT("Maze.Loading", "TopologyStage", "Создание лабиринта…");

		case EMazePreparationStage::Collision:
			return NSLOCTEXT("Maze.Loading", "CollisionStage", "Подготовка пространства…");

		case EMazePreparationStage::Assets:
			return NSLOCTEXT("Maze.Loading", "AssetsStage", "Загрузка ресурсов…");

		case EMazePreparationStage::Geometry:
		case EMazePreparationStage::WorldStreaming:
			return NSLOCTEXT("Maze.Loading", "GeometryStage", "Подготовка окружения…");

		case EMazePreparationStage::Shaders:
			return NSLOCTEXT("Maze.Loading", "ShadersStage", "Подготовка графики…");

		default:
			return NSLOCTEXT("Maze.Loading", "FinalizingStage", "Завершение загрузки…");
		}
	}

	// MoviePlayer may paint on its loading thread. Only detached copied values
	// cross this boundary; no UObject, world, gameplay state or timed progress.
	class SMazeLoadingScreen final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SMazeLoadingScreen)
		{
		}
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			ChildSlot.HAlign(HAlign_Center)
			    .VAlign(VAlign_Center)[SNew(SBox).WidthOverride(
			        480)[SNew(SVerticalBox) +
			             SVerticalBox::Slot().AutoHeight().Padding(
			                 0, 0, 0, 28)[SNew(STextBlock)
			                                  .Text(FText::AsCultureInvariant(TEXT("LABY")))
			                                  .Font(MazeInterfaceStyle::Font(36, 450))
			                                  .Justification(ETextJustify::Center)
			                                  .ColorAndOpacity(MazeInterfaceStyle::Palette().Accent)] +
			             SVerticalBox::Slot().AutoHeight()[SNew(STextBlock)
			                                                   .Text_Lambda(
			                                                       [this]
			                                                       {
				                                                       return StageTitle(Read().Stage);
			                                                       })
			                                                   .Font(MazeInterfaceStyle::Font(16, 0))
			                                                   .Justification(ETextJustify::Center)
			                                                   .ColorAndOpacity(MazeInterfaceStyle::Palette().Ink)]]];
		}

		virtual int32 OnPaint(const FPaintArgs& Args,
		                      const FGeometry& Geometry,
		                      const FSlateRect& CullingRect,
		                      FSlateWindowElementList& Elements,
		                      int32 Layer,
		                      const FWidgetStyle& Style,
		                      bool bParentEnabled) const override
		{
			const float Height = Geometry.GetLocalSize().Y;
			const auto Palette = MazeInterfaceStyle::Palette();
			const TArray<FSlateGradientStop> Stops = {
			    FSlateGradientStop(FVector2f(0, 0), Palette.BackgroundMid),
			    FSlateGradientStop(FVector2f(0, Height * .45f), Palette.BackgroundHigh),
			    FSlateGradientStop(FVector2f(0, Height), Palette.BackgroundLow)};

			// Horizontal stop lines interpolate vertically in Slate.
			FSlateDrawElement::MakeGradient(Elements, Layer, Geometry.ToPaintGeometry(), Stops, Orient_Horizontal);

			return SCompoundWidget::OnPaint(Args, Geometry, CullingRect, Elements, Layer + 1, Style, bParentEnabled);
		}

		void SetStatus(const FMazePreparationStatus& Value)
		{
			FScopeLock Guard(&Mutex);

			Status = Value;
		}

		FMazePreparationStatus Read() const
		{
			FScopeLock Guard(&Mutex);

			return Status;
		}

	private:
		mutable FCriticalSection Mutex;
		FMazePreparationStatus Status;
	};
}

TSharedRef<SWidget> MazeInterfaceStyle::MakeLoadingScreen()
{
	return MakeLoadingScreen(nullptr);
}

TSharedRef<SWidget> MazeInterfaceStyle::MakeLoadingScreen(const TSharedPtr<SWidget>& Previous)
{
	// Preserve the last status during handover, but never give MoviePlayer's widget two parents.
	TSharedRef<SMazeLoadingScreen> Screen = SNew(SMazeLoadingScreen);

	if (Previous)
		Screen->SetStatus(StaticCastSharedPtr<SMazeLoadingScreen>(Previous)->Read());

	return Screen;
}

void MazeInterfaceStyle::UpdateLoadingScreen(const TSharedRef<SWidget>& Screen, const FMazePreparationStatus& Status)
{
	StaticCastSharedRef<SMazeLoadingScreen>(Screen)->SetStatus(Status);
}
