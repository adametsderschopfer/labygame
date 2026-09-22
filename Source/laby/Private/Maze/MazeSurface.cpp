#include "Maze/MazeSurface.h"
#include "Maze/MazeRoomDefinition.h"

namespace
{
	// Fill one wall strip with a U-shaped opening. End faces join the neighboring
	// occupied junction tiles; no overlapping boxes or hidden internal caps.
	void Doorway(
	    FMazeSurface& Mesh, FVector Origin, FVector Along, FVector Across, float Span, float Depth, float Height)
	{
		const float Width = FMazeRoomDefinition::OpeningWidth(Span);
		const float OpeningHeight = FMazeRoomDefinition::OpeningHeight(Height);
		const float Left = (Span - Width) / 2, Right = (Span + Width) / 2;
		const FVector Up(0, 0, 1);
		const auto Point = [&](float X, float Y, float Z)
		{
			return Origin + Along * X + Across * Y + Up * Z;
		};
		const auto Quad = [&](FVector A, FVector B, FVector C, FVector D, FVector Normal)
		{
			const int32 Base = Mesh.Vertices.Num();
			Mesh.Vertices.Append({A, B, C, D});
			Mesh.Normals.Append({Normal, Normal, Normal, Normal});

			if (FVector::DotProduct(FVector::CrossProduct(B - A, C - A), Normal) > 0)
				Mesh.Triangles.Append({Base, Base + 2, Base + 1, Base, Base + 3, Base + 2});
			else
				Mesh.Triangles.Append({Base, Base + 1, Base + 2, Base, Base + 2, Base + 3});
		};

		for (const float Y : {0.f, Depth})
		{
			const FVector Normal = Y == 0 ? -Across : Across;

			// Trapezoids meet the lintel without T-junctions on the outer tile edges.
			Quad(Point(0, Y, 0), Point(Left, Y, 0), Point(Left, Y, OpeningHeight), Point(0, Y, Height), Normal);
			Quad(Point(Right, Y, 0), Point(Span, Y, 0), Point(Span, Y, Height), Point(Right, Y, OpeningHeight), Normal);
			Quad(Point(0, Y, Height),
			     Point(Left, Y, OpeningHeight),
			     Point(Right, Y, OpeningHeight),
			     Point(Span, Y, Height),
			     Normal);
		}

		Quad(Point(0, 0, Height), Point(Span, 0, Height), Point(Span, Depth, Height), Point(0, Depth, Height), Up);
		Quad(Point(0, 0, 0), Point(Left, 0, 0), Point(Left, Depth, 0), Point(0, Depth, 0), -Up);
		Quad(Point(Right, 0, 0), Point(Span, 0, 0), Point(Span, Depth, 0), Point(Right, Depth, 0), -Up);
		Quad(Point(Left, 0, 0),
		     Point(Left, Depth, 0),
		     Point(Left, Depth, OpeningHeight),
		     Point(Left, 0, OpeningHeight),
		     Along);
		Quad(Point(Right, Depth, 0),
		     Point(Right, 0, 0),
		     Point(Right, 0, OpeningHeight),
		     Point(Right, Depth, OpeningHeight),
		     -Along);
		Quad(Point(Left, 0, OpeningHeight),
		     Point(Right, 0, OpeningHeight),
		     Point(Right, Depth, OpeningHeight),
		     Point(Left, Depth, OpeningHeight),
		     -Up);
	}
}

void FMazeSurface::Build(
    const FMazeLayout& Layout, float Cell, float Thickness, float Height, FIntRect Cells, int32 Seed)
{
	Vertices.Reset();
	Triangles.Reset();
	Normals.Reset();

	const int32 N = Layout.Size * 2 + 1;
	const bool bRegion = Cells.Width() > 0 && Cells.Height() > 0;
	const int32 BeginX = bRegion ? FMath::Clamp(Cells.Min.X * 2, 0, N) : 0;
	const int32 BeginY = bRegion ? FMath::Clamp(Cells.Min.Y * 2, 0, N) : 0;
	const int32 EndX = bRegion ? FMath::Min(N, Cells.Max.X * 2 + (Cells.Max.X == Layout.Size ? 1 : 0)) : N;
	const int32 EndY = bRegion ? FMath::Min(N, Cells.Max.Y * 2 + (Cells.Max.Y == Layout.Size ? 1 : 0)) : N;
	TArray<bool> Solid;

	Solid.Init(false, N * N);

	auto Strip = [&](int32 X, int32 Y, bool Horizontal)
	{
		for (int32 I = 0; I < 3; ++I)
			Solid[(Y + (Horizontal ? 0 : I)) * N + X + (Horizontal ? I : 0)] = true;
	};

	for (int32 Y = 0; Y < Layout.Size; ++Y)
		for (int32 X = 0; X < Layout.Size; ++X)
		{
			uint8 W = Layout.Walls[Y * Layout.Size + X];

			if (W & 1)
				Strip(X * 2, Y * 2, true);

			if (W & 8)
				Strip(X * 2, Y * 2, false);

			if (X == Layout.Size - 1 && (W & 2))
				Strip((X + 1) * 2, Y * 2, false);

			if (Y == Layout.Size - 1 && (W & 4))
				Strip(X * 2, (Y + 1) * 2, true);
		}

	// Door positions are derived from the same immutable topology for collision,
	// visual chunks and their halos. The strip's center tile owns the opening.
	TMap<int32, bool> DoorTiles;

	for (const FMazeRoomDoorway& Door : Layout.RoomDoorways())
	{
		const bool bHorizontal = Door.Direction == 0 || Door.Direction == 2;
		const int32 X = Door.Cell.X * 2 + (Door.Direction == 1 ? 2 : 0);
		const int32 Y = Door.Cell.Y * 2 + (Door.Direction == 2 ? 2 : 0);

		Strip(X, Y, bHorizontal);
		DoorTiles.Add((Y + (bHorizontal ? 0 : 1)) * N + X + (bHorizontal ? 1 : 0), bHorizontal);
	}

	auto Occupied = [&](int32 X, int32 Y)
	{
		return X >= 0 && Y >= 0 && X < N && Y < N && Solid[Y * N + X];
	};
	auto Edge = [&](int32 I)
	{
		return (I / 2) * Cell + (I % 2 ? Thickness / 2 : -Thickness / 2);
	};
	auto Face = [&](FVector A, FVector B, FVector C, FVector D, FVector Normal)
	{
		int32 Base = Vertices.Num();
		Vertices.Append({A, B, C, D});
		Normals.Append({Normal, Normal, Normal, Normal});
		Triangles.Append({Base, Base + 2, Base + 1, Base, Base + 3, Base + 2});
	};

	for (int32 Y = BeginY; Y < EndY; ++Y)
		for (int32 X = BeginX; X < EndX; ++X)
		{
			if (!Occupied(X, Y))
				continue;

			const float A = Edge(X), B = Edge(X + 1), C = Edge(Y), D = Edge(Y + 1);

			if (const bool* Horizontal = DoorTiles.Find(Y * N + X))
			{
				Doorway(*this,
				        *Horizontal ? FVector(A, C, 0) : FVector(B, C, 0),
				        *Horizontal ? FVector(1, 0, 0) : FVector(0, 1, 0),
				        *Horizontal ? FVector(0, 1, 0) : FVector(-1, 0, 0),
				        *Horizontal ? B - A : D - C,
				        *Horizontal ? D - C : B - A,
				        Height);
				continue;
			}

			const FVector Corners[] = {{A, C, 0}, {B, C, 0}, {B, D, 0}, {A, D, 0}};
			const int32 DX[] = {-1, 1, 1, -1}, DY[] = {-1, -1, 1, 1};
			const float Cut = FMath::Min(ChamferInsetCm, FMath::Min(B - A, D - C) * 0.4f);
			TArray<FVector, TInlineAllocator<8>> Polygon;

			for (int32 Corner = 0; Corner < 4; ++Corner)
			{
				const int32 SX = DX[Corner], SY = DY[Corner];
				// Never remove a shared wall or junction edge.
				const bool bConvex = !Occupied(X + SX, Y) && !Occupied(X, Y + SY) && !Occupied(X + SX, Y + SY);
				const uint32 KeyX = uint32(X + (SX > 0)), KeyY = uint32(Y + (SY > 0));
				FRandomStream Random(
				    static_cast<int32>(uint32(Seed) ^ KeyX * 73856093u ^ KeyY * 19349663u ^ 0xB5297A4Du));
				const FVector P = Corners[Corner];

				if (bConvex && Cut > 0 && Random.FRand() < ChamferFraction)
				{
					Polygon.Add(P + (Corners[(Corner + 3) % 4] - P).GetSafeNormal() * Cut);
					Polygon.Add(P + (Corners[(Corner + 1) % 4] - P).GetSafeNormal() * Cut);
				}
				else
					Polygon.Add(P);
			}

			// Interior trim scans vertical quads. Align cap blocks to four vertices;
			// padding is unreferenced and creates no degenerate triangles.
			for (const bool bTop : {false, true})
			{
				const int32 Base = Vertices.Num();
				const FVector Normal(0, 0, bTop ? 1 : -1), Offset(0, 0, bTop ? Height : 0);

				for (const FVector& P : Polygon)
				{
					Vertices.Add(P + Offset);
					Normals.Add(Normal);
				}

				while (Vertices.Num() % 4 != 0)
				{
					Vertices.Add(Polygon.Last() + Offset);
					Normals.Add(Normal);
				}

				for (int32 I = 1; I + 1 < Polygon.Num(); ++I)
					if (bTop)
						Triangles.Append({Base, Base + I + 1, Base + I});
					else
						Triangles.Append({Base, Base + I, Base + I + 1});
			}

			for (int32 I = 0; I < Polygon.Num(); ++I)
			{
				const FVector P = Polygon[I], Q = Polygon[(I + 1) % Polygon.Num()];

				if ((P.Y == C && Q.Y == C && Occupied(X, Y - 1)) || (P.Y == D && Q.Y == D && Occupied(X, Y + 1)) ||
				    (P.X == A && Q.X == A && Occupied(X - 1, Y)) || (P.X == B && Q.X == B && Occupied(X + 1, Y)))
					continue;

				const FVector Direction = Q - P;
				const FVector Normal = FVector(Direction.Y, -Direction.X, 0).GetSafeNormal();
				Face(P, Q, Q + FVector(0, 0, Height), P + FVector(0, 0, Height), Normal);
			}
		}
}
