#pragma once

// Shared gameplay and presentation dimensions for generated automatic doors.
// Distances are Unreal centimeters; durations are seconds.
struct FMazeDoorDefinition
{
	static constexpr float OpenRadiusCm = 230.f;
	static constexpr float KeepOpenRadiusCm = 285.f;
	static constexpr float VerticalRangeCm = 190.f;
	static constexpr float CloseDelaySeconds = 0.85f;
	static constexpr float OpenDurationSeconds = 0.62f;
	static constexpr float CloseDurationSeconds = 0.82f;
	static constexpr float LeafTravelCm = 62.f;
};
