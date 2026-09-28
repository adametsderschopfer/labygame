#include "Maze/MazeLayout.h"
#include "Maze/MazeRoomDefinition.h"

void FMazeLayout::AddRoomsAtDeadEnds(FRandomStream& Random, const TArray<int32>& ScenicFloor)
{
	TArray<bool> Protected;

	Protected.Init(false, Walls.Num());

	for (const int32 Cell : ScenicFloor)
		Protected[Cell] = true;

	const int32 SectorCount = FMath::DivideAndRoundUp(Size, FMazeRoomDefinition::DeadEndSectorSide);

	// Rebuild candidates for every slot so new rooms also participate in spacing checks.
	for (int32 Slot = 0; Slot < FMazeRoomDefinition::RoomsPerSector; ++Slot)
		for (int32 SY = 0; SY < SectorCount; ++SY)
			for (int32 SX = 0; SX < SectorCount; ++SX)
			{
				TArray<FIntPoint> Candidates;

				for (int32 Y = FMath::Max(1, SY * Size / SectorCount);
				     Y < FMath::Min(Size - 1, (SY + 1) * Size / SectorCount);
				     ++Y)
					for (int32 X = FMath::Max(1, SX * Size / SectorCount);
					     X < FMath::Min(Size - 1, (SX + 1) * Size / SectorCount);
					     ++X)
					{
						const int32 Cell = Y * Size + X;
						const FIntRect Room(X, Y, X + 1, Y + 1);
						const uint8 Open = ~Walls[Cell] & 15;
						const bool bDeadEnd = Open != 0 && (Open & (Open - 1)) == 0;

						if (Protected[Cell] || !bDeadEnd || OverlapsEntrance(Room))
							continue;

						// Keep at least one corridor cell between rooms, including across sectors.
						if (Rooms.ContainsByPredicate(
						        [&](const FIntRect& Existing)
						        {
							        return X >= Existing.Min.X - 1 && X <= Existing.Max.X && Y >= Existing.Min.Y - 1 &&
							               Y <= Existing.Max.Y;
						        }))
							continue;

						Candidates.Emplace(X, Y);
					}

				if (!Candidates.IsEmpty())
				{
					const FIntPoint P = Candidates[Random.RandRange(0, Candidates.Num() - 1)];
					// The existing corridor connection becomes the room's only doorway.
					Rooms.Emplace(P, P + FIntPoint(1, 1));
				}
			}
}
