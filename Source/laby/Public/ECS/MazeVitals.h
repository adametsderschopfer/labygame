#pragma once
#include "CoreMinimal.h"
#include "MazeVitals.generated.h"

// Data-only snapshot stored in a Mass fragment. Rules live in FMazeVitalsSystem.
USTRUCT()
struct FMazeVitals
{
	GENERATED_BODY()
	static constexpr float HealthMaximum = 100.f;
	static constexpr float StaminaMaximum = 100.f;
	static constexpr float DrainPerSecond = 10.f;
	static constexpr float RecoveryPerSecond = 15.f;
	static constexpr float RecoveryDelay = 1.5f;
	static constexpr float ResumeThreshold = 25.f;
	static constexpr float JumpCost = 3.75f;
	static constexpr float SignalCost = 25.f;

	UPROPERTY()
	float Health = HealthMaximum;
	UPROPERTY()
	float Stamina = StaminaMaximum;
	UPROPERTY()
	float RecoveryWait = 0.f;
	UPROPERTY()
	bool bExhausted = false;
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	// Development-only state owned by this player's vitals fragment; never serialized.
	bool bInfiniteStamina = false;
	bool bDevelopmentImmortal = false;
#endif
};
