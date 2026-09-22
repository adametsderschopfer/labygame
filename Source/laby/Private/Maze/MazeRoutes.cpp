#include "Maze/MazeRoutes.h"
#include "Maze/MazeLayout.h"
#include "Maze/MazeRouteDefinition.h"

namespace
{
	const FIntPoint Steps[] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};

	// Temporary generation workspace, never retained by ECS or a presentation adapter.
	struct FRouteBuilder
	{
		FMazeLayout& Layout;
		const TArray<bool>& Reserved;
		FRandomStream& Random;

		int32 Neighbor(int32 Cell, int32 Direction) const
		{
			const FIntPoint P(Cell % Layout.Size + Steps[Direction].X, Cell / Layout.Size + Steps[Direction].Y);

			return P.X >= 0 && P.Y >= 0 && P.X < Layout.Size && P.Y < Layout.Size ? P.Y * Layout.Size + P.X
			                                                                      : INDEX_NONE;
		}

		bool Corridor(int32 Cell) const
		{
			return Reserved.IsValidIndex(Cell) && !Reserved[Cell];
		}

		void SetOpen(int32 Cell, int32 Direction, bool Open = true)
		{
			const int32 Next = Neighbor(Cell, Direction);
			const uint8 Bit = 1 << Direction, Opposite = 1 << ((Direction + 2) % 4);

			if (Open)
			{
				Layout.Walls[Cell] &= ~Bit;
				Layout.Walls[Next] &= ~Opposite;
			}
			else
			{
				Layout.Walls[Cell] |= Bit;
				Layout.Walls[Next] |= Opposite;
			}
		}

		int32 Degree(int32 Cell) const
		{
			int32 Count = 0;

			for (int32 D = 0; D < 4; ++D)
				Count += !(Layout.Walls[Cell] & (1 << D)) && Corridor(Neighbor(Cell, D));

			return Count;
		}

		int32 RayLength(int32 Cell, int32 Direction) const
		{
			int32 Count = 0;

			while (!(Layout.Walls[Cell] & (1 << Direction)) && Corridor(Neighbor(Cell, Direction)))
			{
				Cell = Neighbor(Cell, Direction);
				++Count;
			}

			return Count;
		}

		int32 StraightLength(int32 Cell, int32 Direction) const
		{
			return 1 + RayLength(Cell, Direction) + RayLength(Cell, (Direction + 2) % 4);
		}

		void Tree(int32 Start)
		{
			TArray<bool> Visited = Reserved;
			TArray<int32> Stack, Arrival, Straight;
			Arrival.Init(INDEX_NONE, Reserved.Num());
			Straight.Init(0, Reserved.Num());
			Visited[Start] = true;
			Stack.Add(Start);

			while (!Stack.IsEmpty())
			{
				const int32 Cell = Stack.Last();
				TArray<int32, TInlineAllocator<4>> Choices;

				for (int32 D = 0; D < 4; ++D)
				{
					const int32 Next = Neighbor(Cell, D);

					if (Corridor(Next) && !Visited[Next])
						Choices.Add(D);
				}

				if (Choices.IsEmpty())
				{
					Stack.Pop();
					continue;
				}

				if (Choices.Num() > 1 && Straight[Cell] >= 2 &&
				    (Straight[Cell] >= FMazeRouteDefinition::OrdinaryStraightCells - 1 ||
				     Random.FRand() < FMazeRouteDefinition::TurnPreference))
					Choices.Remove(Arrival[Cell]);

				const int32 Direction = Choices[Random.RandRange(0, Choices.Num() - 1)];
				const int32 Next = Neighbor(Cell, Direction);
				SetOpen(Cell, Direction);
				Visited[Next] = true;
				Arrival[Next] = Direction;
				Straight[Next] = Arrival[Cell] == Direction ? Straight[Cell] + 1 : 1;
				Stack.Add(Next);
			}
		}

		void Braid()
		{
			TArray<int32> Ends;

			for (int32 Cell = 0; Cell < Reserved.Num(); ++Cell)
				if (Corridor(Cell) && Degree(Cell) == 1)
					Ends.Add(Cell);

			for (int32 I = Ends.Num() - 1; I > 0; --I)
				Ends.Swap(I, Random.RandRange(0, I));

			const int32 Target = FMath::RoundToInt(Ends.Num() * FMazeRouteDefinition::BraidFraction);
			int32 Opened = 0;

			for (const int32 Cell : Ends)
			{
				if (Opened >= Target)
					break;

				if (Degree(Cell) != 1)
					continue;

				TArray<int32, TInlineAllocator<4>> Choices;

				for (int32 D = 0; D < 4; ++D)
				{
					const int32 Next = Neighbor(Cell, D);

					if (!Corridor(Next) || !(Layout.Walls[Cell] & (1 << D)))
						continue;

					const int32 Length = 2 + RayLength(Cell, (D + 2) % 4) + RayLength(Next, D);

					if (Length <= FMazeRouteDefinition::OrdinaryStraightCells)
						Choices.Add(D);
				}

				if (!Choices.IsEmpty())
				{
					const int32 Direction = Choices[Random.RandRange(0, Choices.Num() - 1)];
					Opened += Degree(Neighbor(Cell, Direction)) == 1 ? 2 : 1;
					SetOpen(Cell, Direction);
				}
			}
		}

		bool Connected(int32 Start, int32 Target) const
		{
			TArray<bool> Seen = Reserved;
			TArray<int32> Queue;
			Queue.Add(Start);
			Seen[Start] = true;

			for (int32 Head = 0; Head < Queue.Num(); ++Head)
				for (int32 D = 0; D < 4; ++D)
				{
					const int32 Next = Neighbor(Queue[Head], D);

					if ((Layout.Walls[Queue[Head]] & (1 << D)) || !Corridor(Next) || Seen[Next])
						continue;

					if (Next == Target)
						return true;

					Seen[Next] = true;
					Queue.Add(Next);
				}

			return false;
		}

		bool BreakStraight(int32 Cell, int32 Direction)
		{
			const int32 Next = Neighbor(Cell, Direction);
			const int32 MinDegree = FMath::Min(2, Degree(Cell)), MinNextDegree = FMath::Min(2, Degree(Next));

			// Replace an edge with a three-edge bend; the original connection survives.
			for (const int32 Side : {(Direction + 1) % 4, (Direction + 3) % 4})
			{
				const int32 A = Neighbor(Cell, Side), B = Neighbor(Next, Side);

				if (!Corridor(A) || !Corridor(B))
					continue;

				const uint8 Old[] = {Layout.Walls[Cell], Layout.Walls[Next], Layout.Walls[A], Layout.Walls[B]};
				SetOpen(Cell, Direction, false);
				SetOpen(Cell, Side);
				SetOpen(A, Direction);
				SetOpen(Next, Side);

				if (Degree(Cell) >= MinDegree && Degree(Next) >= MinNextDegree &&
				    StraightLength(Cell, Side) <= FMazeRouteDefinition::OrdinaryStraightCells &&
				    StraightLength(Next, Side) <= FMazeRouteDefinition::OrdinaryStraightCells &&
				    StraightLength(A, Direction) <= FMazeRouteDefinition::OrdinaryStraightCells)
					return true;

				Layout.Walls[Cell] = Old[0];
				Layout.Walls[Next] = Old[1];
				Layout.Walls[A] = Old[2];
				Layout.Walls[B] = Old[3];
			}

			// Existing loops may already provide a safe alternate route.
			SetOpen(Cell, Direction, false);

			if (Degree(Cell) >= MinDegree && Degree(Next) >= MinNextDegree && Connected(Cell, Next))
				return true;

			SetOpen(Cell, Direction);

			return false;
		}

		void Shape()
		{
			for (int32 Pass = 0; Pass < FMazeRouteDefinition::ShapingPasses; ++Pass)
			{
				bool Changed = false;

				for (int32 Cell = 0; Cell < Reserved.Num(); ++Cell)
					if (Corridor(Cell))
						for (const int32 D : {1, 2})
							if (!(Layout.Walls[Cell] & (1 << D)) && Corridor(Neighbor(Cell, D)) &&
							    StraightLength(Cell, D) > FMazeRouteDefinition::OrdinaryStraightCells)
								Changed |= BreakStraight(Cell, D);

				if (!Changed)
					break;
			}
		}

		TArray<int32> Scenic()
		{
			TArray<int32> Protected;
			TArray<bool> Used;
			Used.Init(false, Reserved.Num());
			int32 Count = 0;

			// Count unavoidable long runs toward the quota too; connectivity takes priority.
			for (int32 Cell = 0; Cell < Reserved.Num(); ++Cell)
				if (Corridor(Cell))
					for (const int32 D : {1, 2})
						if (RayLength(Cell, (D + 2) % 4) == 0 &&
						    RayLength(Cell, D) + 1 > FMazeRouteDefinition::OrdinaryStraightCells)
						{
							++Count;

							for (int32 P = Cell;; P = Neighbor(P, D))
							{
								Used[P] = true;
								Protected.AddUnique(P);

								if ((Layout.Walls[P] & (1 << D)) || !Corridor(Neighbor(P, D)))
									break;
							}
						}

			const int32 Target =
			    FMath::Max(1, Layout.Size * Layout.Size / FMazeRouteDefinition::CellsPerScenicCorridor);

			for (int32 Attempt = 0; Attempt < Target * FMazeRouteDefinition::ScenicPlacementAttempts && Count < Target;
			     ++Attempt)
			{
				const int32 Cell = Random.RandRange(0, Reserved.Num() - 1), D = Random.RandRange(1, 2);
				const int32 Length =
				    Random.RandRange(FMazeRouteDefinition::ScenicMinCells, FMazeRouteDefinition::ScenicMaxCells);
				TArray<int32, TInlineAllocator<10>> Path;

				for (int32 P = Cell; Corridor(P) && Path.Num() < Length; P = Neighbor(P, D))
				{
					bool NearOther = Used[P];

					for (int32 Side = 0; Side < 4; ++Side)
					{
						const int32 N = Neighbor(P, Side);
						NearOther |= Used.IsValidIndex(N) && Used[N];
					}

					if (NearOther)
						break;

					Path.Add(P);
				}

				if (Path.Num() != Length || !(Layout.Walls[Cell] & (1 << ((D + 2) % 4))) ||
				    !(Layout.Walls[Path.Last()] & (1 << D)))
					continue;

				for (int32 I = 0; I + 1 < Path.Num(); ++I)
					SetOpen(Path[I], D);

				for (const int32 P : Path)
				{
					Used[P] = true;
					Protected.Add(P);
				}

				++Count;
			}

			return Protected;
		}
	};
}

TArray<int32> MazeRoutes::Build(FMazeLayout& Layout, const TArray<bool>& Reserved, int32 Start, FRandomStream& Random)
{
	FRouteBuilder Builder{Layout, Reserved, Random};

	Builder.Tree(Start);
	Builder.Braid();
	Builder.Shape();

	return Builder.Scenic();
}
