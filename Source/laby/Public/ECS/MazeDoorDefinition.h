#pragma once

// Shared gameplay dimensions for generated manually operated doors.
struct FMazeDoorDefinition
{
	static constexpr float InteractionRangeCm = 120.f;
	static constexpr float AimRadiusCm = 70.f;
	static constexpr float OpenDurationSeconds = 0.65f;
	static constexpr float CloseDurationSeconds = 0.65f;
	static constexpr float SwingDegrees = 88.f;
	static constexpr float FrameWidthCm = 8.f;
	static constexpr float HandleInsetCm = 15.f;
	static constexpr float HandleHeightCm = 110.f;
	static constexpr float CardDoorFraction = .3f;
	static constexpr float ReaderSideOffsetCm = 24.f;
	static constexpr float ReaderHeightCm = 125.f;
};
