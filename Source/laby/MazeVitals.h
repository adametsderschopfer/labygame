#pragma once
#include "CoreMinimal.h"
#include "MazeVitals.generated.h"

// Per-life state, reflected so Live Coding reinstancing preserves it safely.
USTRUCT()
struct FMazeVitals
{
	GENERATED_BODY()
	static constexpr float Maximum = 100.f;
	static constexpr float DrainPerSecond = 20.f;
	static constexpr float RecoveryPerSecond = 15.f;
	static constexpr float RecoveryDelay = 1.5f;
	static constexpr float ResumeThreshold = 25.f;

	UPROPERTY() float Health = Maximum;
	UPROPERTY() float Stamina = Maximum;
	UPROPERTY() float RecoveryWait = 0.f;
	UPROPERTY() bool bExhausted = false;

	bool IsAlive() const { return Health > 0.f; }
	bool CanSprint() const { return IsAlive() && !bExhausted && Stamina > 0.f; }
	void Update(float DeltaSeconds, bool bRunning)
	{
		if (!IsAlive() || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f) return;
		if (bRunning && CanSprint())
		{
			Stamina = FMath::Max(0.f, Stamina - DrainPerSecond * DeltaSeconds);
			RecoveryWait = RecoveryDelay;
			if (Stamina <= 0.f) bExhausted = true;
		}
		else
		{
			const float RecoveryTime = FMath::Max(0.f, DeltaSeconds - RecoveryWait);
			RecoveryWait = FMath::Max(0.f, RecoveryWait - DeltaSeconds);
			Stamina = FMath::Min(Maximum, Stamina + RecoveryPerSecond * RecoveryTime);
			if (Stamina >= ResumeThreshold) bExhausted = false;
		}
	}
	float Damage(float Amount)
	{
		if (!FMath::IsFinite(Amount) || Amount <= 0.f || !IsAlive()) return 0.f;
		const float Applied = FMath::Min(Health, Amount);
		Health -= Applied;
		return Applied;
	}
};
