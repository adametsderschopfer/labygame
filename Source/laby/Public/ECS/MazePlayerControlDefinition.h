#pragma once

#include "Maze/MazeNarrowPassageDefinition.h"

struct FMazePlayerControlDefinition
{
	static constexpr float WalkSpeed = 350.f;
	static constexpr float SprintSpeed = 600.f;
	static constexpr float NarrowPassageSpeed = FMazeNarrowPassageDefinition::SlowWalkSpeed;
	static constexpr float CrouchSpeed = 225.f;
	static constexpr float CrouchedHalfHeight = 55.f;
	static constexpr float JumpVelocity = 330.f;
};
