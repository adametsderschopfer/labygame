#include "Maze/MazeLayout.h"
#include "Maze/MazeRoomDefinition.h"

void FMazeLayout::AddRoomsAtDeadEnds(FRandomStream& Random, const TArray<int32>& ScenicFloor)
{
	TArray<bool> Protected;

	Protected.Init(false, Walls.Num());

	for (const int32 Cell : ScenicFloor)
		Protected[Cell] = true;

	const int32 SectorCount = FMath::DivideAndRoundUp(Size, FMazeRoomDefinition::DeadEndSectorSide);
	const int32 DX[] = {0, 1, 0, -1}, DY[] = {-1, 0, 1, 0};
	const auto CanPlaceRoom = [&](const FIntRect& Room)
	{
		return !OverlapsEntrance(Room) && !Rooms.ContainsByPredicate(
		                                      [&](const FIntRect& Existing)
		                                      {
			                                      return Room.Min.X <= Existing.Max.X && Room.Max.X >= Existing.Min.X &&
			                                             Room.Min.Y <= Existing.Max.Y && Room.Max.Y >= Existing.Min.Y;
		                                      });
	};

	// Rebuild candidates for every slot so new rooms also participate in spacing checks.
	for (int32 Slot = 0; Slot < FMazeRoomDefinition::RoomsPerSector; ++Slot)
		for (int32 SY = 0; SY < SectorCount; ++SY)
			for (int32 SX = 0; SX < SectorCount; ++SX)
			{
				TArray<FIntRect> Candidates;

				for (int32 Y = FMath::Max(1, SY * Size / SectorCount);
				     Y < FMath::Min(Size - 1, (SY + 1) * Size / SectorCount);
				     ++Y)
					for (int32 X = FMath::Max(1, SX * Size / SectorCount);
					     X < FMath::Min(Size - 1, (SX + 1) * Size / SectorCount);
					     ++X)
					{
						const int32 Cell = Y * Size + X;
						const uint8 Open = ~Walls[Cell] & 15;
						const bool bDeadEnd = Open != 0 && (Open & (Open - 1)) == 0;

						if (Protected[Cell] || !bDeadEnd)
							continue;

						for (int32 Direction = 0; Direction < 4; ++Direction)
							if (Open & (1 << Direction))
							{
								for (int32 Length = 1; Length <= FMazeRoomDefinition::MaxDeadEndRoomLength; ++Length)
								{
									const int32 EndX = X + (Length - 1) * DX[Direction];
									const int32 EndY = Y + (Length - 1) * DY[Direction];

									if (EndX < 1 || EndY < 1 || EndX >= Size - 1 || EndY >= Size - 1)
										break;

									const int32 EndCell = EndY * Size + EndX;
									const uint8 ExpectedOpen = Length == 1 ? Open : Open | (1 << ((Direction + 2) % 4));

									if (Protected[EndCell] || (~Walls[EndCell] & 15) != ExpectedOpen)
										break;

									const FIntRect Room(FMath::Min(X, EndX),
									                    FMath::Min(Y, EndY),
									                    FMath::Max(X, EndX) + 1,
									                    FMath::Max(Y, EndY) + 1);

									// Keep a corridor cell between rooms, including across sectors.
									if (!CanPlaceRoom(Room))
										break;

									Candidates.Add(Room);
								}

								break;
							}
					}

				if (!Candidates.IsEmpty())
				{
					// A straight dead-end branch has only one connection to the corridor.
					Rooms.Add(Candidates[Random.RandRange(0, Candidates.Num() - 1)]);
				}
			}
}
