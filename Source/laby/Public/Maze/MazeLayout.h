#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Maze/MazeRoomDefinition.h"

struct FMazeRoomDoorway
{
	FIntPoint Cell;
	int32 Direction = 0; // north, east, south, west
	int32 RoomIndex = INDEX_NONE;
};

// Pure deterministic topology, shared by authority, clients and automation tests.
struct FMazeLayout
{
	static constexpr int32 DefaultSize = 80;
	static constexpr int32 EntranceLengthCells = 2;
	int32 Size = DefaultSize;
	TArray<uint8> Walls;
	TArray<int32> Exits;
	TArray<FIntRect> Rooms;
	// Canonical room presentation/geometry kind, aligned one-to-one with Rooms.
	TArray<EMazeRoomType> RoomTypes;
	TArray<uint8> Holes;
	// 0 = ordinary floor, 1 = narrow east-west passage, 2 = narrow north-south passage.
	TArray<uint8> NarrowPassages;
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
		return (Size - 2) * Size;
	}

	void Generate(int32 Seed, int32 InSize = DefaultSize);
	// Derived from room bounds and the canonical wall bits; never an independent cache.
	TArray<FMazeRoomDoorway> RoomDoorways() const;
	// Per-corridor-cell bits pointing back through room doorways; derived, never stored.
	TArray<uint8> RoomDoorApproachSides() const;
	EMazeRoomType RoomType(int32 RoomIndex) const
	{
		return RoomTypes.IsValidIndex(RoomIndex) ? RoomTypes[RoomIndex] : EMazeRoomType::Empty;
	}

private:
	bool IsEntranceCell(int32 X, int32 Y) const;
	bool OverlapsEntrance(const FIntRect& Room) const;
	void ReserveRooms(FRandomStream& Random);
	void AddRoomsAtDeadEnds(FRandomStream& Random, const TArray<int32>& ScenicFloor);
	void GenerateRoomTypes(int32 Seed);
	void GenerateNarrowPassages(int32 Seed, const TArray<int32>& ScenicFloor);
	void CarveEntrance();
	void CarveRoom(const FIntRect& Room);
	void GenerateHoles(FRandomStream& Random, const TArray<int32>& ScenicFloor);
};
