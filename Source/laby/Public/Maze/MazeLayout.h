#pragma once

#include "CoreMinimal.h"

// Pure deterministic topology, shared by authority, clients and automation tests.
struct FMazeLayout
{
	static constexpr int32 DefaultSize = 80;
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
		return (Size / 2) * Size + Size / 2;
	}

	void Generate(int32 Seed, int32 InSize = DefaultSize)
	{
		Size = FMath::Clamp(InSize, 8, 100);
		FRandomStream Random(Seed);
		Walls.Init(15, Size * Size); // north, east, south, west
		Exits.Reset();
		TArray<bool> Visited;
		Visited.Init(false, Walls.Num());
		TArray<int32> Stack;
		Stack.Add(Start());
		Visited[Start()] = true;
		const int32 DX[] = {0, 1, 0, -1};
		const int32 DY[] = {-1, 0, 1, 0};

		while (!Stack.IsEmpty())
		{
			int32 Current = Stack.Last();
			TArray<int32, TInlineAllocator<4>> Choices;

			for (int32 D = 0; D < 4; ++D)
			{
				int32 X = Current % Size + DX[D], Y = Current / Size + DY[D];

				if (X >= 0 && X < Size && Y >= 0 && Y < Size && !Visited[Y * Size + X])
					Choices.Add(D);
			}

			if (Choices.IsEmpty())
			{
				Stack.Pop();
				continue;
			}

			int32 D = Choices[Random.RandRange(0, Choices.Num() - 1)];
			int32 Next = Current + DY[D] * Size + DX[D];
			Walls[Current] &= ~(1 << D);
			Walls[Next] &= ~(1 << ((D + 2) % 4));
			Visited[Next] = true;
			Stack.Add(Next);
		}

		// A few loops shorten excessive dead ends without losing the maze character.
		for (int32 Y = 1; Y < Size - 1; ++Y)
			for (int32 X = 1; X < Size - 1; ++X)
				if (Random.FRand() < 0.07f)
				{
					int32 C = Y * Size + X;
					Walls[C] &= ~2;
					Walls[C + 1] &= ~8;
				}

		// One opening on the north boundary, away from corners.
		Exits.Add(Random.RandRange(2, Size - 3));
		Walls[Exits[0]] &= ~1;
		GenerateRoomsAndHoles(Random);
	}

private:
	void CarveRoom(const FIntRect& Room)
	{
		Rooms.Add(Room);

		for (int32 Y = Room.Min.Y; Y < Room.Max.Y; ++Y)
			for (int32 X = Room.Min.X; X < Room.Max.X; ++X)
			{
				const int32 C = Y * Size + X;

				if (X + 1 < Room.Max.X)
				{
					Walls[C] &= ~2;
					Walls[C + 1] &= ~8;
				}

				if (Y + 1 < Room.Max.Y)
				{
					Walls[C] &= ~4;
					Walls[C + Size] &= ~1;
				}
			}
	}

	void GenerateRoomsAndHoles(FRandomStream& Random)
	{
		Rooms.Reset();
		Holes.Init(0, Walls.Num());
		const int32 Center = Size / 2;
		CarveRoom(FIntRect(Center - 1, Center - 1, Center + 2, Center + 2));
		const int32 TargetRooms = FMath::Max(2, Size * Size / 350);

		for (int32 Attempt = 0; Attempt < TargetRooms * 20 && Rooms.Num() < TargetRooms; ++Attempt)
		{
			const int32 Width = Random.RandRange(3, FMath::Min(7, Size - 2));
			const int32 Height = Random.RandRange(3, FMath::Min(7, Size - 2));
			const int32 X = Random.RandRange(1, Size - Width - 1);
			const int32 Y = Random.RandRange(1, Size - Height - 1);
			const FIntRect Room(X, Y, X + Width, Y + Height);
			const bool bOverlaps = Rooms.ContainsByPredicate(
			    [&Room](const FIntRect& Other)
			    {
				    return Room.Min.X <= Other.Max.X && Room.Max.X >= Other.Min.X && Room.Min.Y <= Other.Max.Y &&
				           Room.Max.Y >= Other.Min.Y;
			    });

			if (!bOverlaps)
				CarveRoom(Room);
		}

		TArray<int32> Candidates;

		for (int32 Y = 1; Y < Size - 1; ++Y)
			for (int32 X = 1; X < Size - 1; ++X)
				if (FMath::Abs(X - Center) > 2 || FMath::Abs(Y - Center) > 2)
					Candidates.Add(Y * Size + X);

		for (int32 I = Candidates.Num() - 1; I > 0; --I)
			Candidates.Swap(I, Random.RandRange(0, I));

		const int32 TargetHoles = FMath::Max(1, Size * Size / 100);
		int32 HoleCount = 0;
		const int32 Attempts = FMath::Min(Candidates.Num(), TargetHoles * 12);

		for (int32 I = 0; I < Attempts && HoleCount < TargetHoles; ++I)
		{
			const int32 C = Candidates[I];
			bool bAdjacentHole = false;

			for (int32 DY = -1; DY <= 1; ++DY)
				for (int32 DX = -1; DX <= 1; ++DX)
					bAdjacentHole |= Holes[C + DY * Size + DX] != 0;

			if (bAdjacentHole)
				continue;

			Holes[C] = 1;

			// Reject removal of an articulation cell: every remaining floor tile,
			// including the protected boundary exit, must remain reachable on foot.
			if (ReachableFloorCount() == Walls.Num() - HoleCount - 1)
				++HoleCount;
			else
				Holes[C] = 0;
		}
	}
};
