#include "Maze/MazeLayout.h"
#include "Maze/MazeRoomDefinition.h"

void FMazeLayout::AddRoomsAtBends(FRandomStream& Random, const TArray<int32>& ScenicFloor)
{
	TArray<bool> Protected;

	Protected.Init(false, Walls.Num());

	for (const int32 Cell : ScenicFloor)
		Protected[Cell] = true;

	const auto IsBend = [&](int32 Cell)
	{
		const uint8 Open = ~Walls[Cell] & 15;

		return Open == 3 || Open == 6 || Open == 9 || Open == 12;
	};
	const int32 SectorCount = FMath::DivideAndRoundUp(Size, FMazeRoomDefinition::BendSectorSide);
	const int32 Offsets[] = {-Size, 1, Size, -1};

	// Rebuild candidates for every slot so new rooms also participate in spacing checks.
	for (int32 Slot = 0; Slot < FMazeRoomDefinition::RoomsPerSector; ++Slot)
		for (int32 SY = 0; SY < SectorCount; ++SY)
			for (int32 SX = 0; SX < SectorCount; ++SX)
			{
				TArray<FIntPoint> Candidates, Zigzags, SideRooms;

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
						const bool bSideRoom = Open != 0 && (Open & (Open - 1)) == 0;

						if (Protected[Cell] || (!IsBend(Cell) && !bSideRoom) || OverlapsEntrance(Room))
							continue;

						// Keep at least one corridor cell between rooms, including across sectors.
						if (Rooms.ContainsByPredicate(
						        [&](const FIntRect& Existing)
						        {
							        return X >= Existing.Min.X - 1 && X <= Existing.Max.X && Y >= Existing.Min.Y - 1 &&
							               Y <= Existing.Max.Y;
						        }))
							continue;

						if (!bSideRoom)
							Candidates.Emplace(X, Y);

						for (int32 D = 0; D < 4; ++D)
							if (!(Walls[Cell] & (1 << D)) && IsBend(Cell + Offsets[D]))
							{
								if (bSideRoom)
									SideRooms.Emplace(X, Y);
								else
									Zigzags.Emplace(X, Y);

								break;
							}
					}

				const auto& ThroughRooms = Zigzags.IsEmpty() ? Candidates : Zigzags;
				const bool bChooseThrough =
				    SideRooms.IsEmpty() ||
				    (!ThroughRooms.IsEmpty() && Random.FRand() < FMazeRoomDefinition::BendThroughFraction);
				const auto& Choices = bChooseThrough ? ThroughRooms : SideRooms;

				if (!Choices.IsEmpty())
				{
					const FIntPoint P = Choices[Random.RandRange(0, Choices.Num() - 1)];
					// Preserve one entrance for a side room, or two for a through-room.
					// RoomDoorways adds frames without cutting any existing corridor connections.
					Rooms.Emplace(P, P + FIntPoint(1, 1));
				}
			}
}
