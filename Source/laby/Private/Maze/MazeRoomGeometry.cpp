#include "Maze/MazeRoomGeometry.h"

namespace
{
	FTransform BoxTransform(float MinX, float MaxX, float MinY, float MaxY, float Bottom, float Top)
	{
		return FTransform(FRotator::ZeroRotator,
		                  FVector((MinX + MaxX) * 0.5f, (MinY + MaxY) * 0.5f, (Bottom + Top) * 0.5f),
		                  FVector((MaxX - MinX) / 100.f, (MaxY - MinY) / 100.f, (Top - Bottom) / 100.f));
	}
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
		if (MaxX - MinX > UE_KINDA_SMALL_NUMBER && MaxY - MinY > UE_KINDA_SMALL_NUMBER &&
		    Top - Bottom > UE_KINDA_SMALL_NUMBER)
			Result.Boxes.Add({BoxTransform(MinX, MaxX, MinY, MaxY, Bottom, Top), Surface, bCollision});
	};

	for (int32 RoomIndex = 1; RoomIndex < Layout.Rooms.Num(); ++RoomIndex)
	{
		const EMazeRoomType Type = Layout.RoomType(RoomIndex);

		if (Type == EMazeRoomType::Empty)
			continue;

		const FIntRect& Room = Layout.Rooms[RoomIndex];
		const float MinX = Room.Min.X * Cell;
		const float MaxX = Room.Max.X * Cell;
		const float MinY = Room.Min.Y * Cell;
		const float MaxY = Room.Max.Y * Cell;
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
		AddBox(MinX + HalfWall,
		       MaxX - HalfWall,
		       MinY + HalfWall,
		       MaxY - HalfWall,
		       FMazeRoomDefinition::WaterHeight(Type) - FMazeRoomDefinition::WaterThickness,
		       FMazeRoomDefinition::WaterHeight(Type),
		       EMazeRoomSurface::Water,
		       false);

		TArray<FMazeRoomDoorway> RoomDoors = Doorways.FilterByPredicate(
		    [RoomIndex](const FMazeRoomDoorway& Doorway)
		    {
			    return Doorway.RoomIndex == RoomIndex;
		    });
		const float OpeningWidth = FMazeRoomDefinition::OpeningWidth(Cell - WallThickness);

		const auto AddHorizontalWall = [&](float CenterY,
		                                   float SegmentMinX,
		                                   float SegmentMaxX,
		                                   float Bottom,
		                                   float Top,
		                                   EMazeRoomSurface Surface,
		                                   const FMazeRoomDoorway* Door)
		{
			if (!Door)
			{
				AddBox(SegmentMinX, SegmentMaxX, CenterY - HalfWall, CenterY + HalfWall, Bottom, Top, Surface);

				return;
			}

			const float CenterX = (Door->Cell.X + 0.5f) * Cell;
			AddBox(SegmentMinX,
			       CenterX - OpeningWidth * 0.5f,
			       CenterY - HalfWall,
			       CenterY + HalfWall,
			       Bottom,
			       Top,
			       Surface);
			AddBox(CenterX + OpeningWidth * 0.5f,
			       SegmentMaxX,
			       CenterY - HalfWall,
			       CenterY + HalfWall,
			       Bottom,
			       Top,
			       Surface);
		};
		const auto AddVerticalWall = [&](float CenterX,
		                                 float SegmentMinY,
		                                 float SegmentMaxY,
		                                 float Bottom,
		                                 float Top,
		                                 EMazeRoomSurface Surface,
		                                 const FMazeRoomDoorway* Door)
		{
			if (!Door)
			{
				AddBox(CenterX - HalfWall, CenterX + HalfWall, SegmentMinY, SegmentMaxY, Bottom, Top, Surface);

				return;
			}

			const float CenterY = (Door->Cell.Y + 0.5f) * Cell;
			AddBox(CenterX - HalfWall,
			       CenterX + HalfWall,
			       SegmentMinY,
			       CenterY - OpeningWidth * 0.5f,
			       Bottom,
			       Top,
			       Surface);
			AddBox(CenterX - HalfWall,
			       CenterX + HalfWall,
			       CenterY + OpeningWidth * 0.5f,
			       SegmentMaxY,
			       Bottom,
			       Top,
			       Surface);
		};

		for (int32 Y = Room.Min.Y; Y < Room.Max.Y; ++Y)
			for (const int32 Direction : {1, 3})
			{
				const int32 X = Direction == 1 ? Room.Max.X - 1 : Room.Min.X;
				const float BoundaryX = Direction == 1 ? MaxX : MinX;
				const FMazeRoomDoorway* Door = RoomDoors.FindByPredicate(
				    [X, Y, Direction](const FMazeRoomDoorway& Candidate)
				    {
					    return Candidate.Cell == FIntPoint(X, Y) && Candidate.Direction == Direction;
				    });
				const EMazeRoomSurface LowerSurface =
				    Type == EMazeRoomType::Pool ? EMazeRoomSurface::PoolTile : EMazeRoomSurface::Ceramic;

				AddVerticalWall(BoundaryX, Y * Cell, (Y + 1) * Cell, FloorHeight, 0.f, LowerSurface, Door);
				AddVerticalWall(BoundaryX,
				                Y * Cell,
				                (Y + 1) * Cell,
				                DefaultWallHeight,
				                CeilingHeight,
				                EMazeRoomSurface::Ceramic,
				                nullptr);
			}

		for (int32 X = Room.Min.X; X < Room.Max.X; ++X)
			for (const int32 Direction : {0, 2})
			{
				const int32 Y = Direction == 2 ? Room.Max.Y - 1 : Room.Min.Y;
				const float BoundaryY = Direction == 2 ? MaxY : MinY;
				const FMazeRoomDoorway* Door = RoomDoors.FindByPredicate(
				    [X, Y, Direction](const FMazeRoomDoorway& Candidate)
				    {
					    return Candidate.Cell == FIntPoint(X, Y) && Candidate.Direction == Direction;
				    });
				const EMazeRoomSurface LowerSurface =
				    Type == EMazeRoomType::Pool ? EMazeRoomSurface::PoolTile : EMazeRoomSurface::Ceramic;

				AddHorizontalWall(BoundaryY, X * Cell, (X + 1) * Cell, FloorHeight, 0.f, LowerSurface, Door);
				AddHorizontalWall(BoundaryY,
				                  X * Cell,
				                  (X + 1) * Cell,
				                  DefaultWallHeight,
				                  CeilingHeight,
				                  EMazeRoomSurface::Ceramic,
				                  nullptr);
			}

		if (Type == EMazeRoomType::ShallowFlooded)
		{
			const int32 StepCount =
			    FMath::RoundToInt(FMazeRoomDefinition::ShallowDepth / FMazeRoomDefinition::StairRise);
			const float StepWidth = OpeningWidth - 10.f;

			for (const FMazeRoomDoorway& Door : RoomDoors)
			{
				const FVector2D Outward[] = {
				    FVector2D(0.f, -1.f), FVector2D(1.f, 0.f), FVector2D(0.f, 1.f), FVector2D(-1.f, 0.f)};
				const FVector2D Inward = -Outward[Door.Direction];
				const FVector2D Boundary((Door.Cell.X + 0.5f) * Cell, (Door.Cell.Y + 0.5f) * Cell);

				for (int32 Step = 0; Step < StepCount; ++Step)
				{
					const float Begin = Step * FMazeRoomDefinition::StairTread;
					const float End = (Step + 1) * FMazeRoomDefinition::StairTread;
					const FVector2D A = Boundary + Inward * Begin;
					const FVector2D B = Boundary + Inward * End;
					const float Top = -(Step + 1) * FMazeRoomDefinition::StairRise;

					if (Door.Direction == 0 || Door.Direction == 2)
						AddBox(A.X - StepWidth * 0.5f,
						       A.X + StepWidth * 0.5f,
						       FMath::Min(A.Y, B.Y),
						       FMath::Max(A.Y, B.Y),
						       FloorHeight - 5.f,
						       Top,
						       EMazeRoomSurface::ShallowFloor);
					else
						AddBox(FMath::Min(A.X, B.X),
						       FMath::Max(A.X, B.X),
						       A.Y - StepWidth * 0.5f,
						       A.Y + StepWidth * 0.5f,
						       FloorHeight - 5.f,
						       Top,
						       EMazeRoomSurface::ShallowFloor);
				}
			}
		}
		else if (Type == EMazeRoomType::Pool)
		{
			const FVector2D RoomCenter((MinX + MaxX) * 0.5f, (MinY + MaxY) * 0.5f);

			for (const FMazeRoomDoorway& Door : RoomDoors)
			{
				const FVector2D Boundary((Door.Cell.X + 0.5f) * Cell, (Door.Cell.Y + 0.5f) * Cell);
				const float HalfBridge = FMazeRoomDefinition::BridgeWidth * 0.5f;

				if (Door.Direction == 0 || Door.Direction == 2)
					AddBox(Boundary.X - HalfBridge,
					       Boundary.X + HalfBridge,
					       FMath::Min(Boundary.Y, RoomCenter.Y),
					       FMath::Max(Boundary.Y, RoomCenter.Y),
					       -FMazeRoomDefinition::BridgeThickness,
					       0.f,
					       EMazeRoomSurface::Metal);
				else
					AddBox(FMath::Min(Boundary.X, RoomCenter.X),
					       FMath::Max(Boundary.X, RoomCenter.X),
					       Boundary.Y - HalfBridge,
					       Boundary.Y + HalfBridge,
					       -FMazeRoomDefinition::BridgeThickness,
					       0.f,
					       EMazeRoomSurface::Metal);
			}
		}
	}

	return Result;
}
