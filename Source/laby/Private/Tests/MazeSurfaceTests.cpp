#include "Maze/MazeSurface.h"
#include "Maze/MazeRoomDefinition.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMazeSurfaceTest,
                                 "Laby.Maze.WatertightSurface",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMazeSurfaceTest::RunTest(const FString& Parameters)
{
	for (int32 Seed : {1, 42, 98765})
	{
		FMazeLayout Layout;

		Layout.Generate(Seed);

		FMazeSurface Surface;

		Surface.Build(Layout, 875, 50, 1000);

		TMap<FVector, int32> VertexIds;
		TMap<uint64, int32> EdgeCounts;
		TSet<FString> Faces;

		for (int32 I = 0; I < Surface.Triangles.Num(); I += 3)
		{
			int32 Ids[3];

			for (int32 J = 0; J < 3; ++J)
			{
				FVector V = Surface.Vertices[Surface.Triangles[I + J]];

				if (!VertexIds.Contains(V))
					VertexIds.Add(V, VertexIds.Num());

				Ids[J] = VertexIds[V];

				if (V.Z != 0 && V.Z != FMazeRoomDefinition::DoorHeight && V.Z != 1000)
				{
					AddError(TEXT("Wrong wall height"));

					return false;
				}
			}

			for (int32 J = 0; J < 3; ++J)
			{
				uint32 A = FMath::Min(Ids[J], Ids[(J + 1) % 3]), B = FMath::Max(Ids[J], Ids[(J + 1) % 3]);

				++EdgeCounts.FindOrAdd((uint64(A) << 32) | B);
			}

			if (Ids[0] > Ids[1])
				Swap(Ids[0], Ids[1]);

			if (Ids[1] > Ids[2])
				Swap(Ids[1], Ids[2]);

			if (Ids[0] > Ids[1])
				Swap(Ids[0], Ids[1]);

			FString Key = FString::Printf(TEXT("%d/%d/%d"), Ids[0], Ids[1], Ids[2]);

			if (Faces.Contains(Key))
			{
				AddError(TEXT("Overlapping duplicate face"));

				return false;
			}

			Faces.Add(Key);
		}

		for (const auto& Edge : EdgeCounts)
			if (Edge.Value != 2)
			{
				AddError(TEXT("Open seam or internal shared face"));

				return false;
			}
	}

	return true;
}

#endif
