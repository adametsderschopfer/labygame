#pragma once
#include "ECS/MazeECSFragments.h"
#include "ECS/MazeVitalsSystem.h"

struct FMazeGenerationSystem
{
	static void Generate(FMazeGenerationFragment& Maze)
	{
		if (!Maze.bNeedsGeneration)
			return;

		auto Data = MakeShared<FMazeGeneratedData>();
		Data->Layout.Generate(Maze.Seed, Maze.Size);
		Data->Surface.Build(Data->Layout, Maze.Cell, Maze.WallThickness, Maze.WallHeight);
		const float Span = Data->Layout.Size * Maze.Cell;
		auto FloorRect = [&Data](float X, float Y, float Width, float Height)
		{
			Data->FloorTransforms.Add(FTransform(FRotator::ZeroRotator,
			                                     FVector(X + Width / 2, Y + Height / 2, -25),
			                                     FVector(Width / 100, Height / 100, 0.5f)));
		};

		// Merge intact row runs, leaving actual gaps in both rendering and collision.
		for (int32 Y = 0; Y < Data->Layout.Size; ++Y)
		{
			int32 X = 0;

			while (X < Data->Layout.Size)
			{
				if (!Data->Layout.HasFloor(Y * Data->Layout.Size + X))
				{
					++X;
					continue;
				}

				const int32 Begin = X;

				while (X < Data->Layout.Size && Data->Layout.HasFloor(Y * Data->Layout.Size + X))
					++X;

				FloorRect(Begin * Maze.Cell, Y * Maze.Cell, (X - Begin) * Maze.Cell, Maze.Cell);
			}
		}

		// Preserve the exterior landing beyond the exit, without bridging any holes.
		constexpr float Apron = 1200.f;
		FloorRect(-Apron, -Apron, Span + 2 * Apron, Apron);
		FloorRect(-Apron, Span, Span + 2 * Apron, Apron);
		FloorRect(-Apron, 0, Apron, Span);
		FloorRect(Span, 0, Apron, Span);
		Data->Start =
		    FVector((Data->Layout.Size / 2 + 0.5f) * Maze.Cell, (Data->Layout.Size / 2 + 0.5f) * Maze.Cell, 100.f);
		const FVector Outward[] = {FVector(0, -1, 0), FVector(1, 0, 0), FVector(0, 1, 0)};

		for (int32 Slot = 0; Slot < 4; ++Slot)
			Data->PlayerStarts.Add(Data->Start + FVector(Slot % 2 ? 110 : -110, Slot / 2 ? 110 : -110, 0));

		for (int32 I = 0; I < Data->Layout.Exits.Num(); ++I)
		{
			const int32 C = Data->Layout.Exits[I];
			Data->ExitPositions.Add(
			    FVector((C % Data->Layout.Size + 0.5f) * Maze.Cell, (C / Data->Layout.Size + 0.5f) * Maze.Cell, 300.f) +
			    Outward[I] * (Maze.Cell / 2 + 50));
			Data->ExitRotations.Add((-Outward[I]).Rotation());
		}

		Maze.Data = Data;
		++Maze.Revision;
		Maze.bNeedsGeneration = false;
	}

	static int32 ExitAt(const FMazeGenerationFragment& Maze, const FVector& Location)
	{
		if (!Maze.Data)
			return 0;

		const auto& Layout = Maze.Data->Layout;
		const FVector P = Location - Maze.Origin;

		if (P.Z < 0 || P.Z > 300.f)
			return 0;

		const float Span = Layout.Size * Maze.Cell;

		for (int32 I = 0; I < Layout.Exits.Num(); ++I)
		{
			const int32 C = Layout.Exits[I];
			const float X = (C % Layout.Size + 0.5f) * Maze.Cell, Y = (C / Layout.Size + 0.5f) * Maze.Cell;
			const float HalfOpening = (Maze.Cell - Maze.WallThickness) / 2;

			if ((I == 0 && P.Y < -50 && FMath::Abs(P.X - X) < HalfOpening) ||
			    (I == 1 && P.X > Span + 50 && FMath::Abs(P.Y - Y) < HalfOpening) ||
			    (I == 2 && P.Y > Span + 50 && FMath::Abs(P.X - X) < HalfOpening))
				return I + 1;
		}

		return 0;
	}
};

struct FMazeRoomSystem
{
	static FString AdmissionError(const FMazeRoomFragment& Room)
	{
		if (!Room.bActive || Room.bStarted)
			return TEXT("Room is closed");

		if (Room.Members.Num() >= 4)
			return TEXT("Room is full (4 players)");

		return FString();
	}

	static bool Add(FMazeRoomFragment& Room, int32 Id, const FString& Name, bool bHost)
	{
		if (Room.bStarted || Room.Members.Num() >= 4 ||
		    Room.Members.ContainsByPredicate(
		        [Id](const auto& M)
		        {
			        return M.Id == Id;
		        }))
			return false;

		int32 Slot = 0;

		while (Room.Members.ContainsByPredicate(
		    [Slot](const auto& M)
		    {
			    return M.Slot == Slot;
		    }))
			++Slot;

		FMazeRoomMember Member;
		Member.Id = Id;
		Member.Name = Name.Left(48);
		Member.Slot = Slot;
		Room.Members.Add(Member);

		if (bHost)
			Room.HostId = Id;

		return true;
	}

	static bool Start(FMazeRoomFragment& Room, int32 Requester)
	{
		if (!Room.bActive || Room.bStarted || Requester != Room.HostId || Room.Members.IsEmpty())
			return false;

		Room.bStarted = true;

		return true;
	}
};

struct FMazeHazardSystem
{
	static void Apply(const FMazeGenerationFragment& Maze, const FMazePlayerPoseFragment& Pose, FMazeVitals& Vitals)
	{
		if (Maze.Data && Pose.Location.Z < Maze.Origin.Z - 500.f)
			FMazeVitalsSystem::Damage(Vitals, Vitals.Health);
	}
};

struct FMazePlayerControlSystem
{
	static FMazePlayerCommandFragment Resolve(FMazePlayerInputFragment& Input,
	                                          const FMazePlayerPoseFragment& Pose,
	                                          const FMazeVitals& Vitals,
	                                          FMazeLocomotionFragment& Locomotion)
	{
		FMazePlayerCommandFragment Command;
		Command.bDead = !FMazeVitalsSystem::IsAlive(Vitals);

		if (!Pose.bInputEnabled)
			Input = FMazePlayerInputFragment();

		const bool bSprint = Pose.bInputEnabled && Input.bSprintHeld && FMazeVitalsSystem::CanSprint(Vitals);
		Command.Speed = bSprint ? 750.f : 450.f;

		if (!Command.bDead && Pose.bInputEnabled)
		{
			Command.Movement = (Pose.Forward * Input.Forward + Pose.Right * Input.Right).GetClampedToMaxSize(1.f);
			Command.bJumpHeld = Input.bJumpHeld;
			Command.bStartJump = Input.bJumpPressed;
		}

		Command.Yaw = Input.Yaw * Pose.Sensitivity;
		Command.Pitch = Input.Pitch * Pose.Sensitivity;
		Input.Yaw = Input.Pitch = 0.f;
		Input.bJumpPressed = false;
		Locomotion.bOnGround = Pose.bOnGround;
		Locomotion.bRunning =
		    bSprint && Pose.bOnGround && Pose.Velocity.SizeSquared2D() > 1.f && !Pose.Acceleration.IsNearlyZero();

		return Command;
	}
};
