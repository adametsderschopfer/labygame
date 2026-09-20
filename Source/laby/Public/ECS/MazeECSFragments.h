#pragma once
#include "CoreMinimal.h"
#include "Mass/EntityElementTypes.h"
#include "Mass/EntityHandle.h"
#include "MassEntityTypes.h"
#include "Mass/ExternalSubsystemTraits.h"
#include "ECS/MazeVitals.h"
#include "ECS/MazePlayerControlDefinition.h"
#include "Maze/MazeSurface.h"
#include "MazeECSFragments.generated.h"

USTRUCT()
struct FMazeVitalsFragment : public FMassFragment
{
	GENERATED_BODY()
	UPROPERTY()
	FMazeVitals Value;
};

USTRUCT()
struct FMazePlayerInputFragment : public FMassFragment
{
	GENERATED_BODY()
	UPROPERTY()
	float Forward = 0.f;
	UPROPERTY()
	float Right = 0.f;
	UPROPERTY()
	float Yaw = 0.f;
	UPROPERTY()
	float Pitch = 0.f;
	UPROPERTY()
	bool bSprintHeld = false;
	UPROPERTY()
	bool bCrouchHeld = false;
	UPROPERTY()
	bool bJumpHeld = false;
	UPROPERTY()
	bool bJumpPressed = false;
};

// Observations from Unreal physics/input focus, consumed by gameplay systems.
USTRUCT()
struct FMazePlayerPoseFragment : public FMassFragment
{
	GENERATED_BODY()
	UPROPERTY()
	FVector Location = FVector::ZeroVector;
	UPROPERTY()
	FVector Forward = FVector::ForwardVector;
	UPROPERTY()
	FVector Right = FVector::RightVector;
	UPROPERTY()
	FVector Velocity = FVector::ZeroVector;
	UPROPERTY()
	FVector Acceleration = FVector::ZeroVector;
	UPROPERTY()
	bool bOnGround = false;
	UPROPERTY()
	bool bInputEnabled = true;
	UPROPERTY()
	bool bCrouched = false;
	UPROPERTY()
	float Sensitivity = 1.f;
};

USTRUCT()
struct FMazeLocomotionFragment : public FMassFragment
{
	GENERATED_BODY()
	UPROPERTY()
	bool bRunning = false;
	UPROPERTY()
	bool bOnGround = false;
};

// Commands for the engine bridge; no gameplay decisions in the bridge.
USTRUCT()
struct FMazePlayerCommandFragment : public FMassFragment
{
	GENERATED_BODY()
	UPROPERTY()
	FVector Movement = FVector::ZeroVector;
	UPROPERTY()
	float Speed = FMazePlayerControlDefinition::WalkSpeed;
	UPROPERTY()
	float Yaw = 0.f;
	UPROPERTY()
	float Pitch = 0.f;
	UPROPERTY()
	bool bJumpHeld = false;
	UPROPERTY()
	bool bStartJump = false;
	UPROPERTY()
	bool bCrouch = false;
	UPROPERTY()
	bool bDead = false;
};

USTRUCT()
struct FMazeProgressFragment : public FMassFragment
{
	GENERATED_BODY()
	UPROPERTY()
	FMassEntityHandle Maze;
	UPROPERTY()
	int32 ReachedExit = 0;
	UPROPERTY()
	uint32 MazeRevision = 0;
};

// Immutable generated payload; the fragment owns its shared lifetime, never an Actor.
struct FMazeGeneratedData
{
	FMazeLayout Layout;
	TArray<FTransform> FloorTransforms;
	FTransform CeilingTransform;
	FVector Start = FVector::ZeroVector;
	TArray<FVector> PlayerStarts;
	TArray<FVector> ExitPositions;
	TArray<FRotator> ExitRotations;
};

USTRUCT()
struct FMazeGenerationFragment : public FMassFragment
{
	GENERATED_BODY()
	UPROPERTY()
	int32 Seed = 0;
	UPROPERTY()
	int32 Size = FMazeLayout::DefaultSize;
	UPROPERTY()
	float Cell = 462.5f;
	UPROPERTY()
	float WallThickness = 50.f;
	UPROPERTY()
	float WallHeight = 320.f;
	UPROPERTY()
	FVector Origin = FVector::ZeroVector;
	UPROPERTY()
	bool bNeedsGeneration = true;
	UPROPERTY()
	uint32 Revision = 0;
	TSharedPtr<const FMazeGeneratedData, ESPMode::ThreadSafe> Data;
};

template <> struct TMassFragmentTraits<FMazeGenerationFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

USTRUCT()
struct FMazeExplorationFragment : public FMassFragment
{
	GENERATED_BODY()
	FMassEntityHandle Maze;
	uint32 Revision = 0;
	TArray<uint8> Seen;
	FVector2D LastPosition = FVector2D(-1000, -1000);
	FVector2D LastForward = FVector2D::ZeroVector;
};

template <> struct TMassFragmentTraits<FMazeExplorationFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

USTRUCT()
struct FMazeSessionFragment : public FMassFragment
{
	GENERATED_BODY()
	UPROPERTY()
	FMassEntityHandle Maze;
	UPROPERTY()
	bool bMenuOpen = false;
	UPROPERTY()
	bool bSettingsOpen = false;
	UPROPERTY()
	bool bSessionStarted = false;
	UPROPERTY()
	bool bMinimapVisible = true;
	UPROPERTY()
	bool bMapOpen = false;
};

USTRUCT()
struct FMazeRoomMember
{
	GENERATED_BODY()
	UPROPERTY()
	int32 Id = INDEX_NONE;
	UPROPERTY()
	FString Name;
	UPROPERTY()
	int32 Slot = 0;
};

USTRUCT()
struct FMazeRoomFragment : public FMassFragment
{
	GENERATED_BODY()
	UPROPERTY()
	TArray<FMazeRoomMember> Members;
	UPROPERTY()
	int32 HostId = INDEX_NONE;
	UPROPERTY()
	bool bStarted = false;
	UPROPERTY()
	bool bActive = false;
};

template <> struct TMassFragmentTraits<FMazeRoomFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
