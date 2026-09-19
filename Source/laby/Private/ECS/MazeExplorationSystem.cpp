#include "ECS/MazeExplorationSystem.h"

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

		const uint8 Walls = Layout.Walls[Y * Layout.Size + X];

		if (FMath::Abs(NX - NY) < 1.e-6)
			return false;

		if (NX < NY)
		{
			if (Walls & (SX > 0 ? 2 : 8))
				return false;

			X += SX;
			NX += DX;
		}
		else
		{
			if (Walls & (SY > 0 ? 4 : 1))
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

	if (!bAlive || !Pose.bInputEnabled || Local.Z < 0 || Local.Z > Maze.WallHeight)
		return;

	const FVector2D Position(Local.X / Maze.Cell, Local.Y / Maze.Cell);
	const FVector2D Forward = FVector2D(Pose.Forward.X, Pose.Forward.Y).GetSafeNormal();

	if ((Position - Exploration.LastPosition).SizeSquared() < 0.0025 &&
	    FVector2D::DotProduct(Forward, Exploration.LastForward) > 0.999)
		return;

	Exploration.LastPosition = Position;
	Exploration.LastForward = Forward;

	const int32 CX = FMath::FloorToInt(Position.X), CY = FMath::FloorToInt(Position.Y);

	if (CX < 0 || CY < 0 || CX >= Layout.Size || CY >= Layout.Size)
		return;

	Exploration.Seen[CY * Layout.Size + CX] = 1;

	constexpr int32 Radius = 6;

	for (int32 Y = FMath::Max(0, CY - Radius); Y <= FMath::Min(Layout.Size - 1, CY + Radius); ++Y)
		for (int32 X = FMath::Max(0, CX - Radius); X <= FMath::Min(Layout.Size - 1, CX + Radius); ++X)
		{
			const int32 Index = Y * Layout.Size + X;
			const FVector2D Target(X + 0.5, Y + 0.5);
			const FVector2D Delta = Target - Position;

			if (Exploration.Seen[Index] || Delta.SizeSquared() > Radius * Radius ||
			    FVector2D::DotProduct(Delta.GetSafeNormal(), Forward) < 0.5)
				continue;

			if (Visible(Layout, Position, Target))
				Exploration.Seen[Index] = 1;
		}
}

void FMazeExplorationSystem::SetOpen(FMazeSessionFragment& Session, bool bOpen)
{
	Session.bMapOpen = bOpen && !Session.bMenuOpen && Session.bSessionStarted;
}
