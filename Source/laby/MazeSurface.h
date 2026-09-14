#pragma once
#include "MazeLayout.h"

// Union of wall strips on an alternating junction/corridor grid.
// Only boundary faces are emitted; touching solids have no internal faces.
struct FMazeSurface
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	void Build(const FMazeLayout& Layout, float Cell, float Thickness, float Height)
	{
		Vertices.Reset(); Triangles.Reset(); Normals.Reset();
		const int32 N = Layout.Size * 2 + 1;
		TArray<bool> Solid; Solid.Init(false, N * N);
		auto Strip = [&](int32 X, int32 Y, bool Horizontal)
		{
			for (int32 I = 0; I < 3; ++I) Solid[(Y + (Horizontal ? 0 : I)) * N + X + (Horizontal ? I : 0)] = true;
		};
		for (int32 Y = 0; Y < Layout.Size; ++Y)
			for (int32 X = 0; X < Layout.Size; ++X)
			{
				uint8 W = Layout.Walls[Y * Layout.Size + X];
				if (W & 1) Strip(X * 2, Y * 2, true);
				if (W & 8) Strip(X * 2, Y * 2, false);
				if (X == Layout.Size - 1 && (W & 2)) Strip((X + 1) * 2, Y * 2, false);
				if (Y == Layout.Size - 1 && (W & 4)) Strip(X * 2, (Y + 1) * 2, true);
			}
		auto Occupied = [&](int32 X, int32 Y) { return X >= 0 && Y >= 0 && X < N && Y < N && Solid[Y * N + X]; };
		auto Edge = [&](int32 I) { return (I / 2) * Cell + (I % 2 ? Thickness / 2 : -Thickness / 2); };
		auto Face = [&](FVector A, FVector B, FVector C, FVector D, FVector Normal)
		{
			int32 Base = Vertices.Num();
			Vertices.Append({A, B, C, D});
			Normals.Append({Normal, Normal, Normal, Normal});
			Triangles.Append({Base, Base + 2, Base + 1, Base, Base + 3, Base + 2});
		};
		for (int32 Y = 0; Y < N; ++Y)
			for (int32 X = 0; X < N; ++X)
			{
				if (!Occupied(X, Y)) continue;
				float A = Edge(X), B = Edge(X + 1), C = Edge(Y), D = Edge(Y + 1);
				Face({A,C,Height}, {B,C,Height}, {B,D,Height}, {A,D,Height}, {0,0,1});
				Face({A,D,0}, {B,D,0}, {B,C,0}, {A,C,0}, {0,0,-1});
				if (!Occupied(X, Y - 1)) Face({A,C,0}, {B,C,0}, {B,C,Height}, {A,C,Height}, {0,-1,0});
				if (!Occupied(X, Y + 1)) Face({B,D,0}, {A,D,0}, {A,D,Height}, {B,D,Height}, {0,1,0});
				if (!Occupied(X - 1, Y)) Face({A,D,0}, {A,C,0}, {A,C,Height}, {A,D,Height}, {-1,0,0});
				if (!Occupied(X + 1, Y)) Face({B,C,0}, {B,D,0}, {B,D,Height}, {B,C,Height}, {1,0,0});
			}
	}
};
