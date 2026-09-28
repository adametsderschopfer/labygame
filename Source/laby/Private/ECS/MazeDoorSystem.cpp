#include "ECS/MazeDoorSystem.h"

FVector FMazeDoorSystem::HandleLocation(const FMazeDoorView& Door, float OpeningWidth)
{
	const float LeafWidth = OpeningWidth - 2.f * FMazeDoorDefinition::FrameWidthCm;
	const FVector Hinge = Door.Center - Door.SlideAxis * (OpeningWidth * .5f - FMazeDoorDefinition::FrameWidthCm);
	const FQuat Swing(FVector::UpVector,
	                  FMath::DegreesToRadians(Door.SwingSign * FMazeDoorDefinition::SwingDegrees *
	                                          FMath::SmoothStep(0.f, 1.f, Door.OpenAmount)));

	return Hinge + Swing.RotateVector(Door.SlideAxis * (LeafWidth - FMazeDoorDefinition::HandleInsetCm)) +
	       FVector::UpVector * FMazeDoorDefinition::HandleHeightCm;
}

bool FMazeDoorSystem::CanFocus(const FMazeDoorView& Door, const FVector& Eye, const FVector& Aim, float OpeningWidth)
{
	const FVector Delta = HandleLocation(Door, OpeningWidth) - Eye;
	const float Distance = Delta.Size();

	return !Eye.ContainsNaN() && !Aim.ContainsNaN() && Distance <= FMazeDoorDefinition::InteractionRangeCm &&
	       Distance > UE_SMALL_NUMBER &&
	       FVector::CrossProduct(Delta, Aim.GetSafeNormal()).Size() <= FMazeDoorDefinition::AimRadiusCm &&
	       FVector::DotProduct(Delta, Aim) > 0.f;
}

bool FMazeDoorSystem::CanInteract(const FMazeDoorFragment& Door,
                                  const FVector& Eye,
                                  const FVector& Aim,
                                  float OpeningWidth)
{
	const FMazeDoorView View{
	    Door.Index, Door.Center, Door.SlideAxis, Door.Normal, Door.OpenAmount, Door.bWantsOpen, Door.SwingSign};

	return CanFocus(View, Eye, Aim, OpeningWidth);
}

void FMazeDoorSystem::Toggle(FMazeDoorFragment& Door, const FVector& PlayerLocation)
{
	if (Door.PendingSwingSign != 0)
	{
		Door.PendingSwingSign = 0;

		return;
	}

	if (Door.bWantsOpen)
	{
		Door.bWantsOpen = false;

		return;
	}

	const float PlayerSide = FVector::DotProduct(PlayerLocation - Door.Center, Door.Normal);
	const float PositiveSwingSide = FVector::CrossProduct(FVector::UpVector, Door.SlideAxis).Dot(Door.Normal);
	const int8 AwaySign = PlayerSide * PositiveSwingSide > 0.f ? -1 : 1;

	if (Door.OpenAmount > UE_KINDA_SMALL_NUMBER && Door.SwingSign != AwaySign)
	{
		Door.PendingSwingSign = AwaySign;
		Door.bWantsOpen = false;

		return;
	}

	Door.SwingSign = AwaySign;
	Door.bWantsOpen = true;
}

float FMazeDoorSystem::NextOpenAmount(const FMazeDoorView& Door, float DeltaSeconds)
{
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f)
		return Door.OpenAmount;

	const float Target = Door.bWantsOpen ? 1.f : 0.f;
	const float Duration =
	    Door.bWantsOpen ? FMazeDoorDefinition::OpenDurationSeconds : FMazeDoorDefinition::CloseDurationSeconds;

	return FMath::FInterpConstantTo(Door.OpenAmount, Target, DeltaSeconds, 1.f / Duration);
}

void FMazeDoorSystem::Advance(FMazeDoorFragment& Door, float DeltaSeconds, float MaxSafeAmount)
{
	if (!FMath::IsFinite(MaxSafeAmount))
		return;

	const FMazeDoorView View{
	    Door.Index, Door.Center, Door.SlideAxis, Door.Normal, Door.OpenAmount, Door.bWantsOpen, Door.SwingSign};
	const float Desired = NextOpenAmount(View, DeltaSeconds);

	if (Desired > Door.OpenAmount)
		Door.OpenAmount = FMath::Clamp(MaxSafeAmount, Door.OpenAmount, Desired);
	else
		Door.OpenAmount = FMath::Clamp(MaxSafeAmount, Desired, Door.OpenAmount);

	if (Door.PendingSwingSign != 0 && Door.OpenAmount <= UE_KINDA_SMALL_NUMBER)
	{
		Door.OpenAmount = 0.f;
		Door.SwingSign = Door.PendingSwingSign;
		Door.PendingSwingSign = 0;
		Door.bWantsOpen = true;
	}
}
