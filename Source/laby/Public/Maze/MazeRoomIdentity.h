#pragma once

#include "CoreMinimal.h"

// Stable facility designation. Physical room geometry remains in EMazeRoomType.
enum class EMazeRoomPurpose : uint8
{
	Ward,
	Treatment,
	Examination,
	Isolation,
	Staff,
	Records,
	Storage,
	Laundry,
	Generator,
	Recreation,
	Dining,
	Hydrotherapy,
	Washroom,
	Pool
};
