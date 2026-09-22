#pragma once
#include "Maze/MazeLayout.h"

// Union of wall strips with room doorways and deterministic convex-corner chamfers.
// Wall faces are quads (including lintel trapezoids); caps are padded to four vertices.
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
