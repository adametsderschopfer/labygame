#include "World/MazeWorld.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "ECS/MazeDoorSystem.h"
#include "ECS/MazeECSSubsystem.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Math/RotationMatrix.h"
#include "Maze/MazeRoomDefinition.h"
#include "World/MazeDoorSettings.h"
#include "World/MazeLocationSubsystem.h"

namespace
{
	FQuat DoorRotation(const FMazeDoorView& Door)
	{
		return FRotationMatrix::MakeFromXY(Door.SlideAxis, Door.Normal).ToQuat();
	}

	float DoorTravelAlpha(const FMazeDoorView& Door)
	{
		return FMath::SmoothStep(0.f, 1.f, Door.OpenAmount);
	}
}

FTransform AMazeWorld::DoorLeafTransform(const FMazeDoorView& Door, float Side) const
{
	const FVector Location = Door.Center - GetActorLocation() +
	                         Door.SlideAxis * (Side * DoorTravelAlpha(Door) * FMazeDoorDefinition::LeafTravelCm);

	return FTransform(DoorRotation(Door), Location);
}

FTransform AMazeWorld::DoorCollisionTransform(const FMazeDoorView& Door, float Side) const
{
	const auto Maze = ECSSubsystem->ReadMaze(MazeEntity);
	const float Width = FMazeRoomDefinition::OpeningWidth(Maze.Cell - Maze.WallThickness);
	const float Height = FMazeRoomDefinition::OpeningHeight(Maze.WallHeight);
	const float Depth = FMath::Min(Maze.WallThickness, 32.f);
	const FVector Location =
	    Door.Center - GetActorLocation() + FVector::UpVector * (Height * 0.5f) +
	    Door.SlideAxis * (Side * (Width * 0.25f + DoorTravelAlpha(Door) * FMazeDoorDefinition::LeafTravelCm));

	return FTransform(DoorRotation(Door), Location, FVector(Width * 0.005f, Depth * 0.01f, Height * 0.01f));
}

void AMazeWorld::RebuildDoorInstances()
{
	DoorFrames->ClearInstances();
	DoorLeavesLeft->ClearInstances();
	DoorLeavesRight->ClearInstances();
	DoorCollisionLeft->ClearInstances();
	DoorCollisionRight->ClearInstances();
	DoorStatusLights->ClearInstances();

	if (!ECSSubsystem)
		return;

	const TArray<FMazeDoorView> Doors = ECSSubsystem->ReadDoors(MazeEntity);
	const auto Maze = ECSSubsystem->ReadMaze(MazeEntity);

	for (const FMazeDoorView& Door : Doors)
	{
		const FVector BaseLocation = Door.Center - GetActorLocation();
		const FQuat Rotation = DoorRotation(Door);
		const FTransform Base(Rotation, BaseLocation);

		DoorFrames->AddInstance(Base);
		DoorLeavesLeft->AddInstance(DoorLeafTransform(Door, -1.f));
		DoorLeavesRight->AddInstance(DoorLeafTransform(Door, 1.f));
		DoorCollisionLeft->AddInstance(DoorCollisionTransform(Door, -1.f));
		DoorCollisionRight->AddInstance(DoorCollisionTransform(Door, 1.f));

		// Thin emissive overlays make the top beacon and room-side access reader
		// legible in the deliberately dark interior without adding per-door lights.
		const FVector Front = -Door.Normal * (Maze.WallThickness * 0.5f + 3.f);

		DoorStatusLights->AddInstance(
		    FTransform(Rotation, BaseLocation + Front + FVector::UpVector * 235.f, FVector(0.24f, 0.025f, 0.05f)));
		DoorStatusLights->AddInstance(
		    FTransform(Rotation,
		               BaseLocation + Front + Door.SlideAxis * 72.f + FVector::UpVector * 145.f,
		               FVector(0.16f, 0.025f, 0.04f)));
	}
}

void AMazeWorld::PrepareDoorAssets()
{
	if (GetNetMode() == NM_DedicatedServer)
		return;

	const auto* Settings = GetDefault<UMazeDoorSettings>();
	auto* FrameMesh = Settings->FrameMesh.Get();
	auto* LeftLeafMesh = Settings->LeftLeafMesh.Get();
	auto* RightLeafMesh = Settings->RightLeafMesh.Get();
	auto* SurfaceMaterial = Settings->SurfaceMaterial.Get();
	auto* StatusMaterial = Settings->StatusMaterial.Get();

	if (!FrameMesh || !LeftLeafMesh || !RightLeafMesh || !SurfaceMaterial || !StatusMaterial)
	{
		GetWorld()->GetSubsystem<UMazeLocationSubsystem>()->Fail(
		    TEXT("Secure door presentation assets are unavailable"));

		return;
	}

	DoorFrames->SetStaticMesh(FrameMesh);
	DoorLeavesLeft->SetStaticMesh(LeftLeafMesh);
	DoorLeavesRight->SetStaticMesh(RightLeafMesh);
	DoorFrames->SetMaterial(0, SurfaceMaterial);
	DoorLeavesLeft->SetMaterial(0, SurfaceMaterial);
	DoorLeavesRight->SetMaterial(0, SurfaceMaterial);
	DoorStatusLights->SetMaterial(0, StatusMaterial);
}

void AMazeWorld::UpdateDoors()
{
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

	if (DoorCollisionLeft->GetInstanceCount() != Doors.Num())
	{
		RebuildDoorInstances();

		return;
	}

	for (int32 Index = 0; Index < Doors.Num(); ++Index)
	{
		const bool bMarkDirty = Index == Doors.Num() - 1;

		DoorLeavesLeft->UpdateInstanceTransform(Index, DoorLeafTransform(Doors[Index], -1.f), false, bMarkDirty, true);
		DoorLeavesRight->UpdateInstanceTransform(Index, DoorLeafTransform(Doors[Index], 1.f), false, bMarkDirty, true);
		DoorCollisionLeft->UpdateInstanceTransform(
		    Index, DoorCollisionTransform(Doors[Index], -1.f), false, bMarkDirty, true);
		DoorCollisionRight->UpdateInstanceTransform(
		    Index, DoorCollisionTransform(Doors[Index], 1.f), false, bMarkDirty, true);
	}
}
