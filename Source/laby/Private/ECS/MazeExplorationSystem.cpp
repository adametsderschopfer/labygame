#include "ECS/MazeExplorationSystem.h"
#include "ECS/MazeExplorationDefinition.h"

bool FMazeExplorationSystem::Visible(const FMazeLayout& Layout, FVector2D From, FVector2D To)
{
	int32 X = FMath::FloorToInt(From.X), Y = FMath::FloorToInt(From.Y);
	const int32 TX = FMath::FloorToInt(To.X), TY = FMath::FloorToInt(To.Y);
	const FVector2D D = To - From;
	const int32 SX = D.X >= 0 ? 1 : -1, SY = D.Y >= 0 ? 1 : -1;
	const double DX = FMath::Abs(D.X) > UE_SMALL_NUMBER ? 1.0 / FMath::Abs(D.X) : 1.e20;
	const double DY = FMath::Abs(D.Y) > UE_SMALL_NUMBER ? 1.0 / FMath::Abs(D.Y) : 1.e20;
	double NX = (SX > 0 ? X + 1 - From.X : From.X - X) * DX;
	double NY = (SY > 0 ? Y + 1 - From.Y : From.Y - Y) * DY;

	for (int32 I = 0; I < Layout.Size * 2; ++I)
	{
		if (X == TX && Y == TY)
			return true;

		if (X < 0 || Y < 0 || X >= Layout.Size || Y >= Layout.Size)
			return false;

		const int32 Cell = Y * Layout.Size + X;
		const uint8 Walls = Layout.Walls[Cell];

		if (FMath::Abs(NX - NY) < 1.e-6)
			return false;

		if (NX < NY)
		{
			if ((Walls & (SX > 0 ? 2 : 8)) || Layout.IsCrawlway(Cell, SX > 0 ? 1 : 3))
				return false;

			X += SX;
			NX += DX;
		}
		else
		{
			if ((Walls & (SY > 0 ? 4 : 1)) || Layout.IsCrawlway(Cell, SY > 0 ? 2 : 0))
				return false;

			Y += SY;
			NY += DY;
		}
	}

	return false;
}

void FMazeExplorationSystem::Update(FMazeExplorationFragment& Exploration,
                                    FMassEntityHandle Entity,
                                    const FMazeGenerationFragment& Maze,
                                    const FMazePlayerPoseFragment& Pose,
                                    bool bAlive)
{
	if (!Maze.Data || Maze.Cell <= 0)
		return;

	const auto& Layout = Maze.Data->Layout;

	if (Exploration.Maze != Entity || Exploration.Revision != Maze.Revision)
	{
		Exploration = FMazeExplorationFragment();
		Exploration.Maze = Entity;
		Exploration.Revision = Maze.Revision;
		Exploration.Seen.Init(0, Layout.Walls.Num());
	}

	const FVector Local = Pose.Location - Maze.Origin;

	if (!bAlive || !Pose.bInputEnabled)
		return;

	const FVector2D Position(Local.X / Maze.Cell, Local.Y / Maze.Cell);
	const int32 CX = FMath::FloorToInt(Position.X), CY = FMath::FloorToInt(Position.Y);

	if (CX < 0 || CY < 0 || CX >= Layout.Size || CY >= Layout.Size)
		return;

	float FloorHeight = 0.f, CeilingHeight = Maze.WallHeight;

	for (int32 RoomIndex = 0; RoomIndex < Layout.Rooms.Num(); ++RoomIndex)
		if (Layout.Rooms[RoomIndex].Contains(FIntPoint(CX, CY)))
		{
			FloorHeight = FMazeRoomDefinition::FloorHeight(Layout.RoomType(RoomIndex));
			CeilingHeight = FMazeRoomDefinition::CeilingHeight(Layout.RoomType(RoomIndex), Maze.WallHeight);
			break;
		}

	if (Local.Z < FloorHeight || Local.Z > CeilingHeight)
		return;

	// 2 means the player stood on this cell; 1 means it was only seen nearby.
	// Room labels can therefore appear on entry without revealing adjacent rooms.
	Exploration.Seen[CY * Layout.Size + CX] = 2;

	// Reveal nearby floor when entering a cell or moving far enough inside it.
	// A stationary camera turn never expands exploration; walls still occlude it.
	if (Exploration.Seen[CY * Layout.Size + CX] && (Position - Exploration.LastPosition).SizeSquared() <
	                                                   FMath::Square(FMazeExplorationDefinition::RefreshDistanceCells))
		return;

	Exploration.LastPosition = Position;

	constexpr int32 Radius = FMazeExplorationDefinition::RevealRadiusCells;

	for (int32 Y = FMath::Max(0, CY - Radius); Y <= FMath::Min(Layout.Size - 1, CY + Radius); ++Y)
		for (int32 X = FMath::Max(0, CX - Radius); X <= FMath::Min(Layout.Size - 1, CX + Radius); ++X)
		{
			const int32 Index = Y * Layout.Size + X;
			const FVector2D Target(X + 0.5, Y + 0.5);
			const FVector2D Delta = Target - Position;

			if (Exploration.Seen[Index] || Delta.SizeSquared() > Radius * Radius)
				continue;

			if (Visible(Layout, Position, Target))
				Exploration.Seen[Index] = 1;
		}
}

void FMazeExplorationSystem::SetOpen(FMazeSessionFragment& Session, bool bOpen)
{
	Session.bMapOpen = bOpen && !Session.bMenuOpen && Session.bSessionStarted;
}
