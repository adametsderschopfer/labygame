#include "UI/MazeInterfaceStyle.h"
#include "World/MazePreparationStatus.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeLock.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	FText StageTitle(EMazePreparationStage Stage)
	{
		switch (Stage)
		{
		case EMazePreparationStage::Map:
			return NSLOCTEXT("Maze.Loading", "MapStage", "ЗАГРУЗКА УРОВНЯ");

		case EMazePreparationStage::Topology:
			return NSLOCTEXT("Maze.Loading", "TopologyStage", "СОЗДАНИЕ ЛАБИРИНТА");

		case EMazePreparationStage::Collision:
			return NSLOCTEXT("Maze.Loading", "CollisionStage", "ПОДГОТОВКА ФИЗИКИ");

		case EMazePreparationStage::Assets:
			return NSLOCTEXT("Maze.Loading", "AssetsStage", "ЗАГРУЗКА РЕСУРСОВ");

		case EMazePreparationStage::Geometry:
			return NSLOCTEXT("Maze.Loading", "GeometryStage", "ПОДГОТОВКА ОКРУЖЕНИЯ");

		case EMazePreparationStage::WorldStreaming:
			return NSLOCTEXT("Maze.Loading", "StreamingStage", "ЗАГРУЗКА УЧАСТКОВ МИРА");

		case EMazePreparationStage::Shaders:
			return NSLOCTEXT("Maze.Loading", "ShadersStage", "ПОДГОТОВКА ШЕЙДЕРОВ");

		default:
			return NSLOCTEXT("Maze.Loading", "FinalizingStage", "ПОДГОТОВКА ПЕРВОГО КАДРА");
		}
	}

	FText StageDetail(const FMazePreparationStatus& Status)
	{
		switch (Status.Stage)
		{
		case EMazePreparationStage::Map:
			return NSLOCTEXT("Maze.Loading", "MapDetail", "Читаем данные уровня и загружаем его объекты.");

		case EMazePreparationStage::Topology:
			return NSLOCTEXT("Maze.Loading", "TopologyDetail", "Создаём комнаты и проходы.");

		case EMazePreparationStage::Collision:
			return NSLOCTEXT("Maze.Loading", "CollisionDetail", "Готовим стены и пол для перемещения игрока.");

		case EMazePreparationStage::Assets:
			return Status.Total > 0
			           ? FText::Format(
			                 NSLOCTEXT("Maze.Loading", "AssetsCount", "Модели, материалы и звуки: {0} из {1}"),
			                 FText::AsNumber(Status.Completed),
			                 FText::AsNumber(Status.Total))
			           : NSLOCTEXT("Maze.Loading", "AssetsDetail", "Загружаем модели, материалы и звуки.");

		case EMazePreparationStage::Geometry:
			return Status.Total > 0
			           ? FText::Format(
			                 NSLOCTEXT("Maze.Loading", "GeometryCount", "Участки рядом с игроком: {0} из {1}"),
			                 FText::AsNumber(Status.Completed),
			                 FText::AsNumber(Status.Total))
			           : NSLOCTEXT("Maze.Loading", "GeometryDetail", "Ожидаем данные лабиринта и готовим окружение.");

		case EMazePreparationStage::WorldStreaming:
			return NSLOCTEXT("Maze.Loading", "StreamingDetail", "Подгружаем окружение вокруг места появления.");

		case EMazePreparationStage::Shaders:
			return FText::Format(NSLOCTEXT("Maze.Loading", "ShadersCount", "Осталось задач подготовки графики: {0}"),
			                     FText::AsNumber(Status.PendingPSOs));

		default:
			return NSLOCTEXT("Maze.Loading", "FinalizingDetail", "Завершаем подготовку изображения.");
		}
	}

	// MoviePlayer can paint on its loading thread while the game thread is blocked.
	// Only copied values cross that boundary; no UObject, world or fragment bindings.
	class SMazeLoadingScreen final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SMazeLoadingScreen) : _StartedAt(0)
		{
		}
		SLATE_ARGUMENT(double, StartedAt)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			using namespace MazeInterfaceStyle;
			StartedAt = Args._StartedAt > 0 ? Args._StartedAt : FPlatformTime::Seconds();
			ChildSlot
			    [SNew(SBorder)
			         .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			         .BorderBackgroundColor(FLinearColor(0.002f, 0.006f, 0.004f))
			         .HAlign(HAlign_Center)
			         .VAlign(VAlign_Center)[SNew(SBox).WidthOverride(
			             600)[SNew(SVerticalBox) +
			                  SVerticalBox::Slot().AutoHeight().Padding(
			                      0, 0, 0, 18)[SNew(STextBlock)
			                                       .Text(NSLOCTEXT("Maze.Glass", "Brand", "LABY"))
			                                       .Font(Font(44, 500))
			                                       .ColorAndOpacity(Ink)] +
			                  SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 48)[SNew(SBox).HeightOverride(
			                      1)[SNew(SBorder)
			                             .Padding(0)
			                             .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			                             .BorderBackgroundColor(Line)]] +
			                  SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 16)[SNew(STextBlock)
			                                                                             .Text_Lambda(
			                                                                                 [this]
			                                                                                 {
				                                                                                 return StageTitle(
				                                                                                     Read().Stage);
			                                                                                 })
			                                                                             .Font(Font(18, 100))
			                                                                             .ColorAndOpacity(Ink)
			                                                                             .AutoWrapText(true)] +
			                  SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 20)[SNew(STextBlock)
			                                                                             .Text_Lambda(
			                                                                                 [this]
			                                                                                 {
				                                                                                 return StageDetail(
				                                                                                     Read());
			                                                                                 })
			                                                                             .Font(Font(14, 0))
			                                                                             .ColorAndOpacity(Ink)
			                                                                             .AutoWrapText(true)] +
			                  SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 16)[SNew(SBox).HeightOverride(
			                      4)[SNew(SProgressBar)
			                             .FillColorAndOpacity(Accent)
			                             .Percent_Lambda(
			                                 [this]() -> TOptional<float>
			                                 {
				                                 const auto Value = Read();

				                                 return Value.Total > 0
				                                            ? TOptional<float>(FMath::Clamp(
				                                                  float(Value.Completed) / Value.Total, 0.f, 1.f))
				                                            : TOptional<float>();
			                                 })]] +
			                  SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
			                      [SNew(STextBlock)
			                           .Text_Lambda(
			                               [this]
			                               {
				                               const auto Value = Read();

				                               return Value.PendingPSOs > 0 &&
				                                              Value.Stage != EMazePreparationStage::Shaders
				                                          ? FText::Format(
				                                                NSLOCTEXT("Maze.Loading",
				                                                          "ParallelShaders",
				                                                          "Также готовим шейдеры. Осталось задач: {0}"),
				                                                FText::AsNumber(Value.PendingPSOs))
				                                          : FText::GetEmpty();
			                               })
			                           .Font(Font(12, 0))
			                           .ColorAndOpacity(Muted)
			                           .AutoWrapText(true)] +
			                  SVerticalBox::Slot().AutoHeight().Padding(
			                      0, 0, 0, 20)[SNew(STextBlock)
			                                       .Text(NSLOCTEXT("Maze.Loading",
			                                                       "FirstLaunchHint",
			                                                       "При первом запуске и после обновления драйвера "
			                                                       "подготовка графики может занять больше времени."))
			                                       .Font(Font(12, 0))
			                                       .ColorAndOpacity(Muted)
			                                       .AutoWrapText(true)] +
			                  SVerticalBox::Slot().AutoHeight()
			                      [SNew(SHorizontalBox) +
			                       SHorizontalBox::Slot()
			                           .AutoWidth()
			                           .VAlign(VAlign_Center)
			                           .Padding(0, 0, 16, 0)[SNew(SThrobber).NumPieces(3)] +
			                       SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock)
			                                                              .Text_Lambda(
			                                                                  [this]
			                                                                  {
				                                                                  return FText::Format(
				                                                                      NSLOCTEXT("Maze.Loading",
				                                                                                "Elapsed",
				                                                                                "Прошло: {0} с"),
				                                                                      FText::AsNumber(FMath::FloorToInt(
				                                                                          FPlatformTime::Seconds() -
				                                                                          StartedAt)));
			                                                                  })
			                                                              .Font(Font(12, 0))
			                                                              .ColorAndOpacity(Muted)]]]]];
		}

		double GetStartedAt() const
		{
			return StartedAt;
		}

		void SetStatus(const FMazePreparationStatus& Value)
		{
			FScopeLock Guard(&Mutex);

			Status = Value;
		}

	private:
		FMazePreparationStatus Read() const
		{
			FScopeLock Guard(&Mutex);

			return Status;
		}

		mutable FCriticalSection Mutex;
		FMazePreparationStatus Status;
		double StartedAt = 0;
	};
}

TSharedRef<SWidget> MazeInterfaceStyle::MakeLoadingScreen(const TSharedPtr<SWidget>& Previous)
{
	// MoviePlayer may still own its widget during PostLoadMap: never give it two Slate parents.
	const double StartedAt = Previous ? StaticCastSharedPtr<SMazeLoadingScreen>(Previous)->GetStartedAt() : 0;

	return SNew(SMazeLoadingScreen).StartedAt(StartedAt);
}

void MazeInterfaceStyle::UpdateLoadingScreen(const TSharedRef<SWidget>& Screen, const FMazePreparationStatus& Status)
{
	StaticCastSharedRef<SMazeLoadingScreen>(Screen)->SetStatus(Status);
}
