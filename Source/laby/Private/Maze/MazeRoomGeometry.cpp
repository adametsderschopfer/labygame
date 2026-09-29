#include "Maze/MazeRoomGeometry.h"

namespace
{
	FTransform BoxTransform(float MinX, float MaxX, float MinY, float MaxY, float Bottom, float Top)
	{
		return FTransform(FRotator::ZeroRotator,
		                  FVector((MinX + MaxX) * 0.5f, (MinY + MaxY) * 0.5f, (Bottom + Top) * 0.5f),
		                  FVector((MaxX - MinX) / 100.f, (MaxY - MinY) / 100.f, (Top - Bottom) / 100.f));
	}

	const FVector2D Outward[] = {FVector2D(0, -1), FVector2D(1, 0), FVector2D(0, 1), FVector2D(-1, 0)};
}

FMazeRoomGeometry FMazeRoomGeometry::Build(const FMazeLayout& Layout,
                                           float Cell,
                                           float WallThickness,
                                           float DefaultWallHeight)
{
	FMazeRoomGeometry Result;
	const TArray<FMazeRoomDoorway> Doorways = Layout.RoomDoorways();
	const float HalfWall = WallThickness * 0.5f;
	constexpr float SlabThickness = 50.f;
	const auto AddBox = [&Result](float MinX,
	                              float MaxX,
	                              float MinY,
	                              float MaxY,
	                              float Bottom,
	                              float Top,
	                              EMazeRoomSurface Surface,
	                              bool bCollision = true)
	{
		if (MaxX > MinX && MaxY > MinY && Top > Bottom)
			Result.Boxes.Add({BoxTransform(MinX, MaxX, MinY, MaxY, Bottom, Top), Surface, bCollision});
	};

	for (int32 RoomIndex = 0; RoomIndex < Layout.Rooms.Num(); ++RoomIndex)
	{
		const EMazeRoomType Type = Layout.RoomType(RoomIndex);

		const FIntRect& Room = Layout.Rooms[RoomIndex];
		const float MinX = Room.Min.X * Cell, MaxX = Room.Max.X * Cell;
		const float MinY = Room.Min.Y * Cell, MaxY = Room.Max.Y * Cell;

		if (Type == EMazeRoomType::Empty)
			continue;

		const float FloorHeight = FMazeRoomDefinition::FloorHeight(Type);
		const float CeilingHeight = FMazeRoomDefinition::CeilingHeight(Type, DefaultWallHeight);
		const EMazeRoomSurface FloorSurface =
		    Type == EMazeRoomType::Pool ? EMazeRoomSurface::PoolTile : EMazeRoomSurface::ShallowFloor;

		AddBox(MinX + HalfWall,
		       MaxX - HalfWall,
		       MinY + HalfWall,
		       MaxY - HalfWall,
		       FloorHeight - SlabThickness,
		       FloorHeight,
		       FloorSurface);
		Result.Ceilings.Add(BoxTransform(MinX - HalfWall,
		                                 MaxX + HalfWall,
		                                 MinY - HalfWall,
		                                 MaxY + HalfWall,
		                                 CeilingHeight,
		                                 CeilingHeight + SlabThickness));
		// Bounds only: the visual worker emits one upward-facing water interface.
		AddBox(MinX + HalfWall,
		       MaxX - HalfWall,
		       MinY + HalfWall,
		       MaxY - HalfWall,
		       FMazeRoomDefinition::WaterHeight(Type) - FMazeRoomDefinition::WaterThickness,
		       FMazeRoomDefinition::WaterHeight(Type),
		       EMazeRoomSurface::Water,
		       false);

		const auto AddWallRing = [&](float Bottom, float Top, EMazeRoomSurface Surface)
		{
			// Foundations are solid UNDER doors; openings belong above corridor floor.
			AddBox(MinX - HalfWall, MaxX + HalfWall, MinY - HalfWall, MinY + HalfWall, Bottom, Top, Surface);
			AddBox(MinX - HalfWall, MaxX + HalfWall, MaxY - HalfWall, MaxY + HalfWall, Bottom, Top, Surface);
			AddBox(MinX - HalfWall, MinX + HalfWall, MinY + HalfWall, MaxY - HalfWall, Bottom, Top, Surface);
			AddBox(MaxX - HalfWall, MaxX + HalfWall, MinY + HalfWall, MaxY - HalfWall, Bottom, Top, Surface);
		};

		AddWallRing(
		    FloorHeight, 0.f, Type == EMazeRoomType::Pool ? EMazeRoomSurface::PoolTile : EMazeRoomSurface::Ceramic);
		AddWallRing(DefaultWallHeight, CeilingHeight, EMazeRoomSurface::Ceramic);

		const auto RoomDoors = Doorways.FilterByPredicate(
		    [RoomIndex](const FMazeRoomDoorway& Door)
		    {
			    return Door.RoomIndex == RoomIndex;
		    });
		const float OpeningWidth = FMazeRoomDefinition::OpeningWidth(Cell - WallThickness);
		const auto Portal = [Cell](const FMazeRoomDoorway& Door)
		{
			return FVector2D((Door.Cell.X + 0.5f) * Cell, (Door.Cell.Y + 0.5f) * Cell) +
			       Outward[Door.Direction] * (Cell * 0.5f);
		};

		if (Type == EMazeRoomType::ShallowFlooded)
		{
			const int32 StepCount =
			    FMath::RoundToInt(FMazeRoomDefinition::ShallowDepth / FMazeRoomDefinition::StairRise);

			for (const FMazeRoomDoorway& Door : RoomDoors)
			{
				const FVector2D Boundary = Portal(Door);
				const FVector2D Inward = -Outward[Door.Direction];

				// Foundation provides the level threshold. Two treads plus floor make three descents.
				for (int32 Step = 1; Step < StepCount; ++Step)
				{
					const FVector2D A = Boundary + Inward * (HalfWall + (Step - 1) * FMazeRoomDefinition::StairTread);
					const FVector2D B = Boundary + Inward * (HalfWall + Step * FMazeRoomDefinition::StairTread);
					const float HalfWidth = OpeningWidth * 0.5f;
					const FVector2D Across =
					    Door.Direction % 2 == 0 ? FVector2D(HalfWidth, 0) : FVector2D(0, HalfWidth);

					AddBox(FMath::Min(A.X, B.X) - Across.X,
					       FMath::Max(A.X, B.X) + Across.X,
					       FMath::Min(A.Y, B.Y) - Across.Y,
					       FMath::Max(A.Y, B.Y) + Across.Y,
					       FloorHeight,
					       -Step * FMazeRoomDefinition::StairRise,
					       FloorSurface);
				}
			}
		}
		else
		{
			const FVector2D Center((MinX + MaxX) * 0.5f, (MinY + MaxY) * 0.5f);
			const float HalfBridge = FMazeRoomDefinition::BridgeWidth * 0.5f;
			TArray<FBox2D> Decks;
			const auto AddDeck = [&Decks, HalfBridge](FVector2D A, FVector2D B)
			{
				Decks.Emplace(FVector2D(FMath::Min(A.X, B.X) - HalfBridge, FMath::Min(A.Y, B.Y) - HalfBridge),
				              FVector2D(FMath::Max(A.X, B.X) + HalfBridge, FMath::Max(A.Y, B.Y) + HalfBridge));
			};

			for (const FMazeRoomDoorway& Door : RoomDoors)
			{
				const FVector2D Inside = Portal(Door) - Outward[Door.Direction] * HalfWall;
				const FVector2D Elbow =
				    Door.Direction % 2 == 0 ? FVector2D(Inside.X, Center.Y) : FVector2D(Center.X, Inside.Y);

				AddDeck(Inside, Elbow);
				AddDeck(Elbow, Center);
			}

			// Union deck rectangles into disjoint slabs: connected with offset doors,
			// without overlapping top faces at elbows or the central junction.
			TArray<double> Cuts;

			for (const FBox2D& Deck : Decks)
			{
				Cuts.AddUnique(Deck.Min.X);
				Cuts.AddUnique(Deck.Max.X);
			}

			Cuts.Sort();

			for (int32 Index = 1; Index < Cuts.Num(); ++Index)
			{
				const double X0 = Cuts[Index - 1], X1 = Cuts[Index];
				TArray<FVector2D> Spans;

				for (const FBox2D& Deck : Decks)
					if (Deck.Min.X < X1 && Deck.Max.X > X0)
						Spans.Emplace(Deck.Min.Y, Deck.Max.Y);

				Spans.Sort(
				    [](const FVector2D& A, const FVector2D& B)
				    {
					    return A.X < B.X;
				    });

				for (int32 Span = 0; Span < Spans.Num(); ++Span)
				{
					double End = Spans[Span].Y;
					const double Begin = Spans[Span].X;

					while (Span + 1 < Spans.Num() && Spans[Span + 1].X <= End)
						End = FMath::Max(End, Spans[++Span].Y);

					AddBox(FMath::Max(X0, double(MinX + HalfWall)),
					       FMath::Min(X1, double(MaxX - HalfWall)),
					       FMath::Max(Begin, double(MinY + HalfWall)),
					       FMath::Min(End, double(MaxY - HalfWall)),
					       -FMazeRoomDefinition::BridgeThickness,
					       0.f,
					       EMazeRoomSurface::Metal);
				}
			}
		}
	}

	return Result;
}
