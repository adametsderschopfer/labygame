#pragma once
#include "CoreMinimal.h"
struct FMazeLayout;

// Shared localized presentation for map labels and read-only development diagnostics.
FText MazeRoomLabel(const FMazeLayout& Layout, int32 Index, bool bCompact);
