#pragma once

#include "Maze/MazeSurface.h"

// Cosmetic geometry derived from a generation snapshot, never gameplay state.
struct FMazeInterior
{
	static constexpr float PanelTargetCm = 120.f;
	static constexpr float CoveHeightCm = 10.f;
	static constexpr float CoveRadiusCm = 3.f;
	static constexpr float CornerRadiusCm = 0.35f;
	// Green flooring, satin metal, painted ivory, dark recesses.
	FMazeSurface Sections[4];
	TArray<FTransform> SocketTransforms;
	TArray<FTransform> DetectorTransforms;
	TArray<FVector> LampLocations;
	static FMazeInterior Build(
	    const FMazeLayout& Layout, const FMazeSurface& Walls, float Cell, float Thickness, float Height, int32 Seed);
};
