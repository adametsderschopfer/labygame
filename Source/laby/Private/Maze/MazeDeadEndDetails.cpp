#include "Maze/MazeDeadEndDetails.h"
#include "Maze/MazeInterior.h"
#include "Maze/MazeMeshPrimitives.h"
#include "Maze/MazeRouteDefinition.h"

void MazeDeadEndDetails::Append(FMazeInterior& Interior,
                                const FMazeLayout& Layout,
                                float Cell,
                                float Thickness,
                                float Height,
                                int32 Seed,
                                FIntRect Cells)
{
	using MazeMeshPrimitives::Box;

	// These are wall-mounted, non-interactive landmarks, derived from the immutable
	// generation. They neither own inventory nor promise a collectible in every end.
	if (Cell - Thickness < 180.f || Height < 220.f)
		return;

	const bool Region = Cells.Width() > 0 && Cells.Height() > 0;
	const FIntPoint Steps[] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
	const FVector Up(0, 0, 1);

	for (int32 Y = Region ? FMath::Max(1, Cells.Min.Y) : 1;
	     Y < (Region ? FMath::Min(Layout.Size - 1, Cells.Max.Y) : Layout.Size - 1);
	     ++Y)
		for (int32 X = Region ? FMath::Max(1, Cells.Min.X) : 1;
		     X < (Region ? FMath::Min(Layout.Size - 1, Cells.Max.X) : Layout.Size - 1);
		     ++X)
		{
			const FIntPoint Tile(X, Y);
			const int32 Index = Y * Layout.Size + X;

			if (!Layout.HasFloor(Index) || Layout.Rooms.ContainsByPredicate(
			                                   [&](const FIntRect& Room)
			                                   {
				                                   return Room.Contains(Tile);
			                                   }))
				continue;

			int32 Opening = INDEX_NONE, OpenCount = 0;

			for (int32 D = 0; D < 4; ++D)
				if (!(Layout.Walls[Index] & (1 << D)))
				{
					Opening = D;
					++OpenCount;
				}

			if (OpenCount != 1 || !Layout.HasFloor(Index + Steps[Opening].Y * Layout.Size + Steps[Opening].X))
				continue;

			FRandomStream Random(
			    static_cast<int32>(uint32(Seed) ^ uint32(X) * 73856093u ^ uint32(Y) * 19349663u ^ 0x72E49AB5u));

			if (Random.FRand() >= FMazeRouteDefinition::DeadEndDetailFraction)
				continue;

			const FVector Normal(Steps[Opening].X, Steps[Opening].Y, 0);
			const FVector Along(-Normal.Y, Normal.X, 0);
			const FVector Wall = FVector((X + 0.5f) * Cell, (Y + 0.5f) * Cell, 0) - Normal * ((Cell - Thickness) / 2);
			const int32 Kind = Random.RandRange(0, 2);

			if (Kind == 0)
			{
				// A service cabinet with an exposed recess, louvres and a missing cover.
				const FVector P = Wall + Up * 130;
				Box(Interior.Sections[1], P + Normal * 3.1, Along, Up, Normal, FVector(94, 106, 6));
				Box(Interior.Sections[3], P + Normal * 6.3, Along, Up, Normal, FVector(85, 97, 0.4));
				Box(Interior.Sections[2], P - Along * 25 + Normal * 7, Along, Up, Normal, FVector(31, 92, 1));

				for (int32 Slat = 0; Slat < 6; ++Slat)
					Box(Interior.Sections[1],
					    P + Along * 12 + Up * (Slat * 11 - 29) + Normal * 6.9,
					    Along,
					    Up,
					    Normal,
					    FVector(43, 2, 1));

				Box(Interior.Sections[1], P - Along * 18 + Up * 7 + Normal * 8.5, Along, Up, Normal, FVector(3, 16, 3));
			}
			else if (Kind == 1)
			{
				// A shallow shelf and abandoned folders. It stays within the wall-side
				// capsule clearance, so presentation geometry cannot obstruct traversal.
				Box(Interior.Sections[1], Wall + Up * 105 + Normal * 10.1, Along, Up, Normal, FVector(106, 4, 20));

				for (const float Side : {-40.f, 40.f})
					Box(Interior.Sections[1],
					    Wall + Along * Side + Up * 89 + Normal * 7,
					    Along,
					    Up,
					    Normal,
					    FVector(3, 28, 13));

				for (int32 Folder = 0; Folder < 5; ++Folder)
				{
					const float FolderHeight = Random.FRandRange(23.f, 32.f);
					const FVector P = Wall + Along * (Folder * 14 - 34) + Up * (107 + FolderHeight / 2) + Normal * 8;
					Box(Interior.Sections[Folder % 2 ? 2 : 3], P, Along, Up, Normal, FVector(8, FolderHeight, 14));
					Box(Interior.Sections[2], P + Normal * 7.15, Along, Up, Normal, FVector(5, 6, 0.2));
				}

				Box(Interior.Sections[2],
				    Wall + Along * 35 + Up * 107.5 + Normal * 10,
				    Along,
				    Normal,
				    Up,
				    FVector(21, 15, 0.6));
			}
			else
			{
				// Incomplete tally marks give a place an identity without readable quest text.
				const int32 Groups = Random.RandRange(2, 3);

				for (int32 Group = 0; Group < Groups; ++Group)
				{
					const FVector P =
					    Wall + Along * (Group * 38 - 38) + Up * Random.FRandRange(135.f, 143.f) + Normal * 0.25;
					const int32 Strokes = Group + 1 == Groups ? Random.RandRange(1, 3) : 4;

					for (int32 Stroke = 0; Stroke < Strokes; ++Stroke)
						Box(Interior.Sections[3],
						    P + Along * (Stroke * 7),
						    Along,
						    Up,
						    Normal,
						    FVector(1.4, Random.FRandRange(23.f, 30.f), 0.25));

					if (Strokes == 4)
						Box(Interior.Sections[3],
						    P + Along * 10.5 + Normal * 0.2,
						    Along * 0.8 + Up * 0.6,
						    -Along * 0.6 + Up * 0.8,
						    Normal,
						    FVector(35, 1.5, 0.25));
				}
			}
		}
}
