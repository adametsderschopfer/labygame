#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

class APlayerController;

// Device-local input configuration. Gameplay intent still goes through ECS.
struct FMazeKeyBinding
{
	FName Id;
	FName Mapping;
	float AxisScale;
	FKey DefaultKey;
	FText Label;
	FName WidgetName() const
	{
		return FName(*(TEXT("Binding_") + Id.ToString()));
	}
};

namespace MazeKeyBindings
{
	LABY_API TConstArrayView<FMazeKeyBinding> Definitions();
	LABY_API FKey GetKey(FName Id);
	LABY_API bool IsHeld(const APlayerController& Controller, FName Id);
	LABY_API bool SetKey(FName Id, FKey Key, FText& Error);
	LABY_API void Reset();
	LABY_API void EnsureDefaults();
}
