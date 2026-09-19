#pragma once

#include "CoreMinimal.h"

// Local presentation cache only; never feeds aim, movement, or gameplay observations.
struct FMazeCameraMotion
{
	void Update(float DeltaSeconds, float GroundSpeed, const FRotator& ViewRotation);
	FVector Offset = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;

private:
	float Phase = 0.f;
	float Weight = 0.f;
	float TurnRoll = 0.f;
	float LookPitch = 0.f;
	FRotator PreviousView = FRotator::ZeroRotator;
	bool bHasPreviousView = false;
	uint64 LastFrame = MAX_uint64;
};
