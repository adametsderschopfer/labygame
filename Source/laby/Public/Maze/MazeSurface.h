#pragma once
#include "Maze/MazeLayout.h"

// Union of wall strips with deterministic full-height chamfers on convex corners.
// Vertical faces are quads; cap vertex blocks are padded to multiples of four.
struct FMazeSurface
{
	static constexpr float ChamferInsetCm = 15.f;
	static constexpr float ChamferFraction = 1.f / 3.f;
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	void Build(const FMazeLayout& Layout,
	           float Cell,
	           float Thickness,
	           float Height,
	           FIntRect Cells = FIntRect(),
	           int32 Seed = 0);
};
