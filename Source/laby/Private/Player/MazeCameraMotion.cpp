#include "Player/MazeCameraMotion.h"
#include "CoreGlobals.h"

namespace
{
	// Cosmetic tuning in centimetres, degrees and seconds; no locomotion rules here.
	struct FMazeCameraMotionDefinition
	{
		float FullWeightSpeed = 450.f;
		float MinimumSpeed = 5.f;
		float StrideLength = 300.f;
		float VerticalAmplitude = 0.85f;
		float SideAmplitude = 0.45f;
		float WalkRoll = 0.15f;
		float TurnRollPerDegreePerSecond = 0.004f;
		float LookPitchPerDegreePerSecond = 0.002f;
		float MaximumTurnRoll = 0.7f;
		float MaximumLookPitch = 0.35f;
		float BlendRate = 10.f;
		float MaximumFrameTime = 0.1f;
		float ViewCutDegrees = 45.f;
	};

	constexpr FMazeCameraMotionDefinition Definition;
}

void FMazeCameraMotion::Update(float DeltaSeconds, float GroundSpeed, const FRotator& ViewRotation)
{
	if (LastFrame == GFrameCounter)
		return;

	LastFrame = GFrameCounter;

	// Drop stale history after a hitch or camera cut, instead of producing a kick.
	const FRotator ViewDelta = (ViewRotation - PreviousView).GetNormalized();
	const bool bViewCut = bHasPreviousView && (FMath::Abs(ViewDelta.Yaw) > Definition.ViewCutDegrees ||
	                                           FMath::Abs(ViewDelta.Pitch) > Definition.ViewCutDegrees);

	if (DeltaSeconds <= 0.f || DeltaSeconds > Definition.MaximumFrameTime || bViewCut)
	{
		*this = FMazeCameraMotion();
		LastFrame = GFrameCounter;
		PreviousView = ViewRotation;

		return;
	}

	const float Blend = 1.f - FMath::Exp(-Definition.BlendRate * DeltaSeconds);
	const float Speed = GroundSpeed > Definition.MinimumSpeed ? GroundSpeed : 0.f;
	const float TargetWeight = FMath::Clamp(Speed / Definition.FullWeightSpeed, 0.f, 1.f);

	Weight = FMath::Lerp(Weight, TargetWeight, Blend);
	Phase = FMath::Fmod(Phase + Speed * DeltaSeconds * 2.f * PI / Definition.StrideLength, 2.f * PI);

	const float YawRate = bHasPreviousView ? ViewDelta.Yaw / DeltaSeconds : 0.f;
	const float PitchRate = bHasPreviousView ? ViewDelta.Pitch / DeltaSeconds : 0.f;
	const float TargetRoll = FMath::Clamp(
	    -YawRate * Definition.TurnRollPerDegreePerSecond, -Definition.MaximumTurnRoll, Definition.MaximumTurnRoll);
	const float TargetPitch = FMath::Clamp(
	    -PitchRate * Definition.LookPitchPerDegreePerSecond, -Definition.MaximumLookPitch, Definition.MaximumLookPitch);

	TurnRoll = FMath::Lerp(TurnRoll, TargetRoll, Blend);
	LookPitch = FMath::Lerp(LookPitch, TargetPitch, Blend);
	Offset = FVector(0.f,
	                 FMath::Sin(Phase) * Definition.SideAmplitude,
	                 FMath::Sin(2.f * Phase) * Definition.VerticalAmplitude) *
	         Weight;
	Rotation = FRotator(LookPitch, 0.f, TurnRoll + FMath::Sin(Phase) * Definition.WalkRoll * Weight);
	PreviousView = ViewRotation;
	bHasPreviousView = true;
}
