#pragma once

#include "CoreMinimal.h"

// Pure deterministic topology, shared by authority, clients and automation tests.
struct FMazeLayout
{
	int32 Size = 40;
	TArray<uint8> Walls;
	TArray<int32> Exits;
	int32 Start() const { return (Size / 2) * Size + Size / 2; }
	void Generate(int32 Seed, int32 InSize = 40)
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
				if (X >= 0 && X < Size && Y >= 0 && Y < Size && !Visited[Y * Size + X]) Choices.Add(D);
			}
			if (Choices.IsEmpty()) { Stack.Pop(); continue; }
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
		// Exactly three openings on separate sides, away from corners.
		Exits.Add(Random.RandRange(2, Size - 3));
		Exits.Add(Random.RandRange(2, Size - 3) * Size + Size - 1);
		Exits.Add((Size - 1) * Size + Random.RandRange(2, Size - 3));
		for (int32 D = 0; D < 3; ++D) Walls[Exits[D]] &= ~(1 << D);

	}
};
