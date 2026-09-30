#include "World/MazeWorld.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "ECS/MazeDoorSystem.h"
#include "ECS/MazeECSSubsystem.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/OverlapResult.h"
#include "Materials/MaterialInterface.h"
#include "Math/RotationMatrix.h"
#include "Maze/MazeRoomDefinition.h"
#include "World/MazeLocationSettings.h"
#include "World/MazeLocationSubsystem.h"

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
		const FQuat Swing(FVector::UpVector,
		                  FMath::DegreesToRadians(Door.SwingSign * FMazeDoorDefinition::SwingDegrees *
		                                          FMath::SmoothStep(0.f, 1.f, Door.OpenAmount)));
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
		const FQuat Swing(FVector::UpVector,
		                  FMath::DegreesToRadians(Door.SwingSign * FMazeDoorDefinition::SwingDegrees *
		                                          FMath::SmoothStep(0.f, 1.f, Door.OpenAmount)));
		const FVector Axis = Swing.RotateVector(Door.SlideAxis);
		const float HandlePull =
		    Door.bWantsOpen ? FMath::Clamp(1.f - FMath::Abs(Door.OpenAmount - .12f) / .12f, 0.f, 1.f) : 0.f;
		const FQuat LeverTurn(FVector::RightVector, FMath::DegreesToRadians(-22.f * HandlePull));

		return FTransform(FRotationMatrix::MakeFromXZ(Axis, FVector::UpVector).ToQuat() * LeverTurn,
		                  Center - ActorLocation,
		                  FVector(12.f, WallThickness * .5f + 5.f, 5.f) * .01f);
	}
}

bool AMazeWorld::IsDoorCollisionComponent(const UPrimitiveComponent* Component, int32 Instance, int32 DoorIndex) const
{
	return (Component == DoorCollision && Instance == DoorIndex) ||
	       (Component == DoorReaders && Instance >= 0 && ReaderDoorIndices.IsValidIndex(Instance / 2) &&
	        ReaderDoorIndices[Instance / 2] == DoorIndex);
}

void AMazeWorld::RebuildDoorInstances()
{
	DoorFrames->ClearInstances();
	DoorLeaves->ClearInstances();
	DoorHandles->ClearInstances();
	DoorCollision->ClearInstances();
	AppliedDoorOpenAmounts.Reset();
	AppliedDoorSwingSigns.Reset();
	DoorReaders->ClearInstances();
	DoorScreens->ClearInstances();
	ReaderDoorIndices.Reset();
	AppliedDoorUnlocks.Reset();

	if (!ECSSubsystem)
		return;

	const TArray<FMazeDoorView> Doors = ECSSubsystem->ReadDoors(MazeEntity);
	const auto Maze = ECSSubsystem->ReadMaze(MazeEntity);
	const float Width = FMazeRoomDefinition::OpeningWidth(Maze.Cell - Maze.WallThickness);
	const float Height = FMazeRoomDefinition::OpeningHeight(Maze.WallHeight);
	const bool bVisual = GetNetMode() != NM_DedicatedServer;

	for (const FMazeDoorView& Door : Doors)
	{
		const FVector Base = Door.Center - GetActorLocation();
		const FQuat Rotation = DoorRotation(Door);
		const FTransform Leaf = LeafTransform(Door, GetActorLocation(), Width, Height, Maze.WallThickness);

		if (bVisual)
		{
			DoorFrames->AddInstance(FTransform(Rotation,
			                                   Base + FVector::UpVector * (Height * .5f),
			                                   FVector(Width, Maze.WallThickness, Height) * .01f));
			DoorLeaves->AddInstance(Leaf);
			DoorHandles->AddInstance(HandleTransform(Door, GetActorLocation(), Width, Maze.WallThickness));
		}

		if (Door.bRequiresCard && bVisual)
		{
			ReaderDoorIndices.Add(Door.Index);

			for (float Side : {-1.f, 1.f})
			{
				const FVector Reader =
				    FMazeDoorSystem::ReaderLocation(Door, Width, Maze.WallThickness, Side) - GetActorLocation();

				DoorReaders->AddInstance(FTransform(Rotation, Reader, FVector(.18f, .06f, .28f)));

				const int32 Screen = DoorScreens->AddInstance(FTransform(
				    Rotation, Reader + Door.Normal * Side * 3.2f + FVector(0.f, 0.f, 2.f), FVector(.13f, .005f, .16f)));

				DoorScreens->SetCustomDataValue(Screen, 0, Door.bUnlocked ? 1.f : 0.f, false);
			}
		}

		AppliedDoorUnlocks.Add(Door.bUnlocked);
		DoorCollision->AddInstance(Leaf);
		AppliedDoorOpenAmounts.Add(Door.OpenAmount);
		AppliedDoorSwingSigns.Add(Door.SwingSign);
	}
}

void AMazeWorld::PrepareDoorAssets()
{
	if (GetNetMode() == NM_DedicatedServer)
		return;

	const auto* Settings = GetDefault<UMazeLocationSettings>();
	UStaticMesh* FrameMesh = Settings->DoorFrameMesh().Get();
	UStaticMesh* LeafMesh = Settings->DoorLeafMesh().Get();
	UStaticMesh* HandleMesh = Settings->DoorHandleMesh().Get();

	if (!FrameMesh || !LeafMesh || !HandleMesh || !Settings->DoorReaderMaterial().Get())
	{
		GetWorld()->GetSubsystem<UMazeLocationSubsystem>()->Fail(TEXT("Frosted door meshes are unavailable"));

		return;
	}

	DoorFrames->SetStaticMesh(FrameMesh);
	DoorLeaves->SetStaticMesh(LeafMesh);
	DoorHandles->SetStaticMesh(HandleMesh);
}

void AMazeWorld::UpdateDoors(float DeltaSeconds)
{
	if (!ECSSubsystem)
		return;

	const auto Maze = ECSSubsystem->ReadMaze(MazeEntity);
	const float Width = FMazeRoomDefinition::OpeningWidth(Maze.Cell - Maze.WallThickness);
	const float Height = FMazeRoomDefinition::OpeningHeight(Maze.WallHeight);

	if (HasAuthority() && !GetWorld()->IsPaused())
	{
		const TArray<FMazeDoorView> CurrentDoors = ECSSubsystem->ReadDoors(MazeEntity);
		const FCollisionObjectQueryParams Pawns(ECC_Pawn);
		FCollisionQueryParams Query(SCENE_QUERY_STAT(MazeDoorPawnOverlap), false);

		Query.AddIgnoredActor(this);

		for (const FMazeDoorView& Door : CurrentDoors)
		{
			const float Desired = FMazeDoorSystem::NextOpenAmount(Door, DeltaSeconds);

			if (FMath::IsNearlyEqual(Door.OpenAmount, Desired))
				continue;

			const float CurrentAngle = FMazeDoorDefinition::SwingDegrees * FMath::SmoothStep(0.f, 1.f, Door.OpenAmount);
			const float DesiredAngle = FMazeDoorDefinition::SwingDegrees * FMath::SmoothStep(0.f, 1.f, Desired);
			const int32 StepCount = FMath::Max(1, FMath::CeilToInt(FMath::Abs(DesiredAngle - CurrentAngle) / 5.f));
			float MaxSafeAmount = Door.OpenAmount;

			for (int32 Step = 1; Step <= StepCount; ++Step)
			{
				FMazeDoorView Candidate = Door;

				Candidate.OpenAmount = FMath::Lerp(Door.OpenAmount, Desired, float(Step) / StepCount);

				const FTransform Leaf = LeafTransform(Candidate, GetActorLocation(), Width, Height, Maze.WallThickness);
				const FVector HalfExtent = Leaf.GetScale3D().GetAbs() * 50.f;
				TArray<FOverlapResult> Overlaps;

				if (GetWorld()->OverlapMultiByObjectType(Overlaps,
				                                         Leaf.GetLocation() + GetActorLocation(),
				                                         Leaf.GetRotation(),
				                                         Pawns,
				                                         FCollisionShape::MakeBox(HalfExtent),
				                                         Query))
					break;

				MaxSafeAmount = Candidate.OpenAmount;
			}

			ECSSubsystem->AdvanceDoor(MazeEntity, Door.Index, DeltaSeconds, MaxSafeAmount);
		}

		TArray<uint8> States = ECSSubsystem->ReadDoorStates(MazeEntity);

		if (ReplicatedDoorStates.MazeRevision != Maze.Revision || States != ReplicatedDoorStates.States)
		{
			ReplicatedDoorStates.MazeRevision = Maze.Revision;
			ReplicatedDoorStates.States = MoveTemp(States);
			ForceNetUpdate();
		}
	}

	const TArray<FMazeDoorView> Doors = ECSSubsystem->ReadDoors(MazeEntity);

	if (DoorCollision->GetInstanceCount() != Doors.Num() || AppliedDoorOpenAmounts.Num() != Doors.Num() ||
	    AppliedDoorSwingSigns.Num() != Doors.Num())
	{
		RebuildDoorInstances();

		return;
	}

	const bool bVisual = GetNetMode() != NM_DedicatedServer;

	for (int32 ReaderIndex = 0; bVisual && ReaderIndex < ReaderDoorIndices.Num(); ++ReaderIndex)
	{
		const int32 DoorIndex = ReaderDoorIndices[ReaderIndex];

		if (Doors.IsValidIndex(DoorIndex) && AppliedDoorUnlocks.IsValidIndex(DoorIndex) &&
		    AppliedDoorUnlocks[DoorIndex] != Doors[DoorIndex].bUnlocked)
		{
			for (int32 Side = 0; Side < 2; ++Side)
				DoorScreens->SetCustomDataValue(
				    ReaderIndex * 2 + Side, 0, Doors[DoorIndex].bUnlocked ? 1.f : 0.f, false);

			AppliedDoorUnlocks[DoorIndex] = Doors[DoorIndex].bUnlocked;
		}
	}

	for (int32 Index = 0; Index < Doors.Num(); ++Index)
	{
		if (FMath::IsNearlyEqual(AppliedDoorOpenAmounts[Index], Doors[Index].OpenAmount) &&
		    AppliedDoorSwingSigns[Index] == Doors[Index].SwingSign)
			continue;

		const FTransform Leaf = LeafTransform(Doors[Index], GetActorLocation(), Width, Height, Maze.WallThickness);

		// UE tracks changed instance data and submits it at frame end without recreating the render proxy.
		DoorCollision->UpdateInstanceTransform(Index, Leaf, false, false, true);

		if (bVisual)
		{
			DoorLeaves->UpdateInstanceTransform(Index, Leaf, false, false, true);
			DoorHandles->UpdateInstanceTransform(
			    Index,
			    HandleTransform(Doors[Index], GetActorLocation(), Width, Maze.WallThickness),
			    false,
			    false,
			    true);
		}

		AppliedDoorOpenAmounts[Index] = Doors[Index].OpenAmount;
		AppliedDoorSwingSigns[Index] = Doors[Index].SwingSign;
	}
}
