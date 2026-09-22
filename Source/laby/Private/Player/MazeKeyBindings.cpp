#include "Player/MazeKeyBindings.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
	TArray<FKey> Keys(const FMazeKeyBinding& Binding)
	{
		TArray<FKey> Result;
		const auto* Settings = GetDefault<UInputSettings>();

		if (Binding.AxisScale != 0.f)
		{
			for (const auto& Mapping : Settings->GetAxisMappings())
				if (Mapping.AxisName == Binding.Mapping && Mapping.Scale == Binding.AxisScale &&
				    !Mapping.Key.IsGamepadKey())
					Result.Add(Mapping.Key);
		}
		else
		{
			for (const auto& Mapping : Settings->GetActionMappings())
				if (Mapping.ActionName == Binding.Mapping && !Mapping.Key.IsGamepadKey())
					Result.Add(Mapping.Key);
		}

		return Result;
	}

	void Replace(const FMazeKeyBinding& Binding, FKey Key)
	{
		auto* Settings = GetMutableDefault<UInputSettings>();

		if (Binding.AxisScale != 0.f)
		{
			const auto Mappings = Settings->GetAxisMappings();

			for (const auto& Mapping : Mappings)
				if (Mapping.AxisName == Binding.Mapping && Mapping.Scale == Binding.AxisScale &&
				    !Mapping.Key.IsGamepadKey())
					Settings->RemoveAxisMapping(Mapping, false);

			Settings->AddAxisMapping(FInputAxisKeyMapping(Binding.Mapping, Key, Binding.AxisScale), false);
		}
		else
		{
			const auto Mappings = Settings->GetActionMappings();

			for (const auto& Mapping : Mappings)
				if (Mapping.ActionName == Binding.Mapping && !Mapping.Key.IsGamepadKey())
					Settings->RemoveActionMapping(Mapping, false);

			Settings->AddActionMapping(FInputActionKeyMapping(Binding.Mapping, Key), false);
		}
	}

	void Save()
	{
		auto* Settings = GetMutableDefault<UInputSettings>();

		// Persist to the user's generated Input.ini, never the project's DefaultInput.ini.
		Settings->SaveConfig(CPF_Config, *GInputIni);
		Settings->ForceRebuildKeymaps();
	}
}

TConstArrayView<FMazeKeyBinding> MazeKeyBindings::Definitions()
{
	static const FMazeKeyBinding Bindings[] = {
	    {TEXT("Forward"), TEXT("MoveForward"), 1.f, EKeys::W, NSLOCTEXT("Maze.Keys", "Forward", "Вперёд")},
	    {TEXT("Backward"), TEXT("MoveForward"), -1.f, EKeys::S, NSLOCTEXT("Maze.Keys", "Backward", "Назад")},
	    {TEXT("Left"), TEXT("MoveRight"), -1.f, EKeys::A, NSLOCTEXT("Maze.Keys", "Left", "Влево")},
	    {TEXT("Right"), TEXT("MoveRight"), 1.f, EKeys::D, NSLOCTEXT("Maze.Keys", "Right", "Вправо")},
	    {TEXT("Sprint"), TEXT("Sprint"), 0.f, EKeys::LeftShift, NSLOCTEXT("Maze.Keys", "Sprint", "Бег")},
	    {TEXT("Jump"), TEXT("Jump"), 0.f, EKeys::SpaceBar, NSLOCTEXT("Maze.Keys", "Jump", "Прыжок")},
	    {TEXT("Crouch"), TEXT("Crouch"), 0.f, EKeys::LeftControl, NSLOCTEXT("Maze.Keys", "Crouch", "Присесть")},
	    {TEXT("Headlamp"), TEXT("Headlamp"), 0.f, EKeys::L, NSLOCTEXT("Maze.Keys", "Headlamp", "Фонарик")},
	    {TEXT("Map"), TEXT("Map"), 0.f, EKeys::M, NSLOCTEXT("Maze.Keys", "Map", "Карта")},
	    {TEXT("NewMaze"), TEXT("NewMaze"), 0.f, EKeys::R, NSLOCTEXT("Maze.Keys", "NewMaze", "Новый лабиринт")}};

	return MakeArrayView(Bindings);
}

FKey MazeKeyBindings::GetKey(FName Id)
{
	for (const auto& Binding : Definitions())
		if (Binding.Id == Id)
		{
			const auto Mapped = Keys(Binding);

			return Mapped.IsEmpty() ? Binding.DefaultKey : Mapped[0];
		}

	return EKeys::Invalid;
}

bool MazeKeyBindings::IsHeld(const APlayerController& Controller, FName Id)
{
	for (const auto& Binding : Definitions())
		if (Binding.Id == Id)
			for (FKey Key : Keys(Binding))
				if (Controller.IsInputKeyDown(Key))
					return true;

	return false;
}

bool MazeKeyBindings::SetKey(FName Id, FKey Key, FText& Error)
{
	if (!Key.IsValid() || !Key.IsBindableToActions() || Key.IsGamepadKey() || Key == EKeys::Escape ||
	    Key == EKeys::F6 || Key == EKeys::F11 || GetDefault<UInputSettings>()->ConsoleKeys.Contains(Key))
	{
		Error = NSLOCTEXT("Maze.Keys", "Reserved", "Эта клавиша недоступна. Выберите другую.");

		return false;
	}

	const FMazeKeyBinding* Target = nullptr;

	for (const auto& Binding : Definitions())
	{
		if (Binding.Id == Id)
			Target = &Binding;
		else if (Keys(Binding).Contains(Key) || GetKey(Binding.Id) == Key)
		{
			Error = FText::Format(NSLOCTEXT("Maze.Keys", "Conflict", "Клавиша уже назначена: {0}."), Binding.Label);

			return false;
		}
	}

	if (!Target)
		return false;

	Replace(*Target, Key);
	Save();
	Error = FText::GetEmpty();

	return true;
}

void MazeKeyBindings::Reset()
{
	for (const auto& Binding : Definitions())
		Replace(Binding, Binding.DefaultKey);

	Save();
}

void MazeKeyBindings::EnsureDefaults()
{
	bool bChanged = false;

	for (const auto& Binding : Definitions())
		if (Keys(Binding).IsEmpty())
		{
			Replace(Binding, Binding.DefaultKey);
			bChanged = true;
		}

	if (bChanged)
		Save();
}
