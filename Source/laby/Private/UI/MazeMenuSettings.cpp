#include "UI/MazeWidgets.h"
#include "UI/MazeInterfaceStyle.h"
#include "UI/MazeInterfacePreferences.h"
#include "Player/MazePlayerController.h"
#include "Player/MazeCharacter.h"
#include "Player/MazeKeyBindings.h"
#include "Components/InputKeySelector.h"
#include "World/MazeOnlineGameInstance.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "GameFramework/GameUserSettings.h"

namespace
{
	constexpr float FrameLimits[] = {60.f, 90.f, 120.f, 144.f, 0.f};

	template <typename T> T* Find(UUserWidget* Widget, const TCHAR* Name)
	{
		return Cast<T>(Widget->GetWidgetFromName(Name));
	}

	void SetIndex(UUserWidget* Widget, const TCHAR* Name, int32 Index)
	{
		if (auto* Combo = Find<UComboBoxString>(Widget, Name))
			Combo->SetSelectedIndex(Index);
	}

	int32 Index(UUserWidget* Widget, const TCHAR* Name, int32 Default)
	{
		const auto* Combo = Find<UComboBoxString>(Widget, Name);

		return Combo && Combo->GetSelectedIndex() >= 0 ? Combo->GetSelectedIndex() : Default;
	}

	void SetCheck(UUserWidget* Widget, const TCHAR* Name, bool bValue)
	{
		if (auto* Check = Find<UCheckBox>(Widget, Name))
			Check->SetIsChecked(bValue);
	}

	bool Checked(UUserWidget* Widget, const TCHAR* Name, bool bDefault)
	{
		const auto* Check = Find<UCheckBox>(Widget, Name);

		return Check ? Check->IsChecked() : bDefault;
	}

	void SetSlider(UUserWidget* Widget, const TCHAR* Name, float Value)
	{
		if (auto* Slider = Find<USlider>(Widget, Name))
			Slider->SetValue(Value);
	}

	float SliderValue(UUserWidget* Widget, const TCHAR* Name, float Default)
	{
		const auto* Slider = Find<USlider>(Widget, Name);

		return Slider && FMath::IsFinite(Slider->GetValue()) ? Slider->GetValue() : Default;
	}

	void Status(UUserWidget* Widget, const FText& Value)
	{
		if (auto* Label = Find<UTextBlock>(Widget, TEXT("KeyBindingStatus")))
			Label->SetText(Value);
	}
}

void UMazeMenuWidget::ReadSettingsIntoControls()
{
	if (!GetWidgetFromName(TEXT("SettingsPages")))
		return;

	TGuardValue<bool> Reading(bReadingSettings, true);
	const auto Preferences = FMazeInterfacePreferences::Read();

	SetCheck(this, TEXT("InvertYCheck"), Preferences.bInvertMouseY);
	SetCheck(this, TEXT("CompassCheck"), Preferences.bShowCompass);
	SetCheck(this, TEXT("CrosshairCheck"), Preferences.bShowCrosshair);
	SetCheck(this, TEXT("CameraMotionCheck"), Preferences.bCameraMotion);
	SetSlider(this, TEXT("SensitivitySlider"), GetDefault<UMazePreferences>()->GetSensitivity());
	ChangeSensitivity(GetDefault<UMazePreferences>()->GetSensitivity());

	if (auto* Video = UGameUserSettings::GetGameUserSettings())
	{
		const int32 Quality = Video->GetOverallScalabilityLevel();

		SetIndex(this, TEXT("QualityCombo"), Quality >= 0 && Quality <= 3 ? Quality : 4);
		SetIndex(this, TEXT("ShadowsCombo"), FMath::Clamp(Video->GetShadowQuality(), 0, 4));

		const float Limit = Video->GetFrameRateLimit();
		int32 Selected = 5;

		for (int32 I = 0; I < UE_ARRAY_COUNT(FrameLimits); ++I)
			if (FMath::IsNearlyEqual(Limit, FrameLimits[I]))
				Selected = I;

		SetIndex(this, TEXT("FrameLimitCombo"), Selected);
		SetCheck(this, TEXT("VSyncCheck"), Video->IsVSyncEnabled());

		float Normalized, Scale, Min, Max;

		Video->GetResolutionScaleInformationEx(Normalized, Scale, Min, Max);
		// The engine may keep zero as the platform-default quality before a manual scale is chosen.
		Min = FMath::Max(25.f, Min);
		Max = FMath::Max(Min, Max);
		Scale = FMath::Clamp(FMath::IsFinite(Scale) && Scale > 0.f ? Scale : 100.f, Min, Max);

		if (auto* Slider = Find<USlider>(this, TEXT("RenderScaleSlider")))
		{
			Slider->SetMinValue(Min);
			Slider->SetMaxValue(Max);
			Slider->SetValue(Scale);
		}

		ChangeRenderScale(Scale);
	}

	ReadKeyBindings();
}

void UMazeMenuWidget::SelectSettingsSection(int32 Index)
{
	if (auto* Pages = Find<UWidgetSwitcher>(this, TEXT("SettingsPages")))
		Pages->SetActiveWidgetIndex(FMath::Clamp(Index, 0, 2));

	const TCHAR* Buttons[] = {TEXT("VideoTabButton"), TEXT("ControlsTabButton"), TEXT("GameTabButton")};

	const FText Titles[] = {NSLOCTEXT("Maze.Glass", "Video", "ВИДЕО"),
	                        NSLOCTEXT("Maze.Glass", "Controls", "УПРАВЛЕНИЕ"),
	                        NSLOCTEXT("Maze.Glass", "Game", "ИГРА")};

	Index = FMath::Clamp(Index, 0, 2);

	if (auto* Heading = Find<UTextBlock>(this, TEXT("SettingsSectionTitle")))
		Heading->SetText(Titles[Index]);

	for (int32 I = 0; I < UE_ARRAY_COUNT(Buttons); ++I)
	{
		if (auto* Label = Find<UTextBlock>(this, *(FString(Buttons[I]) + TEXT("Label"))))
			Label->SetColorAndOpacity(I == Index ? MazeInterfaceStyle::Accent : MazeInterfaceStyle::Ink);

		if (auto* Indicator = GetWidgetFromName(*(FString(Buttons[I]) + TEXT("Indicator"))))
			Indicator->SetVisibility(I == Index ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

void UMazeMenuWidget::ShowVideoSettings()
{
	SelectSettingsSection(0);
}

void UMazeMenuWidget::ShowControlSettings()
{
	SelectSettingsSection(1);
}

void UMazeMenuWidget::ShowGameSettings()
{
	SelectSettingsSection(2);
}

void UMazeMenuWidget::ChangeRenderScale(float Value)
{
	if (auto* Label = Find<UTextBlock>(this, TEXT("RenderScaleText")))
		Label->SetText(
		    FText::Format(NSLOCTEXT("Maze.Settings", "ArcPercent", "{0}%"), FText::AsNumber(FMath::RoundToInt(Value))));

	if (!bReadingSettings)
		SaveVideoSettings();
}

void UMazeMenuWidget::BindSettingsEvents()
{
	for (const TCHAR* Name : {TEXT("InvertYCheck"),
	                          TEXT("CompassCheck"),
	                          TEXT("CrosshairCheck"),
	                          TEXT("CameraMotionCheck"),
	                          TEXT("VSyncCheck")})
		if (auto* Check = Find<UCheckBox>(this, Name))
			Check->OnCheckStateChanged.AddUniqueDynamic(this, &UMazeMenuWidget::ChangeCheckSetting);

	for (const TCHAR* Name : {TEXT("ShadowsCombo"), TEXT("FrameLimitCombo")})
		if (auto* Combo = Find<UComboBoxString>(this, Name))
			Combo->OnSelectionChanged.AddUniqueDynamic(this, &UMazeMenuWidget::ChangeVideoOption);

	if (auto* Combo = Find<UComboBoxString>(this, TEXT("QualityCombo")))
		Combo->OnSelectionChanged.AddUniqueDynamic(this, &UMazeMenuWidget::ChangeQuality);

	for (const auto& Binding : MazeKeyBindings::Definitions())
		if (auto* Selector = Cast<UInputKeySelector>(GetWidgetFromName(Binding.WidgetName())))
		{
			// The menu paints a clipped, scrolling label over the native key-capture button.
			Selector->SetTextBlockVisibility(ESlateVisibility::Hidden);
			Selector->OnKeySelected.AddUniqueDynamic(this, &UMazeMenuWidget::ChangeBinding);
		}
}

void UMazeMenuWidget::ReadKeyBindings()
{
	TGuardValue<bool> Reading(bReadingSettings, true);

	for (const auto& Binding : MazeKeyBindings::Definitions())
		if (auto* Selector = Cast<UInputKeySelector>(GetWidgetFromName(Binding.WidgetName())))
			Selector->SetSelectedKey(FInputChord(MazeKeyBindings::GetKey(Binding.Id)));
}

bool UMazeMenuWidget::IsSelectingBinding() const
{
	for (const auto& Binding : MazeKeyBindings::Definitions())
		if (const auto* Selector = Cast<UInputKeySelector>(GetWidgetFromName(Binding.WidgetName()));
		    Selector && Selector->GetIsSelectingKey())
			return true;

	return false;
}

void UMazeMenuWidget::ClearBindingInput()
{
	if (auto* Controller = GetOwningPlayer())
	{
		Controller->FlushPressedKeys();

		if (auto* Character = Cast<AMazeCharacter>(Controller->GetPawn()))
			Character->ClearLocalInput();
	}
}

void UMazeMenuWidget::ChangeBinding(FInputChord SelectedKey)
{
	if (bReadingSettings)
		return;

	for (const auto& Binding : MazeKeyBindings::Definitions())
		if (const auto* Selector = Cast<UInputKeySelector>(GetWidgetFromName(Binding.WidgetName()));
		    Selector && Selector->GetSelectedKey().Key != MazeKeyBindings::GetKey(Binding.Id))
		{
			FText Error;

			if (MazeKeyBindings::SetKey(Binding.Id, Selector->GetSelectedKey().Key, Error))
				ClearBindingInput();

			Status(this, Error);
			ReadKeyBindings();
			break;
		}
}

void UMazeMenuWidget::ChangeCheckSetting(bool bChecked)
{
	ApplySettings();
}

void UMazeMenuWidget::ChangeVideoOption(FString Selected, ESelectInfo::Type SelectionType)
{
	if (!bReadingSettings)
		SaveVideoSettings();
}

void UMazeMenuWidget::ChangeQuality(FString Selected, ESelectInfo::Type SelectionType)
{
	if (bReadingSettings)
		return;

	if (auto* Video = UGameUserSettings::GetGameUserSettings())
	{
		const int32 Quality = Index(this, TEXT("QualityCombo"), 4);

		if (Quality < 4)
		{
			Video->SetOverallScalabilityLevel(Quality);
			Video->ApplyNonResolutionSettings();
			Video->SaveSettings();
		}

		ReadSettingsIntoControls();
	}
}

void UMazeMenuWidget::SaveVideoSettings()
{
	if (bReadingSettings || !GetWidgetFromName(TEXT("SettingsPages")))
		return;

	if (auto* Video = UGameUserSettings::GetGameUserSettings())
	{
		// Individual changes must not reapply the preset and overwrite other choices.
		Video->SetShadowQuality(FMath::Clamp(Index(this, TEXT("ShadowsCombo"), Video->GetShadowQuality()), 0, 4));

		const int32 Limit = Index(this, TEXT("FrameLimitCombo"), 5);

		if (Limit >= 0 && Limit < UE_ARRAY_COUNT(FrameLimits))
			Video->SetFrameRateLimit(FrameLimits[Limit]);

		Video->SetVSyncEnabled(Checked(this, TEXT("VSyncCheck"), Video->IsVSyncEnabled()));
		Video->SetResolutionScaleValueEx(SliderValue(this, TEXT("RenderScaleSlider"), 100.f));
		Video->ApplyNonResolutionSettings();
		Video->SaveSettings();

		TGuardValue<bool> Reading(bReadingSettings, true);
		const int32 Quality = Video->GetOverallScalabilityLevel();

		SetIndex(this, TEXT("QualityCombo"), Quality >= 0 && Quality <= 3 ? Quality : 4);
	}
}

void UMazeMenuWidget::ApplySettings()
{
	if (bReadingSettings)
		return;

	auto Preferences = FMazeInterfacePreferences::Read();

	Preferences.bInvertMouseY = Checked(this, TEXT("InvertYCheck"), Preferences.bInvertMouseY);
	Preferences.bShowCompass = Checked(this, TEXT("CompassCheck"), Preferences.bShowCompass);
	Preferences.bShowCrosshair = Checked(this, TEXT("CrosshairCheck"), Preferences.bShowCrosshair);
	Preferences.bCameraMotion = Checked(this, TEXT("CameraMotionCheck"), Preferences.bCameraMotion);
	Preferences.Save();
	GetMutableDefault<UMazePreferences>()->SetSensitivity(SliderValue(this, TEXT("SensitivitySlider"), 1.f));
	SaveVideoSettings();
}

void UMazeMenuWidget::ResetSettings()
{
	{
		TGuardValue<bool> Reading(bReadingSettings, true);

		if (auto* Video = UGameUserSettings::GetGameUserSettings())
			Video->SetOverallScalabilityLevel(2);

		SetIndex(this, TEXT("QualityCombo"), 2);
		SetIndex(this, TEXT("ShadowsCombo"), 2);
		SetIndex(this, TEXT("FrameLimitCombo"), 2);
		SetCheck(this, TEXT("VSyncCheck"), false);
		SetCheck(this, TEXT("InvertYCheck"), false);
		SetCheck(this, TEXT("CompassCheck"), true);
		SetCheck(this, TEXT("CrosshairCheck"), true);
		SetCheck(this, TEXT("CameraMotionCheck"), true);
		SetSlider(this, TEXT("SensitivitySlider"), 1.f);
		SetSlider(this, TEXT("RenderScaleSlider"), 100.f);
		ChangeSensitivity(1.f);
		ChangeRenderScale(100.f);
	}
	MazeKeyBindings::Reset();
	ClearBindingInput();
	ReadKeyBindings();
	ApplySettings();
	Status(this, FText::GetEmpty());
}

void UMazeMenuWidget::ReturnToMainMenu()
{
	if (auto* Online = GetGameInstance<UMazeOnlineGameInstance>())
		Online->Leave();
}
