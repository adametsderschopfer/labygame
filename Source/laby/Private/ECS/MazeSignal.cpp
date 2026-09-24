#include "ECS/MazeSignal.h"

#include "ECS/MazeECSFragments.h"

void FMazeSignalSystem::Update(FMazeSignalFragment& Signal, float DeltaSeconds, bool bAuthority)
{
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f)
		return;

	Signal.DisplayRemaining = FMath::Max(0.f, Signal.DisplayRemaining - DeltaSeconds);

	if (bAuthority)
		Signal.CooldownRemaining = FMath::Max(0.f, Signal.CooldownRemaining - DeltaSeconds);
}

bool FMazeSignalSystem::Request(FMazeSignalFragment& Signal,
                                const FVector& Location,
                                const FVector& Direction,
                                bool bAllowed)
{
	if (!bAllowed || Signal.CooldownRemaining > 0.f || Location.ContainsNaN() || Direction.ContainsNaN())
		return false;

	const FVector NormalizedDirection = Direction.GetSafeNormal();

	if (NormalizedDirection.IsNearlyZero())
		return false;

	++Signal.Value.Sequence;

	if (Signal.Value.Sequence == 0)
		++Signal.Value.Sequence;

	Signal.Value.Location = Location;
	Signal.Value.Direction = NormalizedDirection;
	Signal.CooldownRemaining = FMazeSignalDefinition::CooldownSeconds;
	Signal.DisplayRemaining = FMazeSignalDefinition::DisplaySeconds;

	return true;
}

bool FMazeSignalSystem::Receive(FMazeSignalFragment& Signal, const FMazeSignalSnapshot& Snapshot)
{
	if (Snapshot.Sequence == 0 || Snapshot.Sequence == Signal.Value.Sequence || Snapshot.Location.ContainsNaN() ||
	    Snapshot.Direction.ContainsNaN())
		return false;

	const FVector NormalizedDirection = Snapshot.Direction.GetSafeNormal();

	if (NormalizedDirection.IsNearlyZero())
		return false;

	Signal.Value = Snapshot;
	Signal.Value.Direction = NormalizedDirection;
	Signal.DisplayRemaining = FMazeSignalDefinition::DisplaySeconds;

	return true;
}
