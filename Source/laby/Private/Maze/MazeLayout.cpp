#include "Maze/MazeLayout.h"

bool FMazeLayout::IsEntranceCell(int32 X, int32 Y) const
{
	const bool bRoom = X >= 0 && X < EntranceRoomSize && Y >= Size - EntranceRoomSize && Y < Size;
	const bool bPassage = X >= EntranceRoomSize && X < EntranceRoomSize + EntrancePassageLength && Y == Start() / Size;

	return bRoom || bPassage;
}

bool FMazeLayout::OverlapsEntrance(const FIntRect& Room) const
{
	// Keep a one-cell margin, including the passage's connection to the maze.
	for (int32 Y = Room.Min.Y - 1; Y <= Room.Max.Y; ++Y)
		for (int32 X = Room.Min.X - 1; X <= Room.Max.X; ++X)
			if (IsEntranceCell(X, Y))
				return true;

	return false;
}

void FMazeLayout::CarveEntrance()
{
	// These cells were excluded from the random maze so the room has exactly one door.
	CarveRoom(FIntRect(0, Size - EntranceRoomSize, EntranceRoomSize, Size));

	const int32 Y = Start() / Size;

	for (int32 X = EntranceRoomSize - 1; X < EntranceRoomSize + EntrancePassageLength; ++X)
	{
		const int32 C = Y * Size + X;

		Walls[C] &= ~2;
		Walls[C + 1] &= ~8;
	}
}
