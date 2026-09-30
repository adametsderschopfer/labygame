#pragma once
#include "ECS/MazeVitals.h"

// Stateless ECS rules shared by batch updates and immediate jump/damage events.
struct FMazeVitalsSystem
{
	static bool IsAlive(const FMazeVitals& V)
	{
		return V.Health > 0.f;
	}

	static bool CanSprint(const FMazeVitals& V)
	{
		return IsAlive(V) && !V.bExhausted && V.Stamina > 0.f;
	}

	static bool CanJump(const FMazeVitals& V)
	{
		return IsAlive(V) && V.Stamina >= FMazeVitals::JumpCost;
	}

	static bool SpendJumpStamina(FMazeVitals& V)
	{
		if (!CanJump(V))
			return false;

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST

		if (V.bInfiniteStamina)
			return true;

#endif

		V.Stamina -= FMazeVitals::JumpCost;
		V.RecoveryWait = FMazeVitals::RecoveryDelay;

		if (V.Stamina <= 0.f)
			V.bExhausted = true;

		return true;
	}

	static bool SpendSignalStamina(FMazeVitals& V)
	{
		if (!IsAlive(V) || V.Stamina < FMazeVitals::SignalCost)
			return false;

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST

		if (V.bInfiniteStamina)
			return true;

#endif

		V.Stamina -= FMazeVitals::SignalCost;
		V.RecoveryWait = FMazeVitals::RecoveryDelay;

		if (V.Stamina <= 0.f)
			V.bExhausted = true;

		return true;
	}

	static void Update(FMazeVitals& V, float DeltaSeconds, bool bRunning, bool bOnGround = true)
	{
		if (!IsAlive(V) || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f)
			return;

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST

		if (V.bInfiniteStamina)
		{
			SetDevelopmentInfiniteStamina(V, true);

			return;
		}

#endif

		if (bOnGround && bRunning && CanSprint(V))
		{
			V.Stamina = FMath::Max(0.f, V.Stamina - FMazeVitals::DrainPerSecond * DeltaSeconds);
			V.RecoveryWait = FMazeVitals::RecoveryDelay;

			if (V.Stamina <= 0.f)
				V.bExhausted = true;
		}
		else
		{
			const float RecoveryTime = FMath::Max(0.f, DeltaSeconds - V.RecoveryWait);
			V.RecoveryWait = FMath::Max(0.f, V.RecoveryWait - DeltaSeconds);

			if (!bOnGround)
				return;

			V.Stamina =
			    FMath::Min(FMazeVitals::StaminaMaximum, V.Stamina + FMazeVitals::RecoveryPerSecond * RecoveryTime);

			if (V.Stamina >= FMazeVitals::ResumeThreshold)
				V.bExhausted = false;
		}
	}

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	static bool SetDevelopmentImmortal(FMazeVitals& V, bool bEnabled)
	{
		if (!IsAlive(V))
			return false;

		V.bDevelopmentImmortal = bEnabled;

		return true;
	}

	static bool ShouldDevelopmentRescue(const FMazeVitals& V)
	{
		return IsAlive(V) && V.bDevelopmentImmortal;
	}

	static void SetDevelopmentInfiniteStamina(FMazeVitals& V, bool bEnabled)
	{
		V.bInfiniteStamina = bEnabled;

		if (bEnabled)
		{
			V.Stamina = FMazeVitals::StaminaMaximum;
			V.RecoveryWait = 0.f;
			V.bExhausted = false;
		}
	}

#endif
	static float Damage(FMazeVitals& V, float Amount)
	{
		if (!FMath::IsFinite(Amount) || Amount <= 0.f || !IsAlive(V))
			return 0.f;

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST

		if (V.bDevelopmentImmortal)
			return 0.f;

#endif

		const float Applied = FMath::Min(V.Health, Amount);
		V.Health -= Applied;

		return Applied;
	}
};
