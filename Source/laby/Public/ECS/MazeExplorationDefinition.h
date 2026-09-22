#pragma once

// Exploration distances are in maze cells, independent of view/camera direction.
struct FMazeExplorationDefinition
{
	static constexpr int RevealRadiusCells = 2;
	static constexpr float RefreshDistanceCells = 0.05f;
};
