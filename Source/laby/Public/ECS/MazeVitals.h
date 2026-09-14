#pragma once
#include "CoreMinimal.h"
#include "MazeVitals.generated.h"

// Data-only snapshot stored in a Mass fragment. Rules live in FMazeVitalsSystem.
USTRUCT()
struct FMazeVitals
{
	GENERATED_BODY()
	static constexpr float Maximum = 100.f;
	static constexpr float DrainPerSecond = 20.f;
	static constexpr float RecoveryPerSecond = 15.f;
	static constexpr float RecoveryDelay = 1.5f;
	static constexpr float ResumeThreshold = 25.f;
	static constexpr float JumpCost = 15.f;

	UPROPERTY()
	float Health = Maximum;
	UPROPERTY()
	float Stamina = Maximum;
	UPROPERTY()
	float RecoveryWait = 0.f;
	UPROPERTY()
	bool bExhausted = false;
};
