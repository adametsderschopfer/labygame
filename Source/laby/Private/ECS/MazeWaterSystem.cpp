#include "ECS/MazeWaterSystem.h"
#include "ECS/MazeECSFragments.h"
#include "ECS/MazeWaterDefinition.h"
#include "ECS/MazeVitalsSystem.h"
#include "Maze/MazeRoomDefinition.h"

FMazeWaterExposure FMazeWaterSystem::Evaluate(const FMazeGenerationFragment& Maze,
                                              const FVector& FeetLocation,
                                              const FVector& EyeLocation)
{
	FMazeWaterExposure Exposure;

	if (!Maze.Data || Maze.Cell <= 0.f)
		return Exposure;

	const FVector Feet = FeetLocation - Maze.Origin;
	const FVector Eye = EyeLocation - Maze.Origin;
	const FMazeLayout& Layout = Maze.Data->Layout;
	const float HalfWall = Maze.WallThickness * 0.5f;

	for (int32 Index = 0; Index < Layout.Rooms.Num(); ++Index)
	{
		const EMazeRoomType Type = Layout.RoomType(Index);

		if (Type == EMazeRoomType::Empty)
			continue;

		const FIntRect& Room = Layout.Rooms[Index];
		const float MinX = Room.Min.X * Maze.Cell + HalfWall;
		const float MaxX = Room.Max.X * Maze.Cell - HalfWall;
		const float MinY = Room.Min.Y * Maze.Cell + HalfWall;
		const float MaxY = Room.Max.Y * Maze.Cell - HalfWall;

		if (Feet.X < MinX || Feet.X >= MaxX || Feet.Y < MinY || Feet.Y >= MaxY)
			continue;

		const float WaterHeight = FMazeRoomDefinition::WaterHeight(Type);

		Exposure.bWading = Feet.Z < WaterHeight - FMazeWaterDefinition::SurfaceTolerance;
		Exposure.bSubmerged = Exposure.bWading && Eye.Z < WaterHeight - FMazeWaterDefinition::SurfaceTolerance;

		return Exposure;
	}

	return Exposure;
}

void FMazeWaterSystem::ApplySubmersion(const FMazeWaterExposure& Exposure, FMazeVitals& Vitals)
{
	if (Exposure.bSubmerged)
		FMazeVitalsSystem::Damage(Vitals, Vitals.Health);
}
