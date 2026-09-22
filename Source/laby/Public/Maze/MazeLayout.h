#pragma once

#include "CoreMinimal.h"

struct FMazeRoomDoorway
{
	FIntPoint Cell;
	int32 Direction = 0; // north, east, south, west
};

// Pure deterministic topology, shared by authority, clients and automation tests.
struct FMazeLayout
{
	static constexpr int32 DefaultSize = 80;
	static constexpr int32 EntranceRoomSize = 3;
	static constexpr int32 EntrancePassageLength = 2;
	int32 Size = DefaultSize;
	TArray<uint8> Walls;
	TArray<int32> Exits;
	TArray<FIntRect> Rooms;
	TArray<uint8> Holes;
	int32 NumHoles() const
	{
		int32 Count = 0;

		for (uint8 Hole : Holes)
			Count += Hole != 0;

		return Count;
	}

	bool HasFloor(int32 CellIndex) const
	{
		return Holes.IsValidIndex(CellIndex) && Holes[CellIndex] == 0;
	}

	int32 ReachableFloorCount() const
	{
		if (!HasFloor(Start()))
			return 0;

		TArray<uint8> Seen;
		Seen.Init(0, Walls.Num());
		TArray<int32> Queue;
		Queue.Reserve(Walls.Num());
		Queue.Add(Start());
		Seen[Start()] = 1;
		const int32 DX[] = {0, 1, 0, -1};
		const int32 DY[] = {-1, 0, 1, 0};

		for (int32 Head = 0; Head < Queue.Num(); ++Head)
		{
			const int32 CellIndex = Queue[Head];

			for (int32 D = 0; D < 4; ++D)
			{
				if (Walls[CellIndex] & (1 << D))
					continue;

				const int32 X = CellIndex % Size + DX[D], Y = CellIndex / Size + DY[D];

				if (X < 0 || Y < 0 || X >= Size || Y >= Size)
					continue;

				const int32 Next = Y * Size + X;

				if (Seen[Next] || !HasFloor(Next))
					continue;

				Seen[Next] = 1;
				Queue.Add(Next);
			}
		}

		return Queue.Num();
	}

	int32 Start() const
	{
		return (Size - 1 - EntranceRoomSize / 2) * Size + EntranceRoomSize / 2;
	}

	void Generate(int32 Seed, int32 InSize = DefaultSize);
	// Derived from room bounds and the canonical wall bits; never an independent cache.
	TArray<FMazeRoomDoorway> RoomDoorways() const;

private:
	bool IsEntranceCell(int32 X, int32 Y) const;
	bool OverlapsEntrance(const FIntRect& Room) const;
	void ReserveRooms(FRandomStream& Random);
	void AddRoomsAtBends(FRandomStream& Random, const TArray<int32>& ScenicFloor);
	void CarveEntrance();
	void CarveRoom(const FIntRect& Room);
	void GenerateHoles(FRandomStream& Random, const TArray<int32>& ScenicFloor);
};
