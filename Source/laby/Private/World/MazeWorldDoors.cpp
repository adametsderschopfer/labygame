#include "World/MazeWorld.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "ECS/MazeDoorSystem.h"
#include "ECS/MazeECSSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "Math/RotationMatrix.h"
#include "Maze/MazeRoomDefinition.h"

namespace
{
	FQuat DoorRotation(const FMazeDoorView& Door)
	{
		return FRotationMatrix::MakeFromXZ(Door.SlideAxis, FVector::UpVector).ToQuat();
	}

	FTransform LeafTransform(
	    const FMazeDoorView& Door, const FVector& ActorLocation, float Width, float Height, float WallThickness)
	{
		const float LeafWidth = Width - 2.f * FMazeDoorDefinition::FrameWidthCm;
		const float LeafHeight = Height - FMazeDoorDefinition::FrameWidthCm;
		const FVector Hinge = Door.Center - Door.SlideAxis * (Width * .5f - FMazeDoorDefinition::FrameWidthCm);
		const FQuat Swing(
		    FVector::UpVector,
		    FMath::DegreesToRadians(FMazeDoorDefinition::SwingDegrees * FMath::SmoothStep(0.f, 1.f, Door.OpenAmount)));
		const FVector Axis = Swing.RotateVector(Door.SlideAxis);
		const FVector Center = Hinge + Axis * (LeafWidth * .5f) + FVector::UpVector * (LeafHeight * .5f);
		const FQuat Rotation = FRotationMatrix::MakeFromXZ(Axis, FVector::UpVector).ToQuat();

		return FTransform(Rotation, Center - ActorLocation, FVector(LeafWidth, WallThickness * .5f, LeafHeight) * .01f);
	}

	FTransform HandleTransform(const FMazeDoorView& Door,
	                           const FVector& ActorLocation,
	                           float Width,
	                           float WallThickness)
	{
		const FVector Center = FMazeDoorSystem::HandleLocation(Door, Width);
		const FQuat Swing(
		    FVector::UpVector,
		    FMath::DegreesToRadians(FMazeDoorDefinition::SwingDegrees * FMath::SmoothStep(0.f, 1.f, Door.OpenAmount)));
		const FVector Axis = Swing.RotateVector(Door.SlideAxis);

		return FTransform(FRotationMatrix::MakeFromXZ(Axis, FVector::UpVector).ToQuat(),
		                  Center - ActorLocation,
		                  FVector(12.f, WallThickness * .5f + 5.f, 5.f) * .01f);
	}
}

bool AMazeWorld::IsDoorCollisionComponent(const UPrimitiveComponent* Component, int32 Instance, int32 DoorIndex) const
{
	return Component == DoorCollision && Instance == DoorIndex;
}

void AMazeWorld::RebuildDoorInstances()
{
	DoorFrames->ClearInstances();
	DoorLeaves->ClearInstances();
	DoorHandles->ClearInstances();
	DoorCollision->ClearInstances();
	AppliedDoorOpenAmounts.Reset();

	if (!ECSSubsystem)
		return;

	const TArray<FMazeDoorView> Doors = ECSSubsystem->ReadDoors(MazeEntity);
	const auto Maze = ECSSubsystem->ReadMaze(MazeEntity);
	const float Width = FMazeRoomDefinition::OpeningWidth(Maze.Cell - Maze.WallThickness);
	const float Height = FMazeRoomDefinition::OpeningHeight(Maze.WallHeight);
	const float Frame = FMazeDoorDefinition::FrameWidthCm;
	const bool bVisual = GetNetMode() != NM_DedicatedServer;

	for (const FMazeDoorView& Door : Doors)
	{
		const FVector Base = Door.Center - GetActorLocation();
		const FQuat Rotation = DoorRotation(Door);
		const FTransform Leaf = LeafTransform(Door, GetActorLocation(), Width, Height, Maze.WallThickness);

		if (bVisual)
		{
			for (const float Side : {-1.f, 1.f})
				DoorFrames->AddInstance(FTransform(Rotation,
				                                   Base + Door.SlideAxis * (Side * (Width - Frame) * .5f) +
				                                       FVector::UpVector * (Height * .5f),
				                                   FVector(Frame, Maze.WallThickness, Height) * .01f));

			DoorFrames->AddInstance(FTransform(Rotation,
			                                   Base + FVector::UpVector * (Height - Frame * .5f),
			                                   FVector(Width, Maze.WallThickness, Frame) * .01f));
			DoorLeaves->AddInstance(Leaf);
			DoorHandles->AddInstance(HandleTransform(Door, GetActorLocation(), Width, Maze.WallThickness));
		}

		DoorCollision->AddInstance(Leaf);
		AppliedDoorOpenAmounts.Add(Door.OpenAmount);
	}
}

void AMazeWorld::PrepareDoorAssets()
{
	if (GetNetMode() == NM_DedicatedServer)
		return;

	if (VisualMaterials.IsValidIndex(3))
	{
		DoorFrames->SetMaterial(0, VisualMaterials[3]);
		DoorLeaves->SetMaterial(0, VisualMaterials[3]);
	}

	if (VisualMaterials.IsValidIndex(2))
		DoorHandles->SetMaterial(0, VisualMaterials[2]);
}

void AMazeWorld::UpdateDoors()
{
	if (!ECSSubsystem)
		return;

	if (HasAuthority())
	{
		TArray<uint8> Targets = ECSSubsystem->ReadDoorTargets(MazeEntity);

		if (Targets != ReplicatedDoorTargets)
		{
			ReplicatedDoorTargets = MoveTemp(Targets);
			ForceNetUpdate();
		}
	}

	const TArray<FMazeDoorView> Doors = ECSSubsystem->ReadDoors(MazeEntity);

	if (DoorCollision->GetInstanceCount() != Doors.Num() || AppliedDoorOpenAmounts.Num() != Doors.Num())
	{
		RebuildDoorInstances();

		return;
	}

	const auto Maze = ECSSubsystem->ReadMaze(MazeEntity);
	const float Width = FMazeRoomDefinition::OpeningWidth(Maze.Cell - Maze.WallThickness);
	const float Height = FMazeRoomDefinition::OpeningHeight(Maze.WallHeight);
	const bool bVisual = GetNetMode() != NM_DedicatedServer;

	for (int32 Index = 0; Index < Doors.Num(); ++Index)
	{
		if (FMath::IsNearlyEqual(AppliedDoorOpenAmounts[Index], Doors[Index].OpenAmount))
			continue;

		const FTransform Leaf = LeafTransform(Doors[Index], GetActorLocation(), Width, Height, Maze.WallThickness);

		DoorCollision->UpdateInstanceTransform(Index, Leaf, false, true, true);

		if (bVisual)
		{
			DoorLeaves->UpdateInstanceTransform(Index, Leaf, false, true, true);
			DoorHandles->UpdateInstanceTransform(
			    Index, HandleTransform(Doors[Index], GetActorLocation(), Width, Maze.WallThickness), false, true, true);
		}

		AppliedDoorOpenAmounts[Index] = Doors[Index].OpenAmount;
	}
}
