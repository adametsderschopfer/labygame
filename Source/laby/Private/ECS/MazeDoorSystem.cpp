#include "ECS/MazeDoorSystem.h"

void FMazeDoorSystem::Update(FMazeDoorFragment& Door,
                             float DeltaSeconds,
                             TConstArrayView<FVector> PlayerLocations,
                             bool bAuthority)
{
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f)
		return;

	if (bAuthority)
	{
		const float Radius =
		    Door.bWantsOpen ? FMazeDoorDefinition::KeepOpenRadiusCm : FMazeDoorDefinition::OpenRadiusCm;
		const float RadiusSquared = FMath::Square(Radius);
		bool bPlayerNearby = false;

		for (const FVector& PlayerLocation : PlayerLocations)
		{
			const FVector Delta = PlayerLocation - Door.Center;

			if (FMath::Abs(Delta.Z) <= FMazeDoorDefinition::VerticalRangeCm && Delta.SizeSquared2D() <= RadiusSquared)
			{
				bPlayerNearby = true;
				break;
			}
		}

		if (bPlayerNearby)
		{
			Door.bWantsOpen = true;
			Door.CloseDelayRemaining = FMazeDoorDefinition::CloseDelaySeconds;
		}
		else if (Door.bWantsOpen)
		{
			Door.CloseDelayRemaining = FMath::Max(0.f, Door.CloseDelayRemaining - DeltaSeconds);

			if (Door.CloseDelayRemaining <= 0.f)
				Door.bWantsOpen = false;
		}
	}

	const float Target = Door.bWantsOpen ? 1.f : 0.f;
	const float Duration =
	    Door.bWantsOpen ? FMazeDoorDefinition::OpenDurationSeconds : FMazeDoorDefinition::CloseDurationSeconds;

	Door.OpenAmount = FMath::FInterpConstantTo(Door.OpenAmount, Target, DeltaSeconds, 1.f / Duration);
}
