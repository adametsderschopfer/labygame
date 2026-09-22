#include "Maze/MazeMeshPrimitives.h"
#include "Maze/MazeSurface.h"

namespace MazeMeshPrimitives
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
}
