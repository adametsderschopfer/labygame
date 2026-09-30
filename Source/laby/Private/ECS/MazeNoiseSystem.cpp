#include "ECS/MazeNoiseSystem.h"
#include "ECS/MazeECSFragments.h"
#include "ECS/MazePlayerControlDefinition.h"

namespace
{
	void ResetStride(FMazeNoiseFragment& Noise)
	{
		Noise.StrideDistance = 0;
		Noise.bHasPreviousLocation = false;
		Noise.bFirstStep = true;
	}

	void AdvanceSequence(uint32& Sequence)
	{
		if (++Sequence == 0)
			++Sequence;
	}
}

void FMazeNoiseSystem::Synchronize(FMazeNoiseFragment& Noise,
                                   FMassEntityHandle Maze,
                                   const FMazeGenerationFragment& Generation)
{
	if (Noise.Maze == Maze && Noise.Value.MazeRevision == Generation.Revision &&
	    Noise.Value.MazeSeed == Generation.Seed)
		return;

	Noise = FMazeNoiseFragment();
	Noise.Maze = Maze;
	Noise.Value.MazeRevision = Generation.Revision;
	Noise.Value.MazeSeed = Generation.Seed;
}

void FMazeNoiseSystem::Emit(FMazeNoiseFragment& Noise, EMazeNoiseSource Source)
{
	float Strength = 0;

	switch (Source)
	{
	case EMazeNoiseSource::Whistle:
		Strength = FMazeNoiseDefinition::WhistleStrength;
		break;
	case EMazeNoiseSource::Headlamp:
		Strength = FMazeNoiseDefinition::HeadlampStrength;
		break;
	case EMazeNoiseSource::Door:
		Strength = FMazeNoiseDefinition::DoorStrength;
		break;
	case EMazeNoiseSource::Footstep:
		Strength = Noise.Value.StepType == EMazeFootstepType::Crouch ? FMazeNoiseDefinition::CrouchStrength
		           : Noise.Value.StepType == EMazeFootstepType::Run  ? FMazeNoiseDefinition::RunStrength
		                                                             : FMazeNoiseDefinition::WalkStrength;
		break;
	default:
		return;
	}

	Noise.Value.Coefficient = Source == EMazeNoiseSource::Headlamp || Source == EMazeNoiseSource::Door
	                              ? FMath::Min(1.f, Noise.Value.Coefficient + Strength)
	                              : FMath::Max(Noise.Value.Coefficient, Strength);
	Noise.Value.LastSource = Source;
	AdvanceSequence(Noise.Value.EventSequence);
}

void FMazeNoiseSystem::Update(FMazeNoiseFragment& Noise,
                              const FMazePlayerPoseFragment& Pose,
                              float DeltaSeconds,
                              bool bAlive)
{
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0)
		return;

	if (!bAlive)
	{
		Noise.Value.Coefficient = 0;
		Noise.Value.Footprints.Reset();
		Noise.bHasObservation = false;
		ResetStride(Noise);

		return;
	}

	Noise.Value.Coefficient =
	    FMath::Max(0.f, Noise.Value.Coefficient - FMazeNoiseDefinition::DecayPerSecond * DeltaSeconds);

	for (auto& Mark : Noise.Value.Footprints)
		Mark.RemainingSeconds = FMath::Max(0.f, Mark.RemainingSeconds - DeltaSeconds);

	Noise.Value.Footprints.RemoveAll(
	    [](const FMazeNoiseFootprint& Mark)
	    {
		    return Mark.RemainingSeconds <= 0;
	    });

	const float Speed = Pose.Velocity.Size2D();

	if (Pose.Location.ContainsNaN() || !FMath::IsFinite(Speed))
	{
		Noise.bHasObservation = false;
		ResetStride(Noise);

		return;
	}

	const float Distance = Noise.bHasObservation ? FVector::Dist2D(Pose.Location, Noise.PreviousLocation) : 0.f;

	Noise.PreviousLocation = Pose.Location;
	Noise.bHasObservation = true;

	// Reject teleports/corrections rather than leaving a trail across the discontinuity.
	if (!FMath::IsFinite(Distance) || Distance > Speed * DeltaSeconds * 2.f + 10.f)
	{
		Noise.Value.Footprints.Reset();
		ResetStride(Noise);

		return;
	}

	if (!Pose.bInputEnabled || !Pose.bOnGround || Speed < FMazeNoiseDefinition::MinimumSpeed ||
	    DeltaSeconds > FMazeNoiseDefinition::MaximumStepDelta)
	{
		ResetStride(Noise);

		return;
	}

	if (!Noise.bHasPreviousLocation)
	{
		Noise.bHasPreviousLocation = true;

		return;
	}

	const auto Type =
	    Pose.bCrouched ? EMazeFootstepType::Crouch
	    : Speed > (FMazePlayerControlDefinition::WalkSpeed + FMazePlayerControlDefinition::SprintSpeed) * 0.5f
	        ? EMazeFootstepType::Run
	        : EMazeFootstepType::Walk;
	const float Stride = Noise.bFirstStep                    ? FMazeNoiseDefinition::FirstStride
	                     : Type == EMazeFootstepType::Crouch ? FMazeNoiseDefinition::CrouchStride
	                     : Type == EMazeFootstepType::Run    ? FMazeNoiseDefinition::RunStride
	                                                         : FMazeNoiseDefinition::WalkStride;

	Noise.StrideDistance += Distance;

	if (Noise.StrideDistance < Stride)
		return;

	Noise.StrideDistance = 0;
	Noise.bFirstStep = false;
	Noise.Value.StepType = Type;
	AdvanceSequence(Noise.Value.StepSequence);
	Emit(Noise, EMazeNoiseSource::Footstep);

	if (Noise.Value.Footprints.Num() == FMazeNoiseDefinition::TrailCount)
		Noise.Value.Footprints.RemoveAt(0);

	auto& Mark = Noise.Value.Footprints.AddDefaulted_GetRef();

	Mark.Location = Pose.Location;
	Mark.RemainingSeconds = FMazeNoiseDefinition::TrailSeconds;
	Mark.Strength = Type == EMazeFootstepType::Crouch ? FMazeNoiseDefinition::CrouchStrength
	                : Type == EMazeFootstepType::Run  ? FMazeNoiseDefinition::RunStrength
	                                                  : FMazeNoiseDefinition::WalkStrength;
}

FMazeNoiseSnapshot FMazeNoiseSystem::Snapshot(const FMazeNoiseFragment& Noise, bool bForReplication)
{
	FMazeNoiseSnapshot Result = Noise.Value;

	for (auto& Mark : Result.Footprints)
		Mark.RemainingSeconds = bForReplication ? FMath::RoundToFloat(Mark.RemainingSeconds * 10.f) / 10.f
		                                        : FMath::Max(0.f, Mark.RemainingSeconds - Noise.PresentationElapsed);

	if (bForReplication)
		Result.Coefficient = FMath::RoundToFloat(Result.Coefficient * 20.f) / 20.f;

	return Result;
}

bool FMazeNoiseSystem::Receive(FMazeNoiseFragment& Noise, const FMazeNoiseSnapshot& Snapshot)
{
	if (!FMath::IsFinite(Snapshot.Coefficient) || Snapshot.Coefficient < 0 || Snapshot.Coefficient > 1 ||
	    Snapshot.Footprints.Num() > FMazeNoiseDefinition::TrailCount ||
	    uint8(Snapshot.LastSource) > uint8(EMazeNoiseSource::Door) ||
	    uint8(Snapshot.StepType) > uint8(EMazeFootstepType::Crouch))
		return false;

	for (const auto& Mark : Snapshot.Footprints)
		if (Mark.Location.ContainsNaN() || !FMath::IsFinite(Mark.RemainingSeconds) || Mark.RemainingSeconds < 0 ||
		    Mark.RemainingSeconds > FMazeNoiseDefinition::TrailSeconds || !FMath::IsFinite(Mark.Strength) ||
		    Mark.Strength < 0 || Mark.Strength > 1)
			return false;

	Noise.Value = Snapshot;
	Noise.PresentationElapsed = 0;

	return true;
}
