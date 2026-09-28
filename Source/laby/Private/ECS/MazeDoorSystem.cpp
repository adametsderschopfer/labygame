#include "ECS/MazeDoorSystem.h"

FVector FMazeDoorSystem::HandleLocation(const FMazeDoorView& Door, float OpeningWidth)
{
	const float LeafWidth = OpeningWidth - 2.f * FMazeDoorDefinition::FrameWidthCm;
	const FVector Hinge = Door.Center - Door.SlideAxis * (OpeningWidth * .5f - FMazeDoorDefinition::FrameWidthCm);
	const FQuat Swing(
	    FVector::UpVector,
	    FMath::DegreesToRadians(FMazeDoorDefinition::SwingDegrees * FMath::SmoothStep(0.f, 1.f, Door.OpenAmount)));

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
	const FMazeDoorView View{Door.Index, Door.Center, Door.SlideAxis, Door.Normal, Door.OpenAmount, Door.bWantsOpen};

	return CanFocus(View, Eye, Aim, OpeningWidth);
}

void FMazeDoorSystem::Toggle(FMazeDoorFragment& Door)
{
	Door.bWantsOpen = !Door.bWantsOpen;
}

void FMazeDoorSystem::Update(FMazeDoorFragment& Door, float DeltaSeconds)
{
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f)
		return;

	const float Target = Door.bWantsOpen ? 1.f : 0.f;
	const float Duration =
	    Door.bWantsOpen ? FMazeDoorDefinition::OpenDurationSeconds : FMazeDoorDefinition::CloseDurationSeconds;

	Door.OpenAmount = FMath::FInterpConstantTo(Door.OpenAmount, Target, DeltaSeconds, 1.f / Duration);
}
