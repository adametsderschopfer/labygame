#include "Player/MazePlayerController.h"

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
#include "MazeDevelopmentUI.h"
#include "Player/MazeCharacter.h"
#include "ECS/MazeECSSubsystem.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "World/MazeLocationSubsystem.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	class SMazeDevelopmentPanel : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SMazeDevelopmentPanel)
		{
		}
		SLATE_EVENT(FSimpleDelegate, Close)
		SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_END_ARGS()
		void Construct(const FArguments& Args)
		{
			Close = Args._Close;
			ChildSlot[Args._Content.Widget];
		}

		virtual bool SupportsKeyboardFocus() const override
		{
			return true;
		}

		virtual FReply OnPreviewKeyDown(const FGeometry&, const FKeyEvent& Event) override
		{
			if (Event.GetKey() == EKeys::Backslash || Event.GetKey() == EKeys::Escape)
			{
				if (!Event.IsRepeat())
					Close.ExecuteIfBound();

				return FReply::Handled().ReleaseMouseCapture();
			}

			return FReply::Unhandled();
		}

	private:
		FSimpleDelegate Close;
	};
}

void AMazePlayerController::ToggleDevelopmentMenu()
{
	if (IsDevelopmentMenuOpen())
	{
		CloseDevelopmentMenu();

		return;
	}

	if (!IsLocalController() || !ECSSubsystem || IsMenuOpen() || !ReadSession().bSessionStarted)
		return;

	auto* Viewport = GetWorld()->GetGameViewport();

	if (!Viewport)
		return;

	EnsureDevelopmentPresentation();

	if (IsMapOpen())
		ToggleMap();

	if (auto* MazePawn = Cast<AMazeCharacter>(GetPawn()))
		MazePawn->ClearLocalInput();

	FlushPressedKeys();
	ResetIgnoreMoveInput();
	ResetIgnoreLookInput();
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
	bShowMouseCursor = true;

	const TWeakObjectPtr<AMazePlayerController> WeakThis(this);
	const auto Close = [WeakThis]()
	{
		if (auto* PC = WeakThis.Get())
			PC->CloseDevelopmentMenu();
	};
	const auto Label = [](const TCHAR* En, const TCHAR* Ru, const TCHAR* Es)
	{
		return SNew(STextBlock).Text(MazeDevelopmentText(En, Ru, Es));
	};
	const TSharedRef<SVerticalBox> Items = SNew(SVerticalBox);

	Items->AddSlot().AutoHeight().Padding(
	    0, 0, 0, 12)[Label(TEXT("Development menu"), TEXT("Меню разработчика"), TEXT("Menú de desarrollo"))];
	Items->AddSlot().AutoHeight().Padding(0, 4)[SNew(STextBlock)
	                                                .Text_Lambda(
	                                                    [WeakThis]()
	                                                    {
		                                                    const auto* PC = WeakThis.Get();

		                                                    return PC && PC->DevelopmentPresentation
		                                                               ? PC->DevelopmentPresentation->Seed
		                                                               : FText::GetEmpty();
	                                                    })];
	Items->AddSlot().AutoHeight().Padding(0, 4)[SNew(SButton).OnClicked_Lambda(
	    [WeakThis]()
	    {
		    if (auto* PC = WeakThis.Get())
		    {
			    const auto Maze = PC->ECSSubsystem->ReadMaze(PC->ReadSession().Maze);

			    if (Maze.Data)
			    {
				    FPlatformApplicationMisc::ClipboardCopy(*LexToString(Maze.Seed));
				    PC->DevelopmentPresentation->Status =
				        MazeDevelopmentText(TEXT("Seed copied"), TEXT("Seed скопирован"), TEXT("Semilla copiada"));
			    }
		    }

		    return FReply::Handled();
	    })[Label(TEXT("Copy seed"), TEXT("Скопировать seed"), TEXT("Copiar semilla"))]];
	Items->AddSlot().AutoHeight().Padding(
	    0, 4)[SNew(SButton)
	              .IsEnabled(GetNetMode() == NM_Standalone)
	              .OnClicked_Lambda(
	                  [WeakThis]()
	                  {
		                  if (auto* PC = WeakThis.Get())
		                  {
			                  PC->CloseDevelopmentMenu();
			                  PC->StartNewGame();
		                  }

		                  return FReply::Handled();
	                  })[Label(TEXT("Regenerate level"), TEXT("Пересоздать уровень"), TEXT("Regenerar nivel"))]];
	Items->AddSlot().AutoHeight().Padding(0, 4)[SNew(SButton)
	                                                .IsEnabled(GetNetMode() == NM_Standalone)
	                                                .OnClicked_Lambda(
	                                                    [WeakThis]()
	                                                    {
		                                                    if (auto* PC = WeakThis.Get())
			                                                    PC->DevelopmentRestartSeed();

		                                                    return FReply::Handled();
	                                                    })[Label(TEXT("Regenerate with same seed"),
	                                                             TEXT("Пересоздать с тем же seed"),
	                                                             TEXT("Regenerar con la misma semilla"))]];

	const auto CanTeleport = [WeakThis]()
	{
		const auto* PC = WeakThis.Get();
		const auto* Location = PC ? PC->GetWorld()->GetSubsystem<UMazeLocationSubsystem>() : nullptr;

		return PC && PC->GetNetMode() == NM_Standalone && Location && Location->IsReady();
	};

	Items->AddSlot().AutoHeight().Padding(0, 4)
	    [SNew(SButton)
	         .IsEnabled_Lambda(CanTeleport)
	         .OnClicked_Lambda(
	             [WeakThis]()
	             {
		             if (auto* PC = WeakThis.Get())
			             PC->DevelopmentTeleport(false);

		             return FReply::Handled();
	             })[Label(TEXT("Teleport to start"), TEXT("Телепорт к старту"), TEXT("Teletransportarse al inicio"))]];
	Items->AddSlot().AutoHeight().Padding(0, 4)
	    [SNew(SButton)
	         .IsEnabled_Lambda(CanTeleport)
	         .OnClicked_Lambda(
	             [WeakThis]()
	             {
		             if (auto* PC = WeakThis.Get())
			             PC->DevelopmentTeleport(true);

		             return FReply::Handled();
	             })[Label(TEXT("Teleport to exit"), TEXT("Телепорт к выходу"), TEXT("Teletransportarse a la salida"))]];
	Items->AddSlot().AutoHeight().Padding(
	    0, 6)[SNew(SCheckBox)
	              .IsChecked_Lambda(
	                  [WeakThis]()
	                  {
		                  const auto* PC = WeakThis.Get();

		                  return PC && PC->bDevelopmentRevealMap ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	                  })
	              .OnCheckStateChanged_Lambda(
	                  [WeakThis](ECheckBoxState State)
	                  {
		                  if (auto* PC = WeakThis.Get())
			                  PC->bDevelopmentRevealMap = State == ECheckBoxState::Checked;
	                  })[Label(TEXT("Show entire map"), TEXT("Показать всю карту"), TEXT("Mostrar todo el mapa"))]];
	Items->AddSlot().AutoHeight().Padding(
	    0, 6)[SNew(SCheckBox)
	              .IsChecked_Lambda(
	                  [WeakThis]()
	                  {
		                  const auto* PC = WeakThis.Get();

		                  return PC && PC->bDevelopmentShowRoute ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	                  })
	              .OnCheckStateChanged_Lambda(
	                  [WeakThis](ECheckBoxState State)
	                  {
		                  if (auto* PC = WeakThis.Get())
			                  PC->bDevelopmentShowRoute = State == ECheckBoxState::Checked;
	                  })[Label(TEXT("Route to exit"), TEXT("Маршрут до выхода"), TEXT("Ruta a la salida"))]];
	Items->AddSlot().AutoHeight().Padding(
	    0,
	    6)[SNew(SCheckBox)
	           .IsEnabled(GetNetMode() == NM_Standalone)
	           .IsChecked_Lambda(
	               [WeakThis]()
	               {
		               const auto* PC = WeakThis.Get();
		               const auto* MazePawn = PC ? Cast<AMazeCharacter>(PC->GetPawn()) : nullptr;

		               return MazePawn && MazePawn->GetVitals().bInfiniteStamina ? ECheckBoxState::Checked
		                                                                         : ECheckBoxState::Unchecked;
	               })
	           .OnCheckStateChanged_Lambda(
	               [WeakThis](ECheckBoxState State)
	               {
		               if (auto* PC = WeakThis.Get())
			               if (const auto* MazePawn = Cast<AMazeCharacter>(PC->GetPawn()))
				               PC->ECSSubsystem->SetDevelopmentInfiniteStamina(MazePawn->GetPlayerEntity(),
				                                                               State == ECheckBoxState::Checked);
	               })[Label(TEXT("Infinite stamina"), TEXT("Бесконечная выносливость"), TEXT("Resistencia infinita"))]];
	Items->AddSlot().AutoHeight().Padding(
	    0, 6)[SNew(SCheckBox)
	              .IsEnabled(GetNetMode() == NM_Standalone)
	              .IsChecked_Lambda(
	                  [WeakThis]()
	                  {
		                  const auto* PC = WeakThis.Get();
		                  const auto* Pawn = PC ? Cast<AMazeCharacter>(PC->GetPawn()) : nullptr;

		                  return Pawn && Pawn->GetVitals().bDevelopmentImmortal ? ECheckBoxState::Checked
		                                                                        : ECheckBoxState::Unchecked;
	                  })
	              .OnCheckStateChanged_Lambda(
	                  [WeakThis](ECheckBoxState State)
	                  {
		                  if (auto* PC = WeakThis.Get())
			                  if (const auto* Pawn = Cast<AMazeCharacter>(PC->GetPawn()))
				                  PC->ECSSubsystem->SetDevelopmentImmortal(Pawn->GetPlayerEntity(),
				                                                           State == ECheckBoxState::Checked);
	                  })[Label(TEXT("Invulnerability"), TEXT("Бессмертие"), TEXT("Invulnerabilidad"))]];
	Items->AddSlot().AutoHeight().Padding(0, 6)[SNew(SCheckBox)
	                                                .IsChecked_Lambda(
	                                                    [WeakThis]()
	                                                    {
		                                                    const auto* PC = WeakThis.Get();

		                                                    return PC && PC->DevelopmentPresentation->bShowPosition
		                                                               ? ECheckBoxState::Checked
		                                                               : ECheckBoxState::Unchecked;
	                                                    })
	                                                .OnCheckStateChanged_Lambda(
	                                                    [WeakThis](ECheckBoxState State)
	                                                    {
		                                                    if (auto* PC = WeakThis.Get())
		                                                    {
			                                                    PC->DevelopmentPresentation->bShowPosition =
			                                                        State == ECheckBoxState::Checked;
			                                                    PC->DevelopmentPresentation->RefreshWait = 0;
			                                                    PC->RefreshDevelopmentPresentation(0);
		                                                    }
	                                                    })[Label(TEXT("Coordinates, cell and room"),
	                                                             TEXT("Координаты, клетка и помещение"),
	                                                             TEXT("Coordenadas, celda y sala"))]];
	Items->AddSlot().AutoHeight().Padding(
	    0, 6)[SNew(SCheckBox)
	              .IsChecked_Lambda(
	                  [WeakThis]()
	                  {
		                  const auto* PC = WeakThis.Get();
		                  const auto* Viewport = PC ? PC->GetWorld()->GetGameViewport() : nullptr;

		                  return Viewport && Viewport->EngineShowFlags.Collision ? ECheckBoxState::Checked
		                                                                         : ECheckBoxState::Unchecked;
	                  })
	              .OnCheckStateChanged_Lambda(
	                  [WeakThis](ECheckBoxState State)
	                  {
		                  if (auto* PC = WeakThis.Get())
			                  PC->SetDevelopmentCollision(State == ECheckBoxState::Checked);
	                  })[Label(TEXT("Show collision"), TEXT("Показать коллизии"), TEXT("Mostrar colisiones"))]];
	Items->AddSlot().AutoHeight().Padding(
	    0,
	    6)[SNew(SCheckBox)
	           .IsChecked_Lambda(
	               [WeakThis]()
	               {
		               const auto* PC = WeakThis.Get();

		               return PC && PC->DevelopmentPresentation->bShowResources ? ECheckBoxState::Checked
		                                                                        : ECheckBoxState::Unchecked;
	               })
	           .OnCheckStateChanged_Lambda(
	               [WeakThis](ECheckBoxState State)
	               {
		               if (auto* PC = WeakThis.Get())
		               {
			               PC->DevelopmentPresentation->bShowResources = State == ECheckBoxState::Checked;
			               PC->DevelopmentPresentation->RefreshWait = 0;
			               PC->RefreshDevelopmentPresentation(0);
		               }
	               })[Label(TEXT("Loading diagnostics"), TEXT("Диагностика загрузки"), TEXT("Diagnóstico de carga"))]];
	Items->AddSlot().AutoHeight().Padding(0, 8)[SNew(STextBlock)
	                                                .AutoWrapText(true)
	                                                .Text_Lambda(
	                                                    [WeakThis]()
	                                                    {
		                                                    const auto* PC = WeakThis.Get();

		                                                    return PC && PC->DevelopmentPresentation
		                                                               ? PC->DevelopmentPresentation->Status
		                                                               : FText::GetEmpty();
	                                                    })];
	Items->AddSlot().AutoHeight().Padding(0, 12, 0, 0)[SNew(SButton).OnClicked_Lambda(
	    [Close]()
	    {
		    Close();

		    return FReply::Handled();
	    })[Label(TEXT("Close (\\ / Esc)"), TEXT("Закрыть (\\ / Esc)"), TEXT("Cerrar (\\ / Esc)"))]];
	DevelopmentMenu =
	    SNew(SMazeDevelopmentPanel)
	        .Close(FSimpleDelegate::CreateLambda(Close))
	            [SNew(SConstraintCanvas) +
	             SConstraintCanvas::Slot()
	                 .Anchors(FAnchors(1, 0))
	                 .Alignment(FVector2D(1, 0))
	                 .Offset(FMargin(-24, 24, 380, 0))
	                 .AutoSize(true)[SNew(SBox).WidthOverride(380).MaxDesiredHeight_Lambda(
	                     [WeakThis]()
	                     {
		                     int32 Width = 0, Height = 0;

		                     if (const auto* PC = WeakThis.Get())
			                     PC->GetViewportSize(Width, Height);

		                     return float(FMath::Max(100, Height * 3 / 4));
	                     })[SNew(SBorder).Padding(18).BorderBackgroundColor(FLinearColor(0.04f, 0.05f, 0.04f, 0.96f))
	                            [SNew(SScrollBox) + SScrollBox::Slot()[Items]]]]];
	Viewport->AddViewportWidgetContent(DevelopmentMenu.ToSharedRef(), 150);
	RefreshDevelopmentPresentation(0);

	FInputModeUIOnly Mode;

	Mode.SetWidgetToFocus(DevelopmentMenu);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
}

void AMazePlayerController::CloseDevelopmentMenu()
{
	if (!IsDevelopmentMenuOpen())
		return;

	if (auto* Viewport = GetWorld()->GetGameViewport())
		Viewport->RemoveViewportWidgetContent(DevelopmentMenu.ToSharedRef());

	DevelopmentMenu.Reset();

	if (auto* MazePawn = Cast<AMazeCharacter>(GetPawn()))
		MazePawn->ClearLocalInput();

	FlushPressedKeys();
	ResetIgnoreMoveInput();
	ResetIgnoreLookInput();
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
}

#endif
