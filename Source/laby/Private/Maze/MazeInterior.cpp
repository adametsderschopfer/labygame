#include "Maze/MazeInterior.h"

namespace
{
	void Quad(FMazeSurface& Mesh, FVector A, FVector B, FVector C, FVector D, FVector N)
	{
		const int32 Base = Mesh.Vertices.Num();

		Mesh.Vertices.Append({A, B, C, D});
		Mesh.Normals.Append({N, N, N, N});

		// Match Unreal's clockwise front faces regardless of the supplied basis.
		if (FVector::DotProduct(FVector::CrossProduct(B - A, C - A), N) > 0)
			Mesh.Triangles.Append({Base, Base + 2, Base + 1, Base, Base + 3, Base + 2});
		else
			Mesh.Triangles.Append({Base, Base + 1, Base + 2, Base, Base + 2, Base + 3});
	}

	void Box(FMazeSurface& Mesh, FVector P, FVector U, FVector V, FVector N, FVector Size)
	{
		U *= Size.X * 0.5;
		V *= Size.Y * 0.5;

		const FVector W = N * Size.Z * 0.5;

		Quad(Mesh, P - U - V + W, P + U - V + W, P + U + V + W, P - U + V + W, N);
		Quad(Mesh, P - U + V - W, P + U + V - W, P + U - V - W, P - U - V - W, -N);
		Quad(Mesh, P + U - V - W, P + U + V - W, P + U + V + W, P + U - V + W, U.GetSafeNormal());
		Quad(Mesh, P - U + V - W, P - U - V - W, P - U - V + W, P - U + V + W, -U.GetSafeNormal());
		Quad(Mesh, P + U + V - W, P - U + V - W, P - U + V + W, P + U + V + W, V.GetSafeNormal());
		Quad(Mesh, P - U - V - W, P + U - V - W, P + U - V + W, P - U - V + W, -V.GetSafeNormal());
	}

	FIntPoint Key(FVector P)
	{
		return FIntPoint(FMath::RoundToInt(P.X * 100), FMath::RoundToInt(P.Y * 100));
	}
}

FMazeInterior FMazeInterior::Build(
    const FMazeLayout& Layout, const FMazeSurface& Walls, float Cell, float Thickness, float Height, int32 Seed)
{
	FMazeInterior Result;
	FRandomStream Random(Seed ^ 0x4c4142);
	const FVector Up(0, 0, 1);
	TMap<FIntPoint, FVector> Joins;

	// Boundary quads are supplied by the existing union mesh, including junctions.
	for (int32 I = 0; I + 3 < Walls.Vertices.Num(); I += 4)
	{
		const FVector N = Walls.Normals[I];

		if (FMath::Abs(N.Z) > 0.1)
			continue;

		Joins.FindOrAdd(Key(Walls.Vertices[I]), FVector::ZeroVector) += N;
		Joins.FindOrAdd(Key(Walls.Vertices[I + 1]), FVector::ZeroVector) += N;
	}

	const auto HasFloor = [&](FVector P)
	{
		const int32 X = FMath::FloorToInt(P.X / Cell), Y = FMath::FloorToInt(P.Y / Cell);

		return X >= 0 && Y >= 0 && X < Layout.Size && Y < Layout.Size && Layout.HasFloor(Y * Layout.Size + X);
	};
	TSet<FIntPoint> FinishedCorners;

	for (int32 I = 0; I + 3 < Walls.Vertices.Num(); I += 4)
	{
		const FVector N = Walls.Normals[I];

		if (FMath::Abs(N.Z) > 0.1)
			continue;

		const FVector A = Walls.Vertices[I], B = Walls.Vertices[I + 1];
		const FVector U = (B - A).GetSafeNormal();
		const FVector Mid = (A + B) * 0.5;
		const FVector JA = Joins.FindChecked(Key(A)), JB = Joins.FindChecked(Key(B));
		const FVector MA = JA / FMath::Max(1.0, FVector::DotProduct(JA, N));
		const FVector MB = JB / FMath::Max(1.0, FVector::DotProduct(JB, N));

		if (HasFloor(Mid + N * 4))
		{
			// Quarter-circle cove, then a vertical upstand and a closed top lip.
			FVector2D Previous(CoveRadiusCm + 0.12f, 0.03f);

			for (int32 Step = 1; Step <= 5; ++Step)
			{
				const float Angle = FMath::Min(Step, 3) * HALF_PI / 3;
				FVector2D Next(0.12f + CoveRadiusCm * (1 - FMath::Sin(Angle)),
				               0.03f + CoveRadiusCm * (1 - FMath::Cos(Angle)));

				if (Step == 4)
					Next = FVector2D(0.12f, CoveHeightCm);

				if (Step == 5)
					Next = FVector2D(0, CoveHeightCm);

				const FVector Normal = (N * (Next.Y - Previous.Y) + Up * (Previous.X - Next.X)).GetSafeNormal();

				Quad(Result.Sections[0],
				     A + MA * Previous.X + Up * Previous.Y,
				     B + MB * Previous.X + Up * Previous.Y,
				     B + MB * Next.X + Up * Next.Y,
				     A + MA * Next.X + Up * Next.Y,
				     Normal);

				if (Step <= 3)
				{
					const float Before = (Step - 1) * HALF_PI / 3;
					const FVector StartNormal = N * FMath::Sin(Before) + Up * FMath::Cos(Before);
					const FVector EndNormal = N * FMath::Sin(Angle) + Up * FMath::Cos(Angle);
					auto& Normals = Result.Sections[0].Normals;
					const int32 Base = Normals.Num() - 4;

					Normals[Base] = Normals[Base + 1] = StartNormal;
					Normals[Base + 2] = Normals[Base + 3] = EndNormal;
				}

				Previous = Next;
			}
		}

		// Convex outside corners only: slim rounded metal tile termination.
		if (FMath::Abs(JA.X) > 0.5 && FMath::Abs(JA.Y) > 0.5 && FVector::DotProduct(JA, U) < -0.5 &&
		    !FinishedCorners.Contains(Key(A)))
		{
			FinishedCorners.Add(Key(A));

			const FVector Other = JA - N;
			const float Bottom = HasFloor(A + JA * 4) ? CoveHeightCm : 0.f;
			FVector Previous = N * CornerRadiusCm;

			for (int32 Step = 1; Step <= 4; ++Step)
			{
				const float Angle = Step * HALF_PI / 4;
				const FVector Next = (N * FMath::Cos(Angle) + Other * FMath::Sin(Angle)) * CornerRadiusCm;

				Quad(Result.Sections[1],
				     A + Previous + Up * Bottom,
				     A + Next + Up * Bottom,
				     A + Next + Up * Height,
				     A + Previous + Up * Height,
				     (Previous + Next).GetSafeNormal());

				auto& Normals = Result.Sections[1].Normals;
				const int32 Base = Normals.Num() - 4;

				Normals[Base] = Normals[Base + 3] = Previous.GetSafeNormal();
				Normals[Base + 1] = Normals[Base + 2] = Next.GetSafeNormal();
				Previous = Next;
			}

			for (const FVector Side : {N, Other})
			{
				const FVector Along = JA - Side;

				Quad(Result.Sections[1],
				     A + Side * CornerRadiusCm + Up * Bottom,
				     A + Side * 0.04 - Along * 1.2 + Up * Bottom,
				     A + Side * 0.04 - Along * 1.2 + Up * Height,
				     A + Side * CornerRadiusCm + Up * Height,
				     Side);
			}
		}

		// Sparse low sockets: stable by seed, shifted along long wall faces.
		if ((B - A).Size() > 150 && HasFloor(Mid + N * 4) && Random.FRand() < 0.035f)
		{
			const FVector P = FMath::Lerp(A, B, Random.FRandRange(0.25f, 0.75f)) + Up * 30.f + N * 0.08f;

			Result.SocketTransforms.Add(FTransform(N.Rotation(), P));
		}
		else if ((B - A).Size() > 150 && HasFloor(Mid + N * 4) && Random.FRand() < 0.02f)
		{
			const FVector P = Mid + Up * 110.f;

			Box(Result.Sections[2], P + N * 0.65, U, Up, N, FVector(8, 8, 1.3));
			Box(Result.Sections[2], P + N * 1.55, U, Up, N, FVector(5, 5.5, 0.6));
		}
	}

	const int32 Count = FMath::Max(3, FMath::RoundToInt(Cell / PanelTargetCm));
	const float Panel = Cell / Count;
	const int32 LampTile = (Count - 1) / 2;
	// Cosmetic mirror of create_ceiling_material.py's diffuser placement/hash.
	const auto Frac = [](float V)
	{
		return V - FMath::FloorToFloat(V);
	};
	const uint32 PatternSeed = static_cast<uint32>(Seed);

	if (LampTile * Panel >= Thickness * 0.5f && (LampTile + 1) * Panel <= Cell - Thickness * 0.5f)
		for (int32 Row = 0; Row < Layout.Size; ++Row)
			for (int32 Col = 0; Col < Layout.Size; ++Col)
			{
				const float PX = Col * Count + LampTile + (PatternSeed & 0xffff) + 19.73f;
				const float PY = Row * Count + LampTile + (PatternSeed >> 16) + 19.73f;
				float XHash = Frac(PX * 0.1031f), YHash = Frac(PY * 0.1031f), ZHash = XHash;
				const float Dot = XHash * (YHash + 33.33f) + YHash * (ZHash + 33.33f) + ZHash * (XHash + 33.33f);
				XHash += Dot;
				YHash += Dot;
				ZHash += Dot;

				if (Frac((XHash + YHash) * ZHash) >= 0.18f)
					Result.LampLocations.Emplace(
					    Col * Cell + (LampTile + 0.5f) * Panel, Row * Cell + (LampTile + 0.5f) * Panel, Height - 3.f);
			}

	const FVector X(1, 0, 0), Y(0, 1, 0), Down(0, 0, -1);
	// Opposite the luminous tile: never cover a diffuser or cross a panel rail.
	const int32 Tile = Count - 2;

	if (Tile * Panel >= Thickness * 0.5 && (Tile + 1) * Panel <= Cell - Thickness * 0.5 && Tile != (Count - 1) / 2)
		for (int32 Row = 0; Row < Layout.Size; ++Row)
			for (int32 Col = 0; Col < Layout.Size; ++Col)
			{
				const float Choice = Random.FRand();

				if (Choice >= 0.15f)
					continue;

				const FVector P(Col * Cell + (Tile + 0.5f) * Panel, Row * Cell + (Tile + 0.5f) * Panel, Height);

				if (Choice < 0.075f)
				{
					Box(Result.Sections[2], P + Down * 0.45, X, Y, Down, FVector(52, 52, 0.9));
					Box(Result.Sections[3], P + Down * 0.95, X, Y, Down, FVector(46, 46, 0.2));

					for (int32 Slat = 0; Slat < 13; ++Slat)
						Box(Result.Sections[2],
						    P + Y * ((Slat - 6) * 3.4) + Down * 1.25,
						    X,
						    Y,
						    Down,
						    FVector(45, 1.9, 0.5));
				}
				else if (Choice < 0.11f)
				{
					Box(Result.Sections[1], P + Down * 0.2, X, Y, Down, FVector(57, 57, 0.4));
					Box(Result.Sections[2], P + Down * 0.5, X, Y, Down, FVector(55, 55, 0.5));
					Box(Result.Sections[1], P + Y * 21 + Down * 0.85, X, Y, Down, FVector(7, 1.2, 0.4));
				}
				else
				{
					Result.DetectorTransforms.Add(FTransform(FRotator(0, Random.FRandRange(0.f, 360.f), 0), P));
				}
			}

	return Result;
}
