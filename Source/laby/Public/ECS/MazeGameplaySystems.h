#pragma once
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ECS/MazeECSFragments.h"
#include "ECS/MazeVitalsSystem.h"
#include "ECS/MazePlayerControlDefinition.h"
#include "ECS/MazeWaterDefinition.h"

struct FMazeGenerationSystem
{
	static void Generate(FMazeGenerationFragment& Maze)
	{
		if (!Maze.bNeedsGeneration)
			return;

		auto Data = MakeShared<FMazeGeneratedData, ESPMode::ThreadSafe>();
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Maze_GenerateTopology);
			Data->Layout.Generate(Maze.Seed, Maze.Size);
		}
		const float Span = Data->Layout.Size * Maze.Cell;
		auto FloorRect = [&Data](float X, float Y, float Width, float Height)
		{
			Data->FloorTransforms.Add(FTransform(FRotator::ZeroRotator,
			                                     FVector(X + Width / 2, Y + Height / 2, -25),
			                                     FVector(Width / 100, Height / 100, 0.5f)));
		};
		auto CeilingRect = [&Data, &Maze](float X, float Y, float Width, float Height)
		{
			Data->CeilingTransforms.Add(
			    FTransform(FRotator::ZeroRotator,
			               FVector(X + Width / 2, Y + Height / 2, Maze.WallHeight + Maze.WallThickness / 2),
			               FVector(Width / 100, Height / 100, Maze.WallThickness / 100)));
		};
		TArray<int32> RoomIndices;

		RoomIndices.Init(INDEX_NONE, Data->Layout.Walls.Num());

		for (int32 RoomIndex = 0; RoomIndex < Data->Layout.Rooms.Num(); ++RoomIndex)
			for (int32 Y = Data->Layout.Rooms[RoomIndex].Min.Y; Y < Data->Layout.Rooms[RoomIndex].Max.Y; ++Y)
				for (int32 X = Data->Layout.Rooms[RoomIndex].Min.X; X < Data->Layout.Rooms[RoomIndex].Max.X; ++X)
					RoomIndices[Y * Data->Layout.Size + X] = RoomIndex;

		const auto IsSpecialRoomCell = [&Data, &RoomIndices](int32 CellIndex)
		{
			return RoomIndices.IsValidIndex(CellIndex) && RoomIndices[CellIndex] != INDEX_NONE &&
			       Data->Layout.RoomType(RoomIndices[CellIndex]) != EMazeRoomType::Empty;
		};

		// Merge intact row runs, leaving holes and lowered rooms for their dedicated geometry.
		for (int32 Y = 0; Y < Data->Layout.Size; ++Y)
		{
			int32 X = 0;

			while (X < Data->Layout.Size)
			{
				if (!Data->Layout.HasFloor(Y * Data->Layout.Size + X) || IsSpecialRoomCell(Y * Data->Layout.Size + X))
				{
					++X;
					continue;
				}

				const int32 Begin = X;

				while (X < Data->Layout.Size && Data->Layout.HasFloor(Y * Data->Layout.Size + X) &&
				       !IsSpecialRoomCell(Y * Data->Layout.Size + X))
					++X;

				FloorRect(Begin * Maze.Cell, Y * Maze.Cell, (X - Begin) * Maze.Cell, Maze.Cell);
			}
		}

		// The ordinary ceiling is likewise cut around tall rooms; their raised slabs
		// and perimeter extensions are deterministic room geometry.
		for (int32 Y = 0; Y < Data->Layout.Size; ++Y)
		{
			int32 X = 0;

			while (X < Data->Layout.Size)
			{
				if (IsSpecialRoomCell(Y * Data->Layout.Size + X))
				{
					++X;
					continue;
				}

				const int32 Begin = X;

				while (X < Data->Layout.Size && !IsSpecialRoomCell(Y * Data->Layout.Size + X))
					++X;

				const float HalfWall = Maze.WallThickness * 0.5f;
				const float X0 = Begin * Maze.Cell - (Begin == 0 ? HalfWall : 0.f);
				const float X1 = X * Maze.Cell + (X == Data->Layout.Size ? HalfWall : 0.f);
				const float Y0 = Y * Maze.Cell - (Y == 0 ? HalfWall : 0.f);
				const float Y1 = (Y + 1) * Maze.Cell + (Y + 1 == Data->Layout.Size ? HalfWall : 0.f);
				CeilingRect(X0, Y0, X1 - X0, Y1 - Y0);
			}
		}

		Data->RoomGeometry = FMazeRoomGeometry::Build(Data->Layout, Maze.Cell, Maze.WallThickness, Maze.WallHeight);

		// Preserve the exterior landing beyond the exit, without bridging any holes.
		constexpr float Apron = 1200.f;
		FloorRect(-Apron, -Apron, Span + 2 * Apron, Apron);
		FloorRect(-Apron, Span, Span + 2 * Apron, Apron);
		FloorRect(-Apron, 0, Apron, Span);
		FloorRect(Span, 0, Apron, Span);
		const int32 StartCell = Data->Layout.Start();
		Data->Start = FVector((StartCell % Data->Layout.Size + 0.5f) * Maze.Cell,
		                      (StartCell / Data->Layout.Size + 0.5f) * Maze.Cell,
		                      100.f);
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
	                                          FMazeLocomotionFragment& Locomotion,
	                                          bool bWading)
	{
		FMazePlayerCommandFragment Command;
		Command.bDead = !FMazeVitalsSystem::IsAlive(Vitals);

		if (!Pose.bInputEnabled || Command.bDead)
			Input = FMazePlayerInputFragment();

		Command.bCrouch = Input.bCrouchHeld;
		const bool bLowStance = Command.bCrouch || Pose.bCrouched;
		const bool bSprint =
		    !bWading && !bLowStance && Pose.bInputEnabled && Input.bSprintHeld && FMazeVitalsSystem::CanSprint(Vitals);
		Command.Speed = bWading
		                    ? (bLowStance ? FMazeWaterDefinition::CrouchedWadeSpeed : FMazeWaterDefinition::WadeSpeed)
		                : bLowStance ? FMazePlayerControlDefinition::CrouchSpeed
		                : bSprint    ? FMazePlayerControlDefinition::SprintSpeed
		                             : FMazePlayerControlDefinition::WalkSpeed;

		if (!Command.bDead && Pose.bInputEnabled)
		{
			Command.Movement = (Pose.Forward * Input.Forward + Pose.Right * Input.Right).GetClampedToMaxSize(1.f);
			Command.bJumpHeld = Input.bJumpHeld && !bLowStance;
			Command.bStartJump = Input.bJumpPressed && !bLowStance;
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
